/* for testing; extracts a key share given a server's master share
 * (users should never have access to any master share)
 * also constructs any private key given
 * the share files (which in real life would never be together in one place)
 * Ben Lynn
 */

/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/

#include <stdlib.h>
#include <openssl/evp.h>
#include "format.h"
#include "ibe_progs.h"

int extract_share(int argc, char **argv)
{
    char *id;
    char *sfile;
    byte_string_t mshare, kshare;

    if (argc < 3) {
	fprintf(stderr, "Usage: extract_share ID SHAREFILE\n");
	return 1;
    }

    id = FMT_make_id(argv[1], NULL, params);

    sfile = argv[2];
    FMT_load_byte_string(sfile, mshare);

    IBE_extract_share(kshare, mshare, id, params);

    FMT_crypt_save_fp(stdout, kshare, "password");

    return 0;
}

int extract(int argc, char **argv)
{
    char *id;
    int count = IBE_threshold(params);
    char **sfiles;
    int i;
    char *privkeyfile;
    byte_string_t key;
    byte_string_t master;

    if (argc < 3) {
	fprintf(stderr, "Usage: extract ID SHAREFILES...\n");
	return 1;
    }

    if (argc - 2 < count) {
	fprintf(stderr, "not enough pieces to make a private key\n");
	return 1;
    }

    id = FMT_make_id(argv[1], NULL, params);

    sfiles = (char **) malloc(count * sizeof(char*));
    for (i=0; i<count; i++) {
	sfiles[i] = argv[i+2];
    }

    privkeyfile = GetStringParam(cnfctx, "keyfile", 0, "keyfile");

    if (!FMT_construct_master(master, sfiles, count, params)) {
	fprintf(stderr, "at least one bad share file!\n");
	return 1;
    }
    IBE_extract(key, master, id, params);
    byte_string_fprintf(stdout, key, " %02X");

    FMT_crypt_save(privkeyfile, key, "password");

    byte_string_clear(master);
    byte_string_clear(key);

    return 0;
}


int fixed_password_decrypt(int argc, char **argv)
{
    char *privkeyfile = GetStringParam(cnfctx, "keyfile", 0, "keyfile");
    byte_string_t key;
    int status;
    char *id;

    if (argc < 2) {
	printf("Usage: fixed_password_decrypt ID\n\n");
	printf("Decrypts stdin and prints on stdout\n");
	printf("Reads private key from file specified in config file\n");
	printf("Assumes file has been encrypted with \"password\"\n");
	return 1;
    }
    id = FMT_make_id(argv[1], NULL, params);

    status = FMT_crypt_load(privkeyfile, key, "password");
    if (status != 1) {
	fprintf(stderr, "error loading private key %s\n", privkeyfile);
	return 1;
    }

    FMT_decrypt_stream(id, key, stdin, stdout, params);

    byte_string_clear(key);
    return 0;
}
