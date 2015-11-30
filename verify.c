/* frontend to IBE_verify() in ibe_lib.c
 * (verifies signature on message)
 * Dan Klinedinst
 */


#include <stdlib.h>
#include <string.h>
#include "ibe_progs.h"
#include "format.h"
#include "ibe.h"
#include "crypto.h"

int verify(int argc, char **argv)
{

    char *msg;
    byte_string_t message;

    char *public;
    byte_string_t pub;

    char *sigfile;
    byte_string_t sig;

    char *id;

    int result;

    if (argc < 2) {
	printf("Usage: verify signature_file ID message\n\n");
	printf("Verifies that ID signed string message with signature\n");
	return 1;
    }

    id = IBE_system(params);

    sigfile = argv[1];
    FMT_crypt_load(sigfile, sig, "password");

    public = argv[2];

    msg = argv[3];
    byte_string_set(message, msg);

    if (IBE_verify(sig, message, pub, id, params)) {
        result = 1;
        printf("Signature verifies: %d", result);
    } else {
        result = 0;
        printf("Invalid signature: %d", result);
    }

    return result;
}
