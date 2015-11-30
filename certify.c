/* frontend to IBE_certify() in ibe_lib.c
 * (creates certificate)
 * Dan Klinedinst
 */


#include <stdlib.h>
#include <string.h>
#include "ibe_progs.h"
#include "format.h"
#include "ibe.h"
#include "crypto.h"

int certify(int argc, char **argv)
{

    char *id;
    char *public;
    int count = IBE_threshold(params);
    char **sfiles;
    int i;


    byte_string_t master;
    byte_string_t pub;
    byte_string_t cert;

    if (argc < 2) {
	printf("Usage: certify ID SHAREFILES\n\n");
	printf("Creates a certificate for the ID, signed by share\n");
	return 1;
    }


    id = IBE_system(params);

    public = argv[1];
    byte_string_set(pub, public);


    sfiles = (char **) malloc(count * sizeof(char*));
    for (i=0; i<count; i++) {
        sfiles[i] = argv[i+2];
    }


    if (!FMT_construct_master(master, sfiles, count, params)) {
        fprintf(stderr, "at least one bad share file!\n");
        return 1;
    }

    IBE_certify(cert, master, pub, id, params);

    FMT_crypt_save("cert.out", cert, "password");
    byte_string_printf(cert, " %02X");

    return 0;
}
