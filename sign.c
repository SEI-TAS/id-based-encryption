/* frontend to IBE_sign() in ibe_lib.c
 * (creates signature from message)
 * Dan Klinedinst
 */


#include <stdlib.h>
#include <string.h>
#include "ibe_progs.h"
#include "format.h"
#include "ibe.h"
#include "crypto.h"

int sign(int argc, char **argv)
{

    char *msg;
    byte_string_t message;

    char *privkeyfile;
    byte_string_t key;
    byte_string_t pub;

    char *certfile;
    byte_string_t cert;
    byte_string_t sig;

    int status;

    if (argc < 2) {
	printf("Usage: sign private_key_file certificate_file message\n\n");
	printf("Signs the string message\n");
	return 1;
    }

    privkeyfile = argv[1];

    status = FMT_crypt_load(privkeyfile, key, "password");
    if (status != 1) {
        fprintf(stderr, "error loading private key %s\n", privkeyfile);
        return 1;
    }

    certfile = argv[2];
    status = FMT_crypt_load(certfile, cert, "password");
    if (status != 1) {
        fprintf(stderr, "error loading certificate %s\n", privkeyfile);
        return 1;
    }

    msg = argv[3];
    byte_string_set(message, msg);

    IBE_sign(sig, message, key, cert,  params);

    FMT_crypt_save("sig.out", sig, "password");

    return 0;
}
