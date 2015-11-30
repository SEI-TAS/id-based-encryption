/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/

#include <string.h>
#include "ibe.h"
#include "format.h"
#include "crypto.h"

enum {
    crypt_buf_size = 100
};

char *FMT_get_year(void)
{
    time_t tt;
    struct tm *tmt;
    char *s;
    int year;

    time(&tt);
    tmt = gmtime(&tt);
    s = (char *) malloc(5);

    year = 1900 + tmt->tm_year;
    if (year > 9999) year = 9999;
    //Y10K bug: need to fix eventually =]

    sprintf(s, "%d", year);
    return s;
}

char *FMT_make_id(char *addr, char *subject, params_t params)
{
    char *year = FMT_get_year();
    int i;
    char *s;

    if (!addr) {
	return NULL;
    }

    //XXX: yet another magic number
    i = strlen(addr) + strlen(IBE_system(params)) + 1000;

    if (subject) i += strlen(subject);
    s = (char *) malloc(i);

    i = sprintf(s, "purpose=email&date=%s&id=%s&", year, addr);
    if (subject) {
	i += sprintf(&s[i], "subject=%s&", subject);
    }
    sprintf(&s[i], "system=%s", IBE_system(params));

    free(year);
    return s;
}

static void advance_to(char *s, FILE *infp)
{
    char line[crypt_buf_size];

    for (;;) {
	fgets(line, crypt_buf_size, infp);
	if (feof(infp)) break;
	if (!strncmp(s, line, strlen(s))) break;
    }
}

static void mime_put(byte_string_t bs, FILE *outfp)
{
    char out[1024];
    int outl;
    int i, l;

    EVP_ENCODE_CTX ctx;

    EVP_EncodeInit(&ctx);

    for (i=0; i<bs->len; i+=l) {
	if (i+crypt_buf_size > bs->len) {
	    l = bs->len - i;
	} else {
	    l = crypt_buf_size;
	}
	EVP_EncodeUpdate(&ctx,out,&outl,&bs->data[i],l);
	fwrite(out, 1, outl, outfp);
    }
    EVP_EncodeFinal(&ctx,out,&outl);
    fwrite(out, 1, outl, outfp);
}

static void mime_get(byte_string_t bs, FILE *infp)
{
    char line[crypt_buf_size];
    int l, l2;
    EVP_ENCODE_CTX mime;

    byte_string_init(bs, 1024);
    EVP_DecodeInit(&mime);
    l = 0;

    for (;;) {
	fgets(line, crypt_buf_size, infp);
	if (feof(infp)) break;
	//break on blank line = "\n" or "\r\n"
	if (strlen(line) <= 2) break;
	EVP_DecodeUpdate(&mime, &bs->data[l], &l2, (unsigned char *) line, strlen(line));
	l += l2;
	if (bs->len - l < crypt_buf_size) {
	    byte_string_reinit(bs, bs->len * 2);
	}
    }
    EVP_DecodeFinal(&mime, &bs->data[l], &l2);
    byte_string_reinit(bs, l + l2);
}

void FMT_encrypt_stream_array(char **id, int idcount,
	FILE *infp, FILE *outfp, params_t params)
{
    byte_string_t U, K;
    byte_string_t *V;
    unsigned char in[crypt_buf_size];
    unsigned char *out;
    int inl, outl;
    crypto_ctx_t ctx;
    EVP_ENCODE_CTX mime;
    unsigned char data[1024];
    int i;
    int count;

    out = (unsigned char *) alloca(crypt_buf_size + crypto_block_size());

    V = (byte_string_t *) alloca(sizeof(byte_string_t) * idcount);

    fprintf(outfp, "\n-----BEGIN IBE-----\n");

    crypto_generate_key(K);

    IBE_hide_key_array(U, V, id, idcount, K, params);
    fprintf(outfp, "\nU:\n");
    mime_put(U, outfp);
    fprintf(outfp, "\n");

    for (i=0; i<idcount; i++) {
	fprintf(outfp, "\nID:\n");
	fprintf(outfp, "%s\n", id[i]);

	fprintf(outfp, "\nV:\n");
	mime_put(V[i], outfp);
	fprintf(outfp, "\n");

	byte_string_clear(V[i]);
    }

    fprintf(outfp, "\nW:\n");
    crypto_ctx_init(ctx);
    crypto_encrypt_init(ctx, K);
    EVP_EncodeInit(&mime);
    for (;;) {
	inl = fread(in, 1, crypt_buf_size, infp);
	if (inl < 0) {
	    fprintf(stderr, "read error\n");
	    exit(1);
	}
	crypto_encrypt_update(ctx, out, &outl, in, inl);
	EVP_EncodeUpdate(&mime, data, &count, out, outl);
	fwrite(data, 1, count, outfp);
	if (feof(infp)) break;
    }
    crypto_encrypt_final(ctx, out, &outl);
    crypto_ctx_clear(ctx);
    EVP_EncodeUpdate(&mime, data, &count, out, outl);
    fwrite(data, 1, count, outfp);
    EVP_EncodeFinal(&mime, data, &count);
    fwrite(data, 1, count, outfp);

    fprintf(outfp, "\n-----END IBE-----\n");

    byte_string_clear(K);
    byte_string_clear(U);
}

void FMT_encrypt_stream(char *id, FILE *infp, FILE *outfp, params_t params)
{
    FMT_encrypt_stream_array(&id, 1, infp, outfp, params);
}

int FMT_decrypt_stream(char *id, byte_string_t key,
	FILE *infp, FILE *outfp, params_t params)
{
    byte_string_t U;
    byte_string_t K, V;
    crypto_ctx_t ctx;
    unsigned char in[crypt_buf_size];
    unsigned char out[200]; //TODO: what should this be?
    int inl, outl;
    unsigned char data[1024];
    int count;
    EVP_ENCODE_CTX mime;
    int result = 0;
    char *s, slen;
    int status;

    advance_to("-----BEGIN IBE-----", infp);

    advance_to("U:", infp);
    mime_get(U, infp);

    slen = strlen(id) + 2;
    s = (char *) alloca(sizeof(char) * slen);
    for(;;) {
	advance_to("ID:", infp);
	if (feof(infp)) {
	    //ID not found
	    return 0;
	}
	fgets(s, slen, infp);
	if (s[strlen(id)] == '\n') { //correct length?
	    if (!strncmp(s, id, strlen(id))) { //compares?
		break; //email has ID for us
	    }
	}
    }

    advance_to("V:", infp);
    mime_get(V, infp);

    status = IBE_reveal_key(K, U, V, key, params);

    if (status != 1) {
	fprintf(outfp, "WARNING: KMAC MISMATCH. INVALID CIPHERTEXT!\n");
	byte_string_clear(V);
	byte_string_clear(K);
	return result;
    }

    advance_to("W:", infp);

    crypto_ctx_init(ctx);
    crypto_decrypt_init(ctx, K);
    EVP_DecodeInit(&mime);
    for (;;) {
	fgets(in, crypt_buf_size, infp);
	inl = strlen(in);
	if (inl < 0) {
	    fprintf(stderr, "read error\n");
	    exit(1);
	}
	if (inl < 2) break;
	EVP_DecodeUpdate(&mime, data, &count, in, inl);
	crypto_decrypt_update(ctx, out, &outl, data, count);
	fwrite(out, 1, outl, outfp);
    }
    EVP_DecodeFinal(&mime, data, &count);
    crypto_decrypt_update(ctx, out, &outl, data, count);
    fwrite(out, 1, outl, outfp);
    if (1 != crypto_decrypt_final(ctx, out, &outl)) {
	fprintf(outfp, "crypto_decrypt_final() failed!\n");
    } else {
	result = 1;
    }
    fwrite(out, 1, outl, outfp);
    crypto_ctx_clear(ctx);

    byte_string_clear(K);
    byte_string_clear(U);
    byte_string_clear(V);

    return result;
}

int FMT_crypt_save_fp(FILE *outfp, byte_string_t bs, const char *password)
{
    byte_string_t K, W;

    byte_string_set(K, password);
    crypto_encrypt(W, bs, K);
    fprintf(outfp, "\nW:\n");
    mime_put(W, outfp);

    byte_string_clear(K);
    byte_string_clear(W);

    return 1;
}

int FMT_crypt_save(const char *fname, byte_string_t bs, const char *password)
{
    FILE *fp;
    int status;

    fp = fopen(fname, "w");
    if (!fp) return 0;
    status = FMT_crypt_save_fp(fp, bs, password);

    fclose(fp);

    return status;
}

int FMT_crypt_load(const char *fname, byte_string_t bs, const char *password)
{
    FILE *infp;
    byte_string_t K, W;
    int result = 0;

    infp = fopen(fname, "r");
    if (!infp) return 0;

    byte_string_set(K, password);

    advance_to("W:", infp);
    mime_get(W, infp);

    if (1 != crypto_decrypt(bs, W, K)) {
	fprintf(stderr, "crypto_decrypt() failed!\n");
    } else {
	result = 1;
    }

    fclose(infp);

    byte_string_clear(K);
    byte_string_clear(W);
    return result;
}

int FMT_load_byte_string(const char *filename, byte_string_t bs)
{
    FILE *fp;

    fp = fopen(filename, "r");
    if (!fp) return 0;

    mime_get(bs, fp);
    fclose(fp);

    return 1;
}

int FMT_save_byte_string(const char *filename, byte_string_t bs)
{
    FILE *fp;
    fp = fopen(filename, "w");
    if (!fp) return 0;

    mime_put(bs, fp);

    fclose(fp);
    return 1;
}

int FMT_load_raw_byte_string(byte_string_t bs, char *file)
//load a raw byte string from a file
{
    int max = 1024;
    int len = 0;
    FILE *fp;

    fp = fopen(file, "r");
    if (!fp) {
	return 0;
    }
    byte_string_init(bs, max);

    for(;;) {
	len += fread(&bs->data[len], 1, max - len, fp);
	if (len == max) {
	    max *= 2;
	    byte_string_reinit(bs, max);
	} else {
	    if (!feof(fp)) {
		fclose(fp);
		byte_string_clear(bs);
		return 0;
	    }
	    fclose(fp);
	    break;
	}
    }
    byte_string_reinit(bs, len);
    return 1;
}

int FMT_split_master(char **fname, byte_string_t master, int t, int n, params_t params)
{
    int i;
    byte_string_t mshare[n];
    int result = 1;

    IBE_split_master(mshare, master, t, n, params);
    for (i=0; i<n; i++) {
	if (1 != FMT_save_byte_string(fname[i], mshare[i])) {
	    fprintf(stderr, "error writing to %s\n", fname[i]);
	    result = 0;
	    break;
	}
    }

    for (i=0; i<n; i++) {
	byte_string_clear(mshare[i]);
    }

    return result;
}

int FMT_construct_master(byte_string_t master,
	char **fname, int t, params_t params)
{
    int i;
    byte_string_t mshare[t];

    for (i=0; i<t; i++) {
	if (1 != FMT_load_byte_string(fname[i], mshare[i])) {
	    fprintf(stderr, "error loading %s\n", fname[i]);
	    return 0;
	}
    }
    IBE_construct_master(master, mshare, params);

    for (i=0; i<t; i++) {
	byte_string_clear(mshare[i]);
    }

    return 1;
}

int FMT_save_params(char *filename, params_t params)
{
    int result;
    byte_string_t bs;

    IBE_serialize_params(bs, params);

    result = FMT_save_byte_string(filename, bs);

    byte_string_clear(bs);
    return result;
}

int FMT_load_params(params_t params, char *filename)
{
    byte_string_t bs;

    int result;

    result = FMT_load_byte_string(filename, bs);

    if (!result) {
	return 0;
    }

    result = IBE_deserialize_params(params, bs);

    byte_string_clear(bs);
    return result;
}

