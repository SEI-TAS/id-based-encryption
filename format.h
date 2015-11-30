/* header file for format.cc
 * Ben Lynn
 */
/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/
#ifndef FORMAT_H
#define FORMAT_H

#include <stdio.h>
#include "ibe.h"

int FMT_load_params(params_t params, char *filename);
//load system parameters

int FMT_save_params(char *filename, params_t params);
//save system parameters

int FMT_split_master(char **fname, byte_string_t master, int t, int n, params_t params);
//split the master secret into n files
//(t are needed to recover it)

int FMT_construct_master(byte_string_t master,
	char **fname, int t, params_t params);
//construct master from secret shares
//(shouldn't be used since shares shouldn't be together)

int FMT_crypt_save_fp(FILE *fp, byte_string_t bs, const char *password);
int FMT_crypt_save(const char *fname, byte_string_t bs, const char *password);
//save a byte string to an encrypted file
int FMT_crypt_load(const char *fname, byte_string_t bs, const char *password);
//load a byte string from an encrypted file

int FMT_load_byte_string(const char *filename, byte_string_t bs);
//load a encoded byte-string

int FMT_load_raw_byte_string(byte_string_t bs, char *file);
//load a raw byte string from a file

char *FMT_make_id(char *addr, char *subject, params_t params);
//adds fields to email address to make it a valid ID

void FMT_encrypt_stream(char *id, FILE *infp, FILE *outfp, params_t params);
int FMT_decrypt_stream(char *id, byte_string_t key,
	FILE *infp, FILE *outfp, params_t params);
void FMT_encrypt_stream_array(char **id, int idcount,
	FILE *infp, FILE *outfp, params_t params);

#endif //FORMAT_H
