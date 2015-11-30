/* frontend to IBE_decrypt() in ibe_lib.cc
 * (performs decryption)
 * Ben Lynn
 */

/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/

#include <openssl/evp.h>
#include <string.h>
#include "ibe.h"
#include "format.h"
#include "ibe_progs.h"

int decrypt(int argc, char **argv)
{
    char *id;
    char *privkeyfile = GetStringParam(cnfctx, "keyfile", 0, "keyfile");
    byte_string_t key;
    char pw[100];
    int status;

    if (argc < 2) {
	printf("Usage: decrypt ID\n\n");
	printf("Decrypts stdin and prints on stdout\n");
	printf("Reads private key from file specified in config file\n");
	return 1;
    }
    id = FMT_make_id(argv[1], NULL, params);

    status = EVP_read_pw_string(pw, 100, "private key password: ", 1);
    if (status) {
	fprintf(stderr, "error reading private key password\n");
	return 1;
    }

    status = FMT_crypt_load(privkeyfile, key, pw);
    if (status != 1) {
	fprintf(stderr, "error loading private key %s\n", privkeyfile);
	return 1;
    }

    status = FMT_decrypt_stream(id, key, stdin, stdout, params);
    if (status != 1) {
	fprintf(stderr, "error in decryption\n");
	return 1;
    }

    byte_string_clear(key);

    return 0;
}
