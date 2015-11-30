/* frontend to IBE_construct_key() in ibe_lib.c
 * (constructs private key shares into a private key)
 * Ben Lynn
 */

/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/

#include <openssl/evp.h>
#include "format.h"
#include "ibe_progs.h"

int combine(int argc, char **argv)
{
    char **ksfiles;
    int i;
    char *id;
    char s[100];
    char pw[100];
    int status;
    int count = IBE_threshold(params);
    char *privkeyfile = GetStringParam(cnfctx, "keyfile", 0, "keyfile");
    byte_string_t key;
    byte_string_t *kshare;

    if (argc < 3) {
	fprintf(stderr, "Usage: combine ID KEYFILES...\n");
	return 1;
    }

    id = FMT_make_id(argv[1], NULL, params);
    if (argc - 2 < count) {
	fprintf(stderr, "not enough pieces to make a private key\n");
	return 1;
    }

    kshare = (byte_string_t *) malloc(count * sizeof(byte_string_t));
    ksfiles = (char **) malloc(count * sizeof(char *));

    for (i=0; i<count; i++) {
	ksfiles[i] = argv[i+2];

	sprintf(s, "password for %s: ", ksfiles[i]);

	status = EVP_read_pw_string(pw, 100, s, 1);
	if (status) {
	    fprintf(stderr, "error reading password\n");
	    return 1;
	}
	status = FMT_crypt_load(ksfiles[i], kshare[i], pw);
	if (!status) {
	    fprintf(stderr, "error loading %s\n", ksfiles[i]);
	    return 1;
	}
    }

    IBE_combine(key, kshare, params);

    status = EVP_read_pw_string(pw, 100, "private key password: ", 1);
    if (status) {
	fprintf(stderr, "error reading password\n");
	return 1;
    }
    FMT_crypt_save(privkeyfile, key, pw);

    for (i=0; i<count; i++) {
	byte_string_clear(kshare[i]);
    }

    free(kshare);
    free(ksfiles);

    return 0;
}
