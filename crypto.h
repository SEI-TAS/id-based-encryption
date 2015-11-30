/* crypto.h
 * Ben Lynn
 */

/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/

#ifndef CRYPTO_H
#define CRYPTO_H
#include "byte_string.h"
#include <gmp.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

struct crypto_ctx_s {
    EVP_CIPHER_CTX ctx;
    HMAC_CTX macctx;
    int state;
    byte_string_t auxbuf;
    byte_string_t ivbuf;
    int count;
};

typedef struct crypto_ctx_s crypto_ctx_t[1];
typedef struct crypto_ctx_s *crypto_ctx_ptr;

void crypto_init(void); //call to init crypto library, NOT THREAD-SAFE
void crypto_clear(void);

void crypto_ctx_init(crypto_ctx_t c);
void crypto_ctx_clear(crypto_ctx_t c);

//If something returns an int, in general: 1 = no error, other value = error

//These encryption/decryption routines automatically do the IV's and MAC's
//Can be used as blackbox IND-CCA ciphers
int crypto_encrypt_init(crypto_ctx_t c, byte_string_t secret);
int crypto_encrypt_update(crypto_ctx_t c, unsigned char *out, int *outl,
	unsigned char *in, int inl);
int crypto_encrypt_final(crypto_ctx_t c, unsigned char *out, int *outl);
int crypto_encrypt(byte_string_t ctext, byte_string_t ptext,
	byte_string_t secret);
//encrypt short messages that fit in memory

int crypto_decrypt_init(crypto_ctx_t c, byte_string_t secret);
int crypto_decrypt_update(crypto_ctx_t c, unsigned char *out, int *outl,
	unsigned char *in, int inl);
int crypto_decrypt_final(crypto_ctx_t c,
	unsigned char *out, int *outl);

int crypto_decrypt(byte_string_t ptext, byte_string_t ctext,
	byte_string_t secret);
//decrypt short messages that fit in memory

void crypto_hash(byte_string_t out, byte_string_t in);
void crypto_va_hash(byte_string_t md_value, int n, ...);

int crypto_generate_salt(byte_string_t salt);
int crypto_generate_key(byte_string_t key);

int crypto_block_size();

int crypto_rand_bytes(unsigned char *r, int len);
void mympz_randomm(mpz_t x, mpz_t limit);
void mympz_randomb(mpz_t x, int bits);
#endif //CRYPTO_H
