/* IBE library routines, see ibe.h for usage
 * Ben Lynn
 */

/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "curve.h"
#include "version.h"
#include "benchmark.h"
#include "ibe.h"
#include "crypto.h"
#include "mm.h"

void preprocessed_key_init(preprocessed_key_t pk, params_t params)
{
    miller_cache_init(pk->mc, params->curve);
}

void preprocessed_key_clear(preprocessed_key_t pk)
{
    miller_cache_clear(pk->mc);
}

int mympz_sizeinbytes(mpz_t x)
{
    int i, n;
    n = mpz_sizeinbase(x, 2);
    i = n >> 3;
    if (n % 8) i++;

    return i;
}

void mympz_out_raw(mpz_t x, unsigned char *c, int n)
{
    unsigned long l;
    mpz_t z;
    int i = n - 1;

    mpz_init_set(z, x);

    while(i >= 0) {
	l = mpz_get_ui(z);
	c[i] = (unsigned char) l;
	mpz_tdiv_q_2exp(z, z, 8);
	i--;
    }
    mpz_clear(z);
}

void mympz_inp_raw(mpz_t z, const unsigned char* c, int n)
{
    int i = 0;

    mpz_set_ui(z, 0);
    while(i < n) {
	mpz_mul_2exp(z, z, 8);
	mpz_add_ui(z, z, c[i]);
	i++;
    }
}

static void mympz_from_hash(mpz_t x, mpz_t limit, byte_string_t hash)
//let z = number represented in c
//x = z || 1 || z || 2 || z || 3 ...
//until there are enough bits
//should be rewritten more efficiently?
{
    mpz_t z;
    int countlen;
    mpz_t count;
    int i = 0, j;
    int bits = mpz_sizeinbase(limit, 2);
    int zbits;
    char *c = hash->data;
    int len = hash->len;

    mpz_init(z);
    mpz_init(count);
    mpz_set_ui(count, 1);

    mympz_inp_raw(z, c, len);
    zbits = mpz_sizeinbase(z, 2);

    mpz_set_ui(x, 0);
    do {
	for (j=0; j<zbits; j++) {
	    if (mpz_tstbit(z, j)) {
		mpz_setbit(x, i);
	    }
	    i++;
	}
	bits -= zbits;
	if (bits <= 0) break;

	countlen = mpz_sizeinbase(count, 2);
	for (j=0; j<countlen; j++) {
	    if (mpz_tstbit(count, j)) {
		mpz_setbit(x, i);
	    }
	    i++;
	}
	bits -= countlen;
	mpz_add_ui(count, count, 1);
    } while (bits > 0);
    /*
    printf("hash: ");
    mpz_out_str(NULL, 0, x);
    printf("\n");
    */
    while (mpz_cmp(x, limit) > 0) {
	mpz_clrbit(x, mpz_sizeinbase(x, 2) - 1);
    }

    mpz_clear(z);
    mpz_clear(count);
}

void byte_string_set_mpz(byte_string_t bs, mpz_t x)
{
    int i = mympz_sizeinbytes(x);

    byte_string_init(bs, i);
    mympz_out_raw(x, bs->data, i);
}

void byte_string_set_fp2(byte_string_t bs, fp2_t x)
{
    byte_string_t bs1, bs2;
    byte_string_set_mpz(bs1, x->a);
    byte_string_set_mpz(bs2, x->b);
    byte_string_join(bs, bs1, bs2);
    byte_string_clear(bs1);
    byte_string_clear(bs2);
}

void byte_string_set_point(byte_string_t bs, point_t P)
{
    byte_string_t bs1, bs2;
    byte_string_set_fp2(bs1, P->x);
    byte_string_set_fp2(bs2, P->y);
    byte_string_join(bs, bs1, bs2);
    byte_string_clear(bs1);
    byte_string_clear(bs2);
}

void mympz_set_byte_string(mpz_t z, byte_string_t bs)
{
    mympz_inp_raw(z, bs->data, bs->len);
}

void fp2_set_byte_string(fp2_t x, byte_string_t bs)
{
    byte_string_t bs1, bs2;

    byte_string_split(bs1, bs2, bs);

    mympz_set_byte_string(x->a, bs1);
    mympz_set_byte_string(x->b, bs2);

    byte_string_clear(bs1);
    byte_string_clear(bs2);
}

void point_set_byte_string(point_t P, byte_string_t bs)
{
    byte_string_t bs1, bs2;

    byte_string_split(bs1, bs2, bs);

    fp2_set_byte_string(P->x, bs1);
    fp2_set_byte_string(P->y, bs2);

    byte_string_clear(bs1);
    byte_string_clear(bs2);
}

static void point_Phi(point_t PhiP, point_t P, params_t params)
{
    fp2_mul(PhiP->x, P->x, params->zeta, params->p);
    fp2_set(PhiP->y, P->y);
}

static void point_make_order_q(point_t P, params_t params)
//P = ((p + 1)/q)P 
//so it has order q
{
    point_mul(P, params->p1onq, P, params->curve);
}

static void initpq(params_t params)
//calculate system parameters that can be determined from p and q
//also initialize the elliptic curve library so points can be used
{
    mpz_init(params->p1onq);
    mpz_add_ui(params->p1onq, params->p, 1);
    mpz_divexact(params->p1onq, params->p1onq, params->q);

    //initialize the elliptic curve library
    curve_init(params->curve, params->p, params->q);

    fp2_init(params->zeta);
    fp2_set_cbrt_unity(params->zeta, params->p);
}

void hash_G(mpz_t h, byte_string_t bs, params_t params)
//hash a byte_string to an element of Z_p
{
    byte_string_t md_value;

    crypto_hash(md_value, bs);

    mympz_from_hash(h, params->p, md_value);

    byte_string_clear(md_value);
}

void hash_H(byte_string_t md_value, fp2_t x, params_t params)
//hash a point of E/F_p^2 to a byte_string
{
    byte_string_t bs;

    byte_string_set_fp2(bs, x);

    crypto_hash(md_value, bs);

    byte_string_clear(bs);
}

static void map_byte_string_to_point(point_t d,
	byte_string_t bs, params_t params)
//converts byte_string into a point of order q on E/F_p
{
    int i;
    fp2_t x, y;

    bm_put(bm_get_time(), "mtp0");

    fp2_init(x);
    fp2_init(y);

    //first use G function to choose x coordinate
    hash_G(y->a, bs, params);

    mpz_set_ui(x->b, 0);
    mpz_set_ui(y->b, 0);

    bm_put(bm_get_time(), "mtp1");

    for (i=1;;i++) {
	x_from_y(x->a, y->a, params->curve);
	//we now have a point d
	fp2_set(d->x, x);
	fp2_set(d->y, y);

	//make it order q
	point_make_order_q(d, params);
	if (!d->infinity) break;

	//unlikely case: pick first available point
	mpz_set_ui(y->a, i);
    }
    bm_put(bm_get_time(), "mtp2");
    /*
    printf("mapped to ");
    point_out_str(NULL, 0, d);
    printf("\n");
    */

    fp2_clear(x);
    fp2_clear(y);
}

static void map_to_point(point_t d, const char *id, params_t params)
//converts id into a point of order q on E/F_p
{
    byte_string_t bsid;

    byte_string_set(bsid, id);
    map_byte_string_to_point(d, bsid, params);
    byte_string_clear(bsid);
}

void IBE_init(void)
//NOT THREAD-SAFE. May add contexts here eventually...
{
    crypto_init();
}

void IBE_clear(void)
/* Free memory used by library
 */
{
    crypto_clear();
}

void params_robust_clear(params_t params)
{
    int i;

    for (i=0; i<params->sharen; i++) {
	point_clear(params->robustP[i]);
	mpz_clear(params->robustx[i]);
    }
    free(params->robustP);
    free(params->robustx);
    params->sharet = params->sharen = 0;
}

void params_clear(params_t params)
{
    curve_clear(params->curve);

    mpz_clear(params->p1onq);
    fp2_clear(params->zeta);

    miller_cache_clear(params->Ppub_mc);

    if (params->sharen) params_robust_clear(params);

    mpz_clear(params->p);
    mpz_clear(params->q);
    point_clear(params->P);
    point_clear(params->Ppub);
    point_clear(params->PhiPpub);

    free(params->version);
    free(params->id);
}

void IBE_setup(params_t params, byte_string_t master, int k, int qk, char *id)
/* generate system parameters
 * k = number of bits in p (should be at least 512)
 * qk = number of bits in q (size of subgroup, 160 is typical)
 * id = system ID
 */
{
    mpz_t p, q, r;
    mpz_t x;
    point_ptr P;

    int solinasa, solinasb;

    int kqk = k - qk - 4; //lose 4 bits since 12 is a 4-bit no.
    unsigned int seed;

    mpz_init(p); mpz_init(q); mpz_init(r); mpz_init(x);

    //find random k-bit prime p such that
    //p = 2 mod 3 and q = (p+1)/12r is prime as well for some r

    //now also want q to be a Solinas prime
    //we use rand() to help us find one: should be ok
    crypto_rand_bytes((unsigned char *) &seed, sizeof(int));
    srand(seed);

    for(;;) {
	//once q was just a random qk-bit prime
	//now it must be a Solinas one
	mpz_set_ui(q, 0);

	solinasa = qk - 1;

	mpz_setbit(q, solinasa);
	mpz_set_ui(r, 0);

	solinasb = rand() % qk;
	mpz_setbit(r, solinasb);
	if (rand() % 2) {
	    mpz_add(q, q, r);
	} else {
	    mpz_sub(q, q, r);
	    solinasb = -solinasb;
	}
	mpz_set_ui(r, 1);
	if (rand() % 2) {
	    mpz_add(q, q, r);
	} else {
	    mpz_sub(q, q, r);
	    solinasa = -solinasa;
	}

	if (!mpz_probab_prime_p(q, 10)) continue;
	mympz_randomb(r, kqk);
	//p = q * r
	mpz_mul(p, q, r);

	//r = (((p << 1) + p) << 2) (= 12p)
	mpz_mul_2exp(r, p, 1);
	mpz_add(r, r, p);
	mpz_mul_2exp(r, r, 2);

	//p = r - 1
	mpz_set_ui(p, 1);
	mpz_sub(p, r, p);
	if (mpz_probab_prime_p(p, 10)) break;
    }

    //pick master key x from F_q
    mympz_randomm(x, q);
    byte_string_set_mpz(master, x);

    mpz_init(params->p);
    mpz_init(params->q);
    mpz_set(params->p, p);
    mpz_set(params->q, q);

    initpq(params);

    //pick random point P of order q from E/F_p
    point_init(params->P);
    P = params->P;

    do {
	point_random(P, params->curve);
	point_make_order_q(P, params);
    } while (P->infinity);

    point_init(params->Ppub);
    point_mul(params->Ppub, x, P, params->curve);

    point_mul_preprocess(P, params->curve);

    miller_cache_init(params->Ppub_mc, params->curve);
    tate_preprocess(params->Ppub_mc, params->Ppub, params->curve);

    point_init(params->PhiPpub);
    point_Phi(params->PhiPpub, params->Ppub, params);

    params->id = (char *) malloc(strlen(id) + 1);
    strcpy(params->id, id);

    params->sharet = params->sharen = 0;
    params->version = (char *) malloc(strlen(IBE_VERSION) + 1);
    strcpy(params->version, IBE_VERSION);

    mpz_clear(p); mpz_clear(q); mpz_clear(r); mpz_clear(x);
}

void IBE_extract_byte_string(byte_string_t bs, byte_string_t master,
	byte_string_t id, params_t params)
//for testing purposes
{
    point_t key;
    mpz_t x;

    mpz_init(x);
    point_init(key);

    map_byte_string_to_point(key, id, params);

    mympz_set_byte_string(x, master);
    point_mul(key, x, key, params->curve);
    byte_string_set_point(bs, key);

    point_clear(key);
    mpz_clear(x);
}

void IBE_extract(byte_string_t bs,
	byte_string_t master, const char *id, params_t params)
//same as above, char * version
{
    byte_string_t bsid;

    byte_string_set(bsid, id);
    IBE_extract_byte_string(bs, master, bsid, params);
    byte_string_clear(bsid);
}

void IBE_extract_share_byte_string(byte_string_t share, byte_string_t mshare,
	byte_string_t id, params_t params)
//extract a private key share from an ID and master share
{
    mpz_t y;
    int i;

    byte_string_t bs1, bs2;
    point_t yd;
    point_t d;

    point_init(yd);
    point_init(d);

    mpz_init(y);

    byte_string_split(bs1, bs2, mshare);
    i = int_from_byte_string(bs1);
    mympz_set_byte_string(y, bs2);
    byte_string_clear(bs1);
    byte_string_clear(bs2);

    map_byte_string_to_point(d, id, params);

    point_mul(yd, y, d, params->curve);

    byte_string_set_int(bs1, i);
    byte_string_set_point(bs2, yd);
    byte_string_join(share, bs1, bs2);
    byte_string_clear(bs1);
    byte_string_clear(bs2);
    point_clear(yd);
    point_clear(d);

    mpz_clear(y);
}

void IBE_extract_share(byte_string_t share,
	byte_string_t mshare, const char *id, params_t params)
//same as above except takes in char * instead of byte_string
{
    byte_string_t bsid;

    byte_string_set(bsid, id);
    IBE_extract_share_byte_string(share, mshare, bsid, params);
    byte_string_clear(bsid);
}

int IBE_combine(byte_string_t key, byte_string_t *kshare, params_t params)
//reconstruct a key from key shares, or a certificate from certificate shares
{
    int i, j;
    int indexi, indexj;
    point_t yP;
    mpz_t num, denom;
    mpz_t z;
    point_t d;
    byte_string_t bs1, bs2;
    int *index;

    index = (int *) alloca(params->sharet * sizeof(int));

    for (i=0; i<params->sharet; i++) {
	byte_string_split(bs1, bs2, kshare[i]);
	byte_string_clear(bs2);
	index[i] = int_from_byte_string(bs1);
	byte_string_clear(bs1);
	for (j=0; j<i; j++) {
	    if (index[i] == index[j]) {
		return 0;
	    }
	}
    }

    point_init(yP);
    mpz_init(z);
    mpz_init(num); mpz_init(denom);
    point_init(d);
    point_set_O(d);

    for (i=0; i<params->sharet; i++) {
	byte_string_split(bs1, bs2, kshare[i]);
	indexi = index[i];
	byte_string_clear(bs1);
	point_set_byte_string(yP, bs2);
	byte_string_clear(bs2);
	mpz_set_ui(num, 1);
	mpz_set_ui(denom, 1);
	for (j=0; j<params->sharet; j++) {
	    if (j != i) {
		indexj = index[j];
		mpz_mul(num, num, params->robustx[indexj]);
		mpz_mod(num, num, params->q);
		mpz_sub(z, params->robustx[indexj], params->robustx[indexi]);
		mpz_mul(denom, denom, z);
		mpz_mod(denom, denom, params->q);
	    }
	}
	mpz_invert(denom, denom, params->q);
	mpz_mul(z, num, denom);
	mpz_mod(z, z, params->q);
	point_mul(yP, z, yP, params->curve);
	point_add(d, d, yP, params->curve);
    }
    byte_string_set_point(key, d);

    point_clear(yP);
    mpz_clear(z);
    mpz_clear(num); mpz_clear(denom);
    point_clear(d);

    return 1;
}

void IBE_get_shared_secret_preprocess(preprocessed_key_t pk,
	byte_string_t key, params_t params)
{
    point_t Q;

    point_init(Q);

    point_set_byte_string(Q, key);
    tate_preprocess(pk->mc, Q, params->curve);

    point_clear(Q);
}

void IBE_get_shared_secret_postprocess(byte_string_t s,
	char *id, preprocessed_key_t pk, params_t params)
{
    point_t Qid;
    fp2_t gid;

    fp2_init(gid);
    point_init(Qid);
    map_to_point(Qid, id, params);
    //calculate gid = e(Q_id, Phi(P_pub))
    point_Phi(Qid, Qid, params);
    tate_postprocess(gid, pk->mc, Qid, params->curve);

    hash_H(s, gid, params);

    fp2_clear(gid);
    point_clear(Qid);
}

void IBE_KEM_encrypt_array(byte_string_t *s, byte_string_t U,
	char **idarray, int count, params_t params)
{
    int i;
    mpz_t r;
    fp2_t gidr;
    point_t Qid;
    point_t rP;

    if (count <= 0) return;

    //r is random in F_q
    mpz_init(r);
    mympz_randomm(r, params->q);
    bm_put(bm_get_time(), "rP0");

    //U = rP
    point_init(rP);
    //point_mul(rP, r, params->P);
    point_mul_postprocess(rP, r, params->curve);
    bm_put(bm_get_time(), "rP1");
    fp2_init(gidr);

    byte_string_set_point(U, rP);

    point_clear(rP);

    point_init(Qid);

    for (i=0; i<count; i++) {
	const char *id = idarray[i];

	//XXX:set up a cache to avoid these expensive ops
	map_to_point(Qid, id, params);
	point_Phi(Qid, Qid, params);
	
	//calculate gidr = e(Q_id, Phi(P_pub))^r

	//tate_pairing(gidr, Qid, PhiPpub);
	tate_postprocess(gidr, params->Ppub_mc, Qid, params->curve);

	bm_put(bm_get_time(), "gidr0");
	fp2_pow(gidr, gidr, r, params->p);
	bm_put(bm_get_time(), "gidr1");

	hash_H(s[i], gidr, params);
    }

    point_clear(Qid);
    mpz_clear(r);
    fp2_clear(gidr);
}

void IBE_KEM_encrypt(byte_string_t secret,
	byte_string_t U, char *id, params_t params)
{
    byte_string_t s[1];

    IBE_KEM_encrypt_array(s, U, &id, 1, params);
    byte_string_assign(secret, s[0]);
}

void IBE_KEM_decrypt(byte_string_t s,
	byte_string_t U, byte_string_t key, params_t params)
{
    point_t xQ, rP;
    fp2_t res;

    fp2_init(res);

    point_init(xQ);
    point_init(rP);

    point_set_byte_string(xQ, key);
    point_set_byte_string(rP, U);

    point_Phi(rP, rP, params);
    tate_pairing(res, xQ, rP, params->curve);
    hash_H(s, res, params);

    fp2_clear(res);
    point_clear(xQ);
    point_clear(rP);
}

void IBE_get_shared_secret(byte_string_t s,
	char *id, byte_string_t key, params_t params)
{
    point_t Qid;
    fp2_t gid;
    point_t Q;

    point_init(Q);

    fp2_init(gid);
    point_init(Qid);
    map_to_point(Qid, id, params);
    //calculate gid = e(Q, Phi(Q_id))
    point_set_byte_string(Q, key);
    point_Phi(Qid, Qid, params);
    tate_pairing(gid, Q, Qid, params->curve);

    hash_H(s, gid, params);

    fp2_clear(gid);
    point_clear(Q);
    point_clear(Qid);
}

char* IBE_version(params_t params)
{
    return params->version;
}

char* IBE_system(params_t params)
{
    return params->id;
}

int IBE_threshold(params_t params)
{
    return params->sharet;
}

void IBE_split_master(byte_string_t *mshare,
	byte_string_t master, int t, int n, params_t params)
//split the master key into n pieces
//t of them are required to generate a key
{
    mpz_t poly[t];
    mpz_t *x;
    mpz_t y[n];
    mpz_t z;
    mpz_t r;
    int i, j;
    byte_string_t bs1, bs2;
    //start with random polynomial with degree <= t-2
    //(in F_q)
    //whose constant term = master

    mpz_init(r);

    mpz_init(poly[0]);
    mympz_set_byte_string(poly[0], master);
    for (i=1; i<t; i++) {
	mpz_init(poly[i]);
	mympz_randomm(r, params->q);
	mpz_set(poly[i], r);
    }

    mpz_init(z);

    if (0) {
	printf("secret poly: ");
	for (i=0; i<t; i++) {
	    printf(" ");
	    mpz_out_str(NULL, 0, poly[i]);
	}
	printf("\n");
    }

    params->robustx = (mpz_t *) malloc(n * sizeof(mpz_t));
    params->robustP = (point_t *) malloc(n * sizeof(point_t));
    params->sharet = t;
    params->sharen = n;
    x = params->robustx;

    for (i=0; i<n; i++) {
	mympz_randomm(r, params->q);
	mpz_init(x[i]);
	mpz_set(x[i], r);
	//Horner's rule
	mpz_set_ui(z, 0);
	for (j=t-1; j>=0; j--) {
	    mpz_mul(z, z, x[i]);
	    mpz_add(z, z, poly[j]);
	    mpz_mod(z, z, params->q);
	}
	mpz_init_set(y[i], z);

	byte_string_set_int(bs1, i);
	byte_string_set_mpz(bs2, z);
	byte_string_join(mshare[i], bs1, bs2);
	byte_string_clear(bs1);
	byte_string_clear(bs2);
    }

    for (i=0; i<n; i++) {
	point_init(params->robustP[i]);
	point_mul(params->robustP[i], y[i], params->Ppub, params->curve);
    }

    for (i=0; i<t; i++) {
	mpz_clear(poly[i]);
    }

    for (i=0; i<n; i++) {
	mpz_clear(y[i]);
    }

    mpz_clear(z);

    mpz_clear(r);
}

int IBE_construct_master(byte_string_t master,
	byte_string_t *mshare, params_t params)
//reconstruct the master from master shares
//(shouldn't normally be used since they should never be in one place)
{
    int i, j;
    int indexi, indexj;
    int t = params->sharet;
    mpz_t x;
    mpz_t y;
    mpz_t z;
    mpz_t num, denom;
    byte_string_t bs1, bs2;

    mpz_init(x);
    mpz_init(y);
    mpz_init(z);
    mpz_init(num);
    mpz_init(denom);
    mpz_set_ui(x, 0);
    for (i=0; i<t; i++) {
	mpz_set_ui(num, 1);
	mpz_set_ui(denom, 1);
	byte_string_split(bs1, bs2, mshare[i]);
	indexi = int_from_byte_string(bs1);
	byte_string_clear(bs1);
	mympz_set_byte_string(y, bs2);
	byte_string_clear(bs2);
	for (j=0; j<t; j++) {
	    if (j != i) {
		byte_string_split(bs1, bs2, mshare[j]);
		indexj = int_from_byte_string(bs1);
		byte_string_clear(bs1);
		byte_string_clear(bs2);
		mpz_mul(num, num, params->robustx[indexj]);
		mpz_mod(num, num, params->q);
		mpz_sub(z, params->robustx[indexj], params->robustx[indexi]);
		mpz_mul(denom, denom, z);
		mpz_mod(denom, denom, params->q);
	    }
	}
	mpz_invert(denom, denom, params->q);
	mpz_mul(z, num, denom);
	mpz_mod(z, z, params->q);
	mpz_mul(z, z, y);
	mpz_mod(z, z, params->q);
	mpz_add(x, x, z);
	if (mpz_cmp(x, params->q) >= 0) {
	    mpz_sub(x, x, params->q);
	}
    }
    byte_string_set_mpz(master, x);

    mpz_clear(x);
    mpz_clear(y);
    mpz_clear(z);
    mpz_clear(num);
    mpz_clear(denom);

    return 1;
}

int IBE_serialize_params(byte_string_t bs, params_t params)
//put system parameters into a byte_string
{
    int i, j;
    byte_string_t *bsa;

    i = 0;

    bsa = (byte_string_t *) alloca(sizeof(byte_string_t)
	    * (2 * params->sharen + 20));

    byte_string_set(bsa[i++], params->version);
    byte_string_set(bsa[i++], params->id);
    byte_string_set_mpz(bsa[i++], params->p);
    byte_string_set_mpz(bsa[i++], params->q);
    byte_string_set_point(bsa[i++], params->P);
    byte_string_set_point(bsa[i++], params->Ppub);
    byte_string_set_int(bsa[i++], params->sharet);
    byte_string_set_int(bsa[i++], params->sharen);

    for (j=0; j<params->sharen; j++) {
	byte_string_set_mpz(bsa[i++], params->robustx[j]);
	byte_string_set_point(bsa[i++], params->robustP[j]);
    }

    byte_string_encode_array(bs, bsa, i);

    for (j=0; j<i; j++) {
	byte_string_clear(bsa[j]);
    }

    return 1;
}

int IBE_deserialize_params(params_t params, byte_string_t bs)
//get system parameters from a byte_string
{
    byte_string_t *bsa;
    int n;
    int i, j;

    byte_string_decode_array(&bsa, &n, bs);
    //TODO: check n is big enough

    i = 0;

    params->version = charstar_from_byte_string(bsa[i++]);
    params->id = charstar_from_byte_string(bsa[i++]);

    mpz_init(params->p);
    mpz_init(params->q);
    mympz_set_byte_string(params->p, bsa[i++]);
    mympz_set_byte_string(params->q, bsa[i++]);

    initpq(params);
    
    point_init(params->P);
    point_init(params->Ppub);

    point_set_byte_string(params->P, bsa[i++]);
    point_set_byte_string(params->Ppub, bsa[i++]);

    point_init(params->PhiPpub);
    point_Phi(params->PhiPpub, params->Ppub, params);

    params->sharet = int_from_byte_string(bsa[i++]);
    params->sharen = int_from_byte_string(bsa[i++]);

    params->robustx = (mpz_t *) malloc(params->sharen * sizeof(mpz_t));
    params->robustP = (point_t *) malloc(params->sharen * sizeof(point_t));
    for (j=0; j<params->sharen; j++) {
	mpz_init(params->robustx[j]);
	mympz_set_byte_string(params->robustx[j], bsa[i++]);
	//TODO: check it's different from others
	point_init(params->robustP[j]);
	point_set_byte_string(params->robustP[j], bsa[i++]);
    }

    point_mul_preprocess(params->P, params->curve);
    miller_cache_init(params->Ppub_mc, params->curve);
    tate_preprocess(params->Ppub_mc, params->Ppub, params->curve);

    for(i=0; i<n; i++) {
	byte_string_clear(bsa[i]);
    }
    free(bsa);

    return 1;
}

void params_out(FILE *outfp, params_t params)
{
    fprintf(outfp, "IBE version: %s\n", params->version);
    fprintf(outfp, "system ID: %s\n", params->id);

    fprintf(outfp, "p = ");
    mpz_out_str(outfp, 0, params->p);
    fprintf(outfp, "\n");
    fprintf(outfp, "q = ");
    mpz_out_str(outfp, 0, params->q);
    fprintf(outfp, "\n");
    fprintf(outfp, "P = ");
    point_out_str(outfp, 0, params->P);
    fprintf(outfp, "\n");
    fprintf(outfp, "Ppub = ");
    point_out_str(outfp, 0, params->Ppub);
    fprintf(outfp, "\n");
    fprintf(outfp, "share t/n = %d/%d\n", params->sharet, params->sharen);
}

void BLS_keygen(byte_string_t private, byte_string_t public, params_t params)
{
    mpz_t x;
    point_t xP;

    //pick random private key x in F_q
    mpz_init(x);
    mympz_randomm(x, params->q);
    byte_string_set_mpz(private, x);

    //public = x params->P
    point_init(xP);
    point_mul(xP, x, params->P, params->curve);
    byte_string_set_point(public, xP);

    mpz_clear(x);
    point_clear(xP);
}

void BLS_sign(byte_string_t sig,
	byte_string_t message, byte_string_t key, params_t params)
{
    mpz_t x;
    point_t P, xP;

    point_init(P);
    //hash message to point
    map_byte_string_to_point(P, message, params);

    //multiply it by key
    mpz_init(x);
    mympz_set_byte_string(x, key);
    point_init(xP);
    point_mul(xP, x, P, params->curve);

    //xP is the signature
    byte_string_set_point(sig, xP);

    mpz_clear(x);
    point_clear(P);
    point_clear(xP);
}

int is_DDH_tuple(point_t P, point_t aP, point_t bP, point_t cP, params_t params)
{
    int result;
    fp2_t a, b;
    point_t PhicP, PhibP;

    fp2_init(a); fp2_init(b);
    point_init(PhibP); point_init(PhicP);

    point_Phi(PhicP, cP, params);
    point_Phi(PhibP, bP, params);
    tate_pairing(a, P, PhicP, params->curve);
    tate_pairing(b, aP, PhibP, params->curve);
    result = fp2_equal(a, b);

    point_clear(PhibP);
    point_clear(PhicP);

    fp2_clear(a); fp2_clear(b);
    return result;
}

int BLS_verify(byte_string_t sig,
	byte_string_t message, byte_string_t pubkey, params_t params)
{
    int result;

    point_t P, xP;
    point_t Q, xQ;

    point_init(P);
    point_init(xP);
    point_init(Q);
    point_init(xQ);
    //hash message to point Q
    map_byte_string_to_point(Q, message, params);

    //sig should be xP
    point_set_byte_string(xQ, sig);

    //pubkey is xP
    point_set_byte_string(xP, pubkey);
    point_set(P, params->P);

    //verify P, xP, Q, xQ is DDH
    result = is_DDH_tuple(P, xP, Q, xQ, params);

    point_clear(P);
    point_clear(xP);
    point_clear(Q);
    point_clear(xQ);

    return result;
}

//using certificates and BLS we can do identity-based signatures
void IBE_keygen(byte_string_t privkey, byte_string_t pubkey, params_t params)
{
    BLS_keygen(privkey, pubkey, params);
}

void IBE_certify_share(byte_string_t share, byte_string_t mshare,
	byte_string_t public, const char *id, params_t params)
{
    byte_string_t H;
    byte_string_t bsid;

    byte_string_set(bsid, id);

    crypto_va_hash(H, 2, public, bsid);

    byte_string_clear(bsid);

    IBE_extract_share_byte_string(share, mshare, H, params);

    byte_string_clear(H);
}

void IBE_certify(byte_string_t cert, byte_string_t master,
	byte_string_t public, const char *id, params_t params)
{
    byte_string_t H;
    byte_string_t bsid;

    byte_string_set(bsid, id);

    crypto_va_hash(H, 2, public, bsid);

    byte_string_clear(bsid);

    IBE_extract_byte_string(cert, master, H, params);

    byte_string_clear(H);
}

void IBE_sign(byte_string_t sig, byte_string_t message, byte_string_t private,
	byte_string_t cert, params_t params)
{
    mpz_t x;
    point_t P, xP, C;

    point_init(P);
    point_init(xP);
    point_init(C);
    //hash message to point
    map_byte_string_to_point(P, message, params);

    //multiply it by key
    mpz_init(x);
    mympz_set_byte_string(x, private);
    point_mul(xP, x, P, params->curve);

    point_set_byte_string(C, cert);

    //signature = BLS signature + certificate
    point_add(C, C, xP, params->curve);
    byte_string_set_point(sig, C);

    mpz_clear(x);
    point_clear(P);
    point_clear(xP);
    point_clear(C);
}

int IBE_verify(byte_string_t sig, byte_string_t message, byte_string_t public,
	const char *id, params_t params)
{
    int result;

    point_t P, Q;
    fp2_t f1, f2, f3;

    byte_string_t H;
    byte_string_t bsid;

    fp2_init(f1); fp2_init(f2); fp2_init(f3);

    //compute LHS of equation = e(P, sig)
    point_init(P);
    point_set(P, params->P);

    point_init(Q);
    point_set_byte_string(Q, sig);
    point_Phi(Q, Q, params);
    tate_pairing(f1, P, Q, params->curve);

    //compute first factor on RHS = e(public key, message)

    point_set_byte_string(P, public);
    map_byte_string_to_point(Q, message, params);
    point_Phi(Q, Q, params);
    tate_pairing(f2, P, Q, params->curve);

    //compute second factor on RHS = e(server public key, cert plaintext)
    byte_string_set(bsid, id);

    crypto_va_hash(H, 2, public, bsid);
    map_byte_string_to_point(Q, H, params);

    byte_string_clear(bsid);
    byte_string_clear(H);

    point_Phi(Q, Q, params);
    tate_pairing(f3, params->Ppub, Q, params->curve);

    fp2_mul(f2, f2, f3, params->p);

    if (!fp2_equal(f1, f2)) {
	result = 0;
    } else result = 1;

    point_clear(P); point_clear(Q);
    fp2_clear(f1); fp2_clear(f2); fp2_clear(f3);

    return result;
}

int IBE_hide_key_array(byte_string_t U, byte_string_t *V,
	char **id, int idcount, byte_string_t K, params_t params)
{
    int i;
    byte_string_t *secret;

    bm_put(bm_get_time(), "enc0");

    secret = (byte_string_t *) alloca(sizeof(byte_string_t) * idcount);

    IBE_KEM_encrypt_array(secret, U, id, idcount, params);

    for (i=0; i<idcount; i++) {
	if (1 != crypto_encrypt(V[i], K, secret[i])) {
	    //error: crypto_encrypt failed
	    for (i--; i>=0; i--) {
		byte_string_clear(V[i]);
	    }
	    for (i=0; i<idcount; i++) {
		byte_string_clear(secret[i]);
	    }
	    return 0;
	}
    }

    for (i=0; i<idcount; i++) {
	byte_string_clear(secret[i]);
    }

    bm_put(bm_get_time(), "enc1");
    bm_report_encrypt();

    return 1;
}

int IBE_hide_key(byte_string_t U, byte_string_t V,
	char *id, byte_string_t K, params_t params)
{
    byte_string_t bs[1];
    int result;

    result = IBE_hide_key_array(U, bs, &id, 1, K, params);

    byte_string_assign(V, bs[0]);

    return result;
}

int IBE_reveal_key(byte_string_t K,
	byte_string_t U, byte_string_t V, byte_string_t privkey, params_t params)
{
    int result = 1;
    byte_string_t secret;

    IBE_KEM_decrypt(secret, U, privkey, params);

    if (1 != crypto_decrypt(K, V, secret)) {
	fprintf(stderr, "WARNING: INVALID CIPHERTEXT!\n");
	result = 0;
    }

    byte_string_clear(secret);

    return result;
}
