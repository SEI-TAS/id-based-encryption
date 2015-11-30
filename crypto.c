/* Crypto layer
 * Currently wrappers around OpenSSL functions
 * possibly substitute with other library in future
 * Ben Lynn
 */

/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/

#include <stdarg.h>
#include <string.h>
#include <openssl/rand.h>
#include <openssl/bn.h>
#include <gmp.h>
#include "crypto.h"
#include "byte_string.h"

//OpenSSL will be changed soon, I've started preparing for this
//when we upgrade to 0.9.7 remove these macros:
#define do_nothing ((void) (0))
#define HMAC_CTX_init(x) do_nothing
#define HMAC_CTX_cleanup(x) HMAC_cleanup(x)

static EVP_MD *md;
static int md_length;
static EVP_CIPHER* cipher;
static int iv_length;

void crypto_init(void)
//NOT THREAD-SAFE. May add contexts here eventually...
{
    //OpenSSL_add_all_algorithms();
    //md = EVP_get_digestbyname("SHA1");
    md = EVP_sha1();
    md_length = EVP_MD_size(md);
    cipher = EVP_des_ede3_cbc();
    iv_length =  EVP_CIPHER_iv_length(cipher);
}

void crypto_clear(void)
{
    //Not needed(?):
    //EVP_cleanup();
}

void crypto_hash(byte_string_t md_value, byte_string_t bs)
//hash a byte_string to a byte_string of certain length
//TODO: clarify this
{
    EVP_MD_CTX mdctx;
    int i;

    byte_string_init(md_value, md_length); //EVP_MAX_MD_SIZE

    EVP_DigestInit(&mdctx, md);
    EVP_DigestUpdate(&mdctx, bs->data, bs->len);
    EVP_DigestFinal(&mdctx, md_value->data, &i);
}

void crypto_va_hash(byte_string_t md_value, int n, ...)
//vararg version
//hashes the concatenation of some number of byte_strings
{
    EVP_MD_CTX mdctx;
    va_list ap;
    int i;
    byte_string_ptr bs;

    byte_string_init(md_value, md_length);

    va_start(ap, n);
    EVP_DigestInit(&mdctx, md);

    for (i=0; i<n; i++) {
	bs = (byte_string_ptr) va_arg(ap, void *);
	EVP_DigestUpdate(&mdctx, bs->data, bs->len);
    }
    va_end(ap);

    EVP_DigestFinal(&mdctx, md_value->data, &i);
}

int crypto_generate_key(byte_string_t key)
{
    byte_string_init(key, EVP_MAX_KEY_LENGTH);

    if (1 != RAND_bytes(key->data, key->len)) {
	byte_string_clear(key);
	return 0;
    }

    return 1;
}

int crypto_generate_salt(byte_string_t salt)
{
    int saltlen = iv_length;
    byte_string_init(salt, saltlen);

    if (1 != RAND_bytes(salt->data, saltlen)) {
	byte_string_clear(salt);
	return 0;
    }

    return 1;
}

void crypto_ctx_init(crypto_ctx_t c)
{
    HMAC_CTX_init(&c->macctx);
    EVP_CIPHER_CTX_init(&c->ctx);
    c->auxbuf->len = 0;
    c->ivbuf->len = 0;
}

void crypto_ctx_clear(crypto_ctx_t c)
{
    HMAC_CTX_cleanup(&c->macctx);
    EVP_CIPHER_CTX_cleanup(&c->ctx);

    if (c->auxbuf->len) {
	byte_string_clear(c->auxbuf);
    }
    if (c->ivbuf->len) {
	byte_string_clear(c->ivbuf);
    }
}

void crypto_make_chkeys(byte_string_t ckey, byte_string_t hkey, byte_string_t secret)
//make sure ckey and hkey are independent by appending different characters
//to `secret' and then hashing it
{
    byte_string_t bs1, bs2;

    byte_string_init(bs1, secret->len + 1);
    byte_string_init(bs2, secret->len + 1);

    memcpy(bs1->data, secret->data, secret->len);
    bs1->data[secret->len] = '1';

    memcpy(bs2->data, secret->data, secret->len);
    bs2->data[secret->len] = '2';

    byte_string_init(ckey, EVP_MAX_KEY_LENGTH);

    EVP_BytesToKey(cipher, md, NULL, bs1->data,
	    bs1->len, 1, ckey->data, NULL);

    crypto_hash(hkey, bs2);

    byte_string_clear(bs1);
    byte_string_clear(bs2);
}

int crypto_encrypt_init(crypto_ctx_t c, byte_string_t secret)
{
    byte_string_t hkey, ckey;

    if (1 != crypto_generate_salt(c->ivbuf)) {
	//error: crypto_generate_salt failed
	return 0;
    }

    c->state = 0;

    crypto_make_chkeys(ckey, hkey, secret);

    if (1 != EVP_EncryptInit(&c->ctx, cipher, ckey->data, c->ivbuf->data)) {
	//error: EVP_EncryptInit failed
	return 0;
    }

    HMAC_Init(&c->macctx, hkey->data, hkey->len, md);

    byte_string_clear(hkey);
    byte_string_clear(ckey);
    return 1;
}

int crypto_encrypt_update(crypto_ctx_t c, unsigned char *out, int *outl,
	unsigned char *in, int inl)
{
    if (!c->state) {
	//the first time we need to prepend the IV
	c->state++;

	memcpy(out, c->ivbuf->data, iv_length);
	byte_string_clear(c->ivbuf);
	if (1 != EVP_EncryptUpdate(&c->ctx, &out[iv_length], outl, in, inl)) {
	    //error: EVP_EncryptUpdate failed
	    return 0;
	}
	*outl += iv_length;
    } else {
	if (1 != EVP_EncryptUpdate(&c->ctx, out, outl, in, inl)) {
	    //error: EVP_EncryptUpdate failed
	    return 0;
	}
    }

    HMAC_Update(&c->macctx, out, *outl);
    return 1;
}

int crypto_encrypt_final(crypto_ctx_t c, unsigned char *out, int *outl)
{
    byte_string_t mac;

    if (1 != EVP_EncryptFinal(&c->ctx, out, outl)) {
	//error: EVP_EncryptFinal failed
	return 0;
    }
    //append the MAC
    HMAC_Update(&c->macctx, out, *outl);

    byte_string_init(mac, md_length);
    HMAC_Final(&c->macctx, mac->data, &mac->len);
    memcpy(&out[*outl], mac->data, md_length);
    *outl += md_length;
    byte_string_clear(mac);
    return 1;
}

int crypto_encrypt(byte_string_t ctext, byte_string_t ptext,
	byte_string_t secret)
//encrypt short messages that fit in memory
{
    int i;
    crypto_ctx_t c;

    byte_string_init(ctext, ptext->len + crypto_block_size());

    crypto_ctx_init(c);
    if (1 != crypto_encrypt_init(c, secret)) {
	byte_string_clear(ctext);
	crypto_ctx_clear(c);
	return 0;
    }
    if (1 != crypto_encrypt_update(c, ctext->data, &i, ptext->data, ptext->len)) {
	byte_string_clear(ctext);
	crypto_ctx_clear(c);
	return 0;
    }
    if (1 != crypto_encrypt_final(c, &ctext->data[i], &ctext->len)) {
	byte_string_clear(ctext);
	crypto_ctx_clear(c);
	return 0;
    }

    crypto_ctx_clear(c);
    ctext->len += i;
    byte_string_reinit(ctext, ctext->len);
    return 1;
}

int crypto_decrypt_init(crypto_ctx_t c, byte_string_t secret)
{
    byte_string_t hkey;

    crypto_make_chkeys(c->auxbuf, hkey, secret);

    c->state = 0;
    byte_string_init(c->ivbuf, iv_length);
    c->count = 0;

    HMAC_Init(&c->macctx, hkey->data, hkey->len, md);
    byte_string_clear(hkey);
    return 1;
}


static int crypto_decrypt_advance(crypto_ctx_t c,
	unsigned char **out, int *outl,
	unsigned char *in, int inl)
{
    int inc;

    HMAC_Update(&c->macctx, in, inl);
    if (1 != EVP_DecryptUpdate(&c->ctx, *out, &inc, in, inl)) {
	///error: EVP_DecryptUpdate failed
	return 0;
    }
    *out += inc;
    *outl += inc;
    return 1;
}

int crypto_decrypt_update(crypto_ctx_t c, unsigned char *out, int *outl,
	unsigned char *in, int inl)
//complicated by two things:
//
//1. the IV is prepended to the ciphertext, but we may get passed the
//ciphertext in very small chunks
//
//2. the size of the ciphertext is not known in
//advance, thus the only way of finding the MAC at the end is to
//keep the last few bytes in a special buffer
{
    unsigned char *vin;
    int vinl;
    unsigned char *vout = out;
    *outl = 0;

    if (!c->state) {
	//IV has not been completely read yet
	//DecryptInit() has not been called yet
	//auxbuf holds the key for now
	int l;
	if (c->count + inl >= iv_length) {
	    l = iv_length - c->count;
	    c->state++;
	} else {
	    l = inl;
	}
	memcpy(&c->ivbuf->data[c->count], in, l);
	c->count += l;

	if (!c->state) {
	    //no state change, then we're done
	    return 1;
	} else {
	    //state change means that IV has finished reading
	    //can now setup context for decryption
	    if (1 != EVP_DecryptInit(&c->ctx, cipher,
			c->auxbuf->data, c->ivbuf->data)) {
		///error: EVP_DecryptInit failed
		return 0;
	    }
	    HMAC_Update(&c->macctx, c->ivbuf->data, c->ivbuf->len);
	    byte_string_clear(c->ivbuf);

	    //now auxbuf holds the last md_length bytes of ciphertext
	    byte_string_reinit(c->auxbuf, md_length);
	    c->count = 0;

	    vin = &in[l];
	    vinl = inl - l;
	    if (!vinl) return 1;
	}
    } else {
	vin = in;
	vinl = inl;
    }

    if (1) { //assert(c->state);
	//auxbuf holds the last md_length bytes received so far
	int flushcount = c->count + vinl - md_length;
	if (flushcount > 0) {
	    if (c->count <= flushcount) {
		//full buffer flush
		if (c->count) {
		    if (1 != crypto_decrypt_advance(c, &vout, outl, c->auxbuf->data, c->count)) return 0;
		}
		flushcount -= c->count;
		//and we may be able to decrypt some more
		if (flushcount > 0) {
		    if (1 != crypto_decrypt_advance(c, &vout, outl, vin, flushcount)) return 0;
		}
		//lastly we refill auxbuf
		memcpy(c->auxbuf->data, &vin[flushcount], md_length);
		c->count = md_length;
	    } else {
		//partial buffer flush
		if (1 != crypto_decrypt_advance(c, &vout, outl, c->auxbuf->data, flushcount)) return 0;

		//move unflushed part to beginning
		memmove(c->auxbuf->data, &c->auxbuf->data[flushcount],
			c->count - flushcount);
		c->count -= flushcount;

		//keep filling auxbuf
		memcpy(&c->auxbuf->data[c->count], vin, vinl);
		c->count += vinl;
	    }
	} else {
	    //keep filling auxbuf
	    memcpy(&c->auxbuf->data[c->count], vin, vinl);
	    c->count += vinl;
	}

    }
    return 1;
}

int crypto_decrypt_final(crypto_ctx_t c, unsigned char *out, int *outl)
{
    byte_string_t mac;

    if (c->count != md_length) {
	//error: ctext was too short to contain MAC!
	printf("ctext was too short to contain MAC!\n");
	return 0;
    }

    byte_string_init(mac, md_length);
    HMAC_Final(&c->macctx, mac->data, &mac->len);

    if (byte_string_cmp(mac, c->auxbuf)) {
	//error: MAC mismatch
	printf("MAC mismatch!\n");
	byte_string_clear(mac);
	return 0;
    }
    byte_string_clear(mac);
    byte_string_clear(c->auxbuf);

    if (1 != EVP_DecryptFinal(&c->ctx, out, outl)) {
	//error: EVP_DecryptFinal failed
	return 0;
    }
    return 1;
}

int crypto_decrypt(byte_string_t ptext, byte_string_t ctext,
	byte_string_t secret)
//decrypt short messages that fit in memory
{
    int i;
    crypto_ctx_t c;

    byte_string_init(ptext, ctext->len + crypto_block_size());

    crypto_ctx_init(c);
    if (1 != crypto_decrypt_init(c, secret)) {
	//error: crypto_decrypt_init failed	
	byte_string_clear(ptext);
	crypto_ctx_clear(c);
	return 0;
    }

    if (1 != crypto_decrypt_update(c, ptext->data, &i, ctext->data, ctext->len)) {
	//error: crypto_decrypt_update failed	
	byte_string_clear(ptext);
	crypto_ctx_clear(c);
	return 0;
    }

    if (1 != crypto_decrypt_final(c, &ptext->data[i], &ptext->len)) {
	//error: crypto_decrypt_final failed	
	printf("crypto_decrypt_final failed\n");
	byte_string_clear(ptext);
	crypto_ctx_clear(c);
	return 0;
    }
    crypto_ctx_clear(c);

    ptext->len += i;
    byte_string_reinit(ptext, ptext->len);

    return 1;
}

void mympz_get_bn(mpz_t z, BIGNUM *bn)
//bn = z
{
    int i;

    BN_zero(bn);
    i = mpz_sizeinbase(z, 2) - 1;
    while (i>=0) {
	if (mpz_tstbit(z, i)) BN_set_bit(bn, i);
	i--;
    }
}

void mympz_set_bn(mpz_t z, BIGNUM *bn)
//z = bn
{
    int i;

    mpz_set_ui(z, 0);
    i = BN_num_bits(bn) - 1;
    while (i>=0) {
	if (BN_is_bit_set(bn, i)) mpz_setbit(z, i);
	i--;
    }
}

void mympz_randomm(mpz_t x, mpz_t limit)
//x = random in {0, ..., limit - 1}
{
    BIGNUM *bnx, *bnl;
    bnx = BN_new();
    bnl = BN_new();
    mympz_get_bn(limit, bnl);
    BN_rand_range(bnx, bnl);
    mympz_set_bn(x, bnx);
    BN_free(bnx);
    BN_free(bnl);
}

void mympz_randomb(mpz_t x, int bits)
{
    BIGNUM *bnx;
    bnx = BN_new();
    BN_rand(bnx, bits, 0, 0);
    mympz_set_bn(x, bnx);
    BN_free(bnx);
}

int crypto_rand_bytes(unsigned char *r, int len)
{
    //could read from /dev/random instead
    return RAND_bytes(r, len);
}

int crypto_block_size()
{
    return EVP_CIPHER_block_size(cipher) + iv_length + md_length;
}
