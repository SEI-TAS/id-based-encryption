	byte_string_t M, char **idarray);
//idarray is a null-terminated array of ID's
//
//See LICENSE for license
//<U,V,W> = encrypt(M)
//(U and W are the same for every id)

int IBE_decrypt(byte_string_t M, byte_string_t U, byte_string_t V,
	byte_string_t W, byte_string_t key);
//M = decrypt(<U,V,W>)

int IBE_authenticated_decrypt(byte_string_t M, byte_string_t U, byte_string_t V,
	byte_string_t W, byte_string_t d, char *sender);

int IBE_authenticated_encrypt(byte_string_t U, byte_string_t *Varray, byte_string_t W,
	byte_string_t M, char **idarray,
	byte_string_t sender_key);

//these are called by encrypt and decrypt
//use these if you want to write your own encrypt/decrypt
//sigma is the key that is actually used to encrypt the plaintext
void IBE_hidesigma(byte_string_t U, byte_string_t *Varray,
    byte_string_t sigma, byte_string_t hash,
    char **idarray);
//encrypt a key sigma (which must be n bits long)
//output in U, Varray
//must be given the hash of the plaintext message,
//and idarray holds the public keys to encrypt for
//requires Varray has same length has idarray

void IBE_revealsigma(byte_string_t sigma,
	byte_string_t U, byte_string_t V, byte_string_t key);
//reveal a key sigma
//given U, V, and a private key

int IBE_verify(byte_string_t U, byte_string_t sigma, byte_string_t hash);
//verify that the ciphertext is valid
//given U, a key sigma, and the hash of the plaintext

void IBE_revealsigma_preprocess(preprocessed_key_t pk, byte_string_t key);
void IBE_revealsigma_postprocess(byte_string_t sigma,
	byte_string_t U, byte_string_t V, preprocessed_key_t pk);

//DEPRECATED: headers from format.h
int FMT_encrypt(FILE *outfp, byte_string_t M, char **idarray);
//encrypts message from a buffer for an array of ID's, writes to a file pointer

int FMT_decrypt(byte_string_t M, FILE *infp, byte_string_t key);

//DEPRECATED: from format.c
static void mime_encode(byte_string_t R, byte_string_t M)
//R = encoded(M)
{
    int outl, l;
    EVP_ENCODE_CTX ctx;

    byte_string_init(R, 10 * M->len);

    EVP_EncodeInit(&ctx);
    EVP_EncodeUpdate(&ctx,R->data,&outl,M->data,M->len);
    EVP_EncodeFinal(&ctx,&R->data[outl],&l);
    outl+=l;
    byte_string_reinit(R, outl);
}

int FMT_output_uvw(FILE *stream, char *ver, byte_string_t U,
	byte_string_t *Varray, byte_string_t W, char **idarray)
{
    int count;
    byte_string_t bstmp;

    FILE *fp = stream;
    if (!fp) {
	fp = stdout;
    }

    fprintf(fp, "Version: %s\r\n", ver);
    mime_encode(bstmp, U);
    fprintf(fp, "U: ");
    byte_string_fprintf(fp, bstmp, "%c");
    byte_string_clear(bstmp);
    fprintf(fp, "\r\n");
    for (count=0; idarray[count]; count++) {
	fprintf(fp, "ID: %s\r\n", idarray[count]);

	fprintf(fp, "V: ");
	mime_encode(bstmp, Varray[count]);
	byte_string_fprintf(fp, bstmp, "%c");
	byte_string_clear(bstmp);
	fprintf(fp,"\r\n");
    }
    fprintf(fp, "W: ");
    mime_encode(bstmp, W);
    byte_string_fprintf(fp, bstmp, "%c");
    byte_string_clear(bstmp);
    fprintf(fp, "\r\n");

    return 1;
}

int FMT_encrypt(FILE *outfp, byte_string_t M, char **idarray)
{
    byte_string_t U;
    byte_string_t *Varray;
    byte_string_t W;
    int i, count;
    int status;

    for (count=0; idarray[count]; count++);
    if (count == 0) return 0;

    Varray = (byte_string_t *) malloc(count * sizeof(byte_string_t));

    IBE_encrypt(U, Varray, W, M, idarray);

    status = FMT_output_uvw(outfp, IBE_version(), U, Varray, W, idarray);

    byte_string_clear(U);
    for (i=0; i<count; i++) {
	byte_string_clear(Varray[i]);
    }
    free(Varray);
    byte_string_clear(W);

    return status;
}

int FMT_decrypt(byte_string_t M, FILE *infp, byte_string_t key)
{
    //XXX:possible buffer overflows
    //needs to be rewritten
    byte_string_t U, V, W;
    char line[100];
    int status;

    byte_string_init(U, 1000);
    byte_string_init(V, 100);
    byte_string_init(W, 10000);

    for(;;) {
	fgets(line, 100, infp);
	if (feof(infp)) {
	    return(0);
	}
	if (!strncmp(line, "U: ", 3)) {
	    char *s = &line[3];
	    int l, l2;
	    EVP_ENCODE_CTX ctx;
	    EVP_DecodeInit(&ctx);
	    l = 0;
	    for (;;) {
		EVP_DecodeUpdate(&ctx, &U->data[l], &l2, (unsigned char *) s, strlen(s));
		l += l2;
		fgets(line, 100, infp);
		s = line;
		if (feof(infp) || strlen(s) < 5) break;
	    }
	    EVP_DecodeFinal(&ctx, &U->data[l], &l2);
	    U->len = l + l2;
	} else if (!strncmp(line, "V: ", 3)) {
	    char *s = &line[3];
	    int l, l2;
	    EVP_ENCODE_CTX ctx;
	    EVP_DecodeInit(&ctx);
	    EVP_DecodeUpdate(&ctx, V->data, &l, (unsigned char *) s, strlen(s));
	    EVP_DecodeFinal(&ctx, &V->data[l], &l2);
	    V->len = l + l2;
	    if (V->len!=20) {
		return(0);
	    }
	} else if (!strncmp(line, "W: ", 3)) {
	    char *s = &line[3];
	    int l, l2;
	    EVP_ENCODE_CTX ctx;
	    EVP_DecodeInit(&ctx);
	    l = 0;
	    for (;;) {
		EVP_DecodeUpdate(&ctx, &W->data[l], &l2, (unsigned char *) s, strlen(s));
		l += l2;
		fgets(line, 100, infp);
		s = line;
		if (feof(infp) || strlen(s) < 5) break;
	    }
	    EVP_DecodeFinal(&ctx, &W->data[l], &l2);
	    W->len = l + l2;
	    break;
	}
    }

    status = IBE_decrypt(M, U, V, W, key);
    if (status != 1) {
	fprintf(stderr, "error in decryption\n");
    }
    return(status);
}

//DEPRECATED: from ibe_lib.c
//see "Authenticated Identity-Based Encryption" paper on eprint
//(no need to use Fujisaki-Okamoto constructions in real life)

void IBE_authenticated_hidesigma(byte_string_t U, byte_string_t *Varray,
    byte_string_t sigma, byte_string_t hash,
    char **idarray,
    byte_string_t sender_key)
//encrypt a key sigma (which must be n bits long)
//output in U, Varray
//must be given the hash of the plaintext message,
//and idarray holds the public keys to encrypt for
//requires Varray has same length has idarray
{
    int i, j;
    int count;
    int n = params.n;
    mpz_t r;
    fp2_t s;
    point_t Qid;
    point_t PhixQ;
    byte_string_t md_value;

    byte_string_t tempbs, tempbs2;

    for (count=0; idarray[count]; count++);
    if (count == 0) return;

    //r = H1(sigma, M)
    mpz_init(r);
    hash_H1(r, sigma, hash);
    bm_put(bm_get_time(), "rP0");
    bm_put(bm_get_time(), "rP1");
    //no need to work out rP any more!

    //U = r
    byte_string_set_mpz(U, r);

    point_init(Qid);

    point_init(PhixQ);
    point_set_byte_string(PhixQ, sender_key);
    point_Phi(PhixQ, PhixQ);

    byte_string_init(md_value, n);

    for (i=0; i<count; i++) {
	const char *id = idarray[i];

	//XXX:set up a cache to avoid these expensive ops
	map_to_point(Qid, id);
	//calculate s = e(Q_id, Phi(xQ))
	tate_pairing(s, Qid, PhixQ);

	bm_put(bm_get_time(), "gidr0");
	//second part of cipher text: V = sigma xor H(U, s)

	byte_string_set_fp2(tempbs2, s);
	byte_string_join(tempbs, U, tempbs2);
	byte_string_clear(tempbs2);
	hash_G1(md_value, tempbs);
	byte_string_clear(tempbs);

	byte_string_init(Varray[i], n);
	for (j=0; j<n; j++) {
	    Varray[i]->data[j] = md_value->data[j] ^ sigma->data[j];
	}

	bm_put(bm_get_time(), "gidr1");
    }
    byte_string_clear(md_value);

    point_clear(Qid);
    mpz_clear(r);
    fp2_clear(s);
}


void IBE_hidesigma(byte_string_t U, byte_string_t *Varray,
    byte_string_t sigma, byte_string_t hash,
    char **idarray)
//encrypt a key sigma (which must be n bits long)
//output in U, Varray
//must be given the hash of the plaintext message,
//and idarray holds the public keys to encrypt for
//requires Varray has same length has idarray
{
    int i, j;
    int count;
    int n = params.n;
    mpz_t r;
    fp2_t gidr;
    point_t Qid;
    point_t rP;
    byte_string_t md_value;

    for (count=0; idarray[count]; count++);
    if (count == 0) return;

    //r = H1(sigma, M)
    mpz_init(r);
    hash_H1(r, sigma, hash);
    bm_put(bm_get_time(), "rP0");

    //U = rP
    point_init(rP);
    //point_mul(rP, r, params.P);
    point_mul_postprocess(rP, r);
    bm_put(bm_get_time(), "rP1");
    fp2_init(gidr);

    byte_string_set_point(U, rP);

    point_clear(rP);

    point_init(Qid);

    byte_string_init(md_value, n);

    for (i=0; i<count; i++) {
	const char *id = idarray[i];

	//XXX:set up a cache to avoid these expensive ops
	map_to_point(Qid, id);
	point_Phi(Qid, Qid);
	
	//calculate gidr = e(Q_id, Phi(P_pub))^r

	//tate_pairing(gidr, Qid, PhiPpub);
	tate_postprocess(gidr, Ppub_mc, Qid);

	bm_put(bm_get_time(), "gidr0");
	fp2_pow(gidr, gidr, r);
	bm_put(bm_get_time(), "gidr1");

	//second part of cipher text = sigma xor H(g_id^r)
	hash_H(md_value, gidr);
	byte_string_init(Varray[i], n);
	for (j=0; j<n; j++) {
	    Varray[i]->data[j] = md_value->data[j] ^ sigma->data[j];
	}
    }
    byte_string_clear(md_value);

    point_clear(Qid);
    mpz_clear(r);
    fp2_clear(gidr);
}

int IBE_authenticated_encrypt(byte_string_t U, byte_string_t *Varray, byte_string_t W,
	byte_string_t M, char **idarray,
	byte_string_t sender_key)
{
    int n = params.n;
    byte_string_t sigma, G1sigma;
    byte_string_t md_value;

    bm_put(bm_get_time(), "enc0");

    //pick random sigma in {0,1}^n
    byte_string_init(sigma, n);
    crypto_rand_bytes(sigma->data, n);

    //Hash message
    byte_string_init(md_value, n);
    crypto_hash(md_value, M);

    IBE_authenticated_hidesigma(U, Varray, sigma, md_value, idarray, sender_key);

    byte_string_clear(md_value);

    //G1(sigma) is the key we use to encrypt
    byte_string_init(G1sigma, n);
    hash_G1(G1sigma, sigma);
    byte_string_clear(sigma);

    crypto_encrypt(W, M, G1sigma);
    byte_string_clear(G1sigma);

    bm_put(bm_get_time(), "enc1");
    bm_report_encrypt();

    return 1;
}

int IBE_encrypt(byte_string_t U, byte_string_t *Varray, byte_string_t W,
	byte_string_t M, char **idarray)
//only works with short messages
{
    int n = params.n;
    byte_string_t sigma, G1sigma;
    byte_string_t md_value;

    bm_put(bm_get_time(), "enc0");

    //pick random sigma in {0,1}^n
    byte_string_init(sigma, n);
    crypto_rand_bytes(sigma->data, n);

    byte_string_init(md_value, n);
    crypto_hash(md_value, M);

    IBE_hidesigma(U, Varray, sigma, md_value, idarray);

    byte_string_clear(md_value);

    //G1(sigma) is the key we use to encrypt
    byte_string_init(G1sigma, n);
    hash_G1(G1sigma, sigma);
    byte_string_clear(sigma);

    crypto_encrypt(W, M, G1sigma);

    byte_string_clear(G1sigma);
    
    bm_put(bm_get_time(), "enc1");
    bm_report_encrypt();

    return 1;
}

void IBE_authenticated_revealsigma(byte_string_t sigma,
	byte_string_t U, byte_string_t V, byte_string_t key, char *sender)
//reveal a key sigma
//given U, V, and a private key
{
    int i;
    int n = params.n;
    point_t xQ, PhiQ2;
    fp2_t s;
    byte_string_t md_value;
    byte_string_t tempbs, tempbs2;

    byte_string_init(md_value, n);

    byte_string_init(sigma, n);
    fp2_init(s);

    point_init(xQ);
    point_init(PhiQ2);

    point_set_byte_string(xQ, key);

    //XXX:cache this
    //calculate s = e(xQ, Phi(Q2)), where Q2 = public key of sender
    map_to_point(PhiQ2, sender);
    point_Phi(PhiQ2, PhiQ2);
    tate_pairing(s, xQ, PhiQ2);

    //compute sigma = V xor H(U,s)
    byte_string_set_fp2(tempbs2, s);
    byte_string_join(tempbs, U, tempbs2);
    hash_G1(md_value, tempbs);
    byte_string_clear(tempbs2);
    byte_string_clear(tempbs);

    for (i=0; i<n; i++) {
	sigma->data[i] = V->data[i] ^ md_value->data[i];
    }
    byte_string_clear(md_value);

    fp2_clear(s);
    point_clear(xQ);
    point_clear(PhiQ2);
}

void IBE_revealsigma_preprocess(preprocessed_key_t pk, byte_string_t key)
//speeds up miller() by caching stuff
{
    point_t xQ;
    point_init(xQ);
    point_set_byte_string(xQ, key);
    tate_preprocess(pk->mc, xQ);
}

void IBE_revealsigma_postprocess(byte_string_t sigma,
	byte_string_t U, byte_string_t V, preprocessed_key_t pk)
//reveal a key sigma
//given U, V, and a private key
{
    int i;
    int n = params.n;
    point_t rP;
    fp2_t res;
    byte_string_t md_value;

    byte_string_init(sigma, n);
    fp2_init(res);

    point_init(rP);

    point_set_byte_string(rP, U);

    //Compute sigma = V xor H(e(Pkey, Phi(U)))
    point_Phi(rP, rP);
    tate_postprocess(res, pk->mc, rP);
    byte_string_init(md_value, n);
    hash_H(md_value, res);
    for (i=0; i<n; i++) {
	sigma->data[i] = V->data[i] ^ md_value->data[i];
    }
    byte_string_clear(md_value);

    fp2_clear(res);
    point_clear(rP);
}

void IBE_revealsigma(byte_string_t sigma,
	byte_string_t U, byte_string_t V, byte_string_t key)
//reveal a key sigma
//given U, V, and a private key
{
    int i;
    int n = params.n;
    point_t xQ, rP;
    fp2_t res;
    byte_string_t md_value;

    byte_string_init(sigma, n);
    fp2_init(res);

    point_init(xQ);
    point_init(rP);

    point_set_byte_string(xQ, key);
    point_set_byte_string(rP, U);

    //Compute sigma = V xor H(e(Pkey, Phi(U)))
    point_Phi(rP, rP);
    tate_pairing(res, xQ, rP);
    byte_string_init(md_value, n);
    hash_H(md_value, res);
    for (i=0; i<n; i++) {
	sigma->data[i] = V->data[i] ^ md_value->data[i];
    }
    byte_string_clear(md_value);

    fp2_clear(res);
    point_clear(xQ);
    point_clear(rP);
}

int IBE_verify(byte_string_t U, byte_string_t sigma, byte_string_t hash)
//verify that the ciphertext is valid
//given U, a key sigma, and the hash of the plaintext
{
    //Set r = H1(sigma, M)
    mpz_t r;
    point_t rP, allegedrP;

    mpz_init(r);
    hash_H1(r, sigma, hash);

    point_init(rP);
    //point_mul(rP, r, params.P);
    point_mul_postprocess(rP, r);
    mpz_clear(r);

    point_init(allegedrP);
    point_set_byte_string(allegedrP, U);

    if (!point_equal(rP, allegedrP)) {
	printf("bad ciphertext: rP != U\n");
	//printf("rP = ");
	//point_out_str(NULL, 0, rP);
	//printf("\n");
	//printf("U = ");
	//point_out_str(NULL, 0, allegedrP);
	//printf("\n");
	point_clear(rP);
	point_clear(allegedrP);
	return 0;
    }
    point_clear(rP);
    point_clear(allegedrP);
    return 1;
}

int IBE_authenticated_decrypt(byte_string_t M, byte_string_t U, byte_string_t V,
	byte_string_t W, byte_string_t d, char *sender)
{
    int n = params.n;
    byte_string_t sigma, G1sigma;
    byte_string_t md_value;

    IBE_authenticated_revealsigma(sigma, U, V, d, sender);

    //Decrypt message with G1sigma as key
    byte_string_init(G1sigma, n);
    hash_G1(G1sigma, sigma);
    byte_string_clear(sigma);
    crypto_decrypt(M, W, G1sigma);
    byte_string_clear(G1sigma);
    
    byte_string_init(md_value, n);
    crypto_hash(md_value, M);

    if (!IBE_verify(U, sigma, md_value)) {
	byte_string_clear(md_value);
	byte_string_clear(sigma);
	printf("Message not valid!\n");
	return 0;
    }

    byte_string_clear(md_value);
    return 1;
}

int IBE_decrypt(byte_string_t M, byte_string_t U, byte_string_t V,
	byte_string_t W, byte_string_t d)
{
    int n = params.n;
    byte_string_t sigma, G1sigma;
    byte_string_t md_value;

    bm_put(bm_get_time(), "dec0");

    IBE_revealsigma(sigma, U, V, d);

    //Decrypt message with G1sigma as key
    byte_string_init(G1sigma, n);
    hash_G1(G1sigma, sigma);

    crypto_decrypt(M, W, G1sigma);
    byte_string_clear(G1sigma);

    byte_string_init(md_value, n);
    crypto_hash(md_value, M);

    if (!IBE_verify(U, sigma, md_value)) {
	byte_string_clear(md_value);
	byte_string_clear(sigma);
	printf("Message not valid!\n");
	return 0;
    }

    byte_string_clear(md_value);
    byte_string_clear(sigma);

    bm_put(bm_get_time(), "dec1");
    bm_report_decrypt();

    return 1;
}

//unused hash functions
void hash_G1(byte_string_t md_value, byte_string_t input)
{
    crypto_hash(md_value, input);
}

void hash_H1(mpz_t r, byte_string_t c1, byte_string_t c2)
{
    byte_string_t md_value;
    crypto_va_hash(md_value, 2, c1, c2);

    mympz_from_hash(r, params.q, md_value);
    byte_string_clear(md_value);
}

//plain encryption and decryption (without MACs) are used in the original
//IBE paper because a Fujisaki-Okamoto transformation is used
void crypto_plain_encrypt(byte_string_t ctext, byte_string_t M, byte_string_t key)
{
    EVP_CIPHER_CTX ctx;
    unsigned char sslkey[EVP_MAX_KEY_LENGTH];
    int saltlen = EVP_CIPHER_iv_length(cipher);
    int newlen;
    int outl;

    byte_string_init(ctext, M->len + EVP_CIPHER_block_size(cipher) + saltlen);

    EVP_BytesToKey(cipher, EVP_md5(), NULL, key->data,
	    key->len, 1, sslkey, NULL);

    if (1 != RAND_bytes(ctext->data, saltlen)) {
	//TODO: warn that random IV failed
	//return 0;
    }

    EVP_EncryptInit(&ctx, cipher, sslkey, ctext->data);
    newlen = saltlen;
    EVP_EncryptUpdate(&ctx, &ctext->data[newlen], &outl, M->data, M->len);
    newlen += outl;
    EVP_EncryptFinal(&ctx, &ctext->data[newlen], &outl);
    newlen += outl;
    byte_string_reinit(ctext, newlen);
}

void crypto_plain_decrypt(byte_string_t M, byte_string_t C, byte_string_t key)
{
    EVP_CIPHER_CTX ctx;
    unsigned char sslkey[EVP_MAX_KEY_LENGTH];
    int saltlen = EVP_CIPHER_iv_length(cipher);
    int mlen;
    int outl;

    EVP_BytesToKey(cipher, EVP_md5(), NULL, key->data,
	    key->len, 1, sslkey, NULL);

    byte_string_init(M, C->len - saltlen + EVP_CIPHER_block_size(cipher));

    EVP_DecryptInit(&ctx, cipher, sslkey, C->data);
    EVP_DecryptUpdate(&ctx, M->data, &mlen, &C->data[saltlen], C->len - saltlen);
    EVP_DecryptFinal(&ctx, &M->data[mlen], &outl);
    mlen += outl;
    byte_string_reinit(M, mlen);
}
