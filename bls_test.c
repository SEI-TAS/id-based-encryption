/* BLS signature test program
 * Ben Lynn
 */
/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/
#include "ibe.h"

int main()
{
    params_t params;
    byte_string_t priv, pub;
    byte_string_t priv2, pub2;
    byte_string_t sig;
    byte_string_t message;
    byte_string_t master;

    IBE_init();

    IBE_setup(params, master, 512, 160, "test");

    //generate a BLS private/public key pair
    BLS_keygen(priv, pub, params);

    BLS_keygen(priv2, pub2, params);

    byte_string_set(message, "Hello, World");

    BLS_sign(sig, message, priv, params);
    printf("Sig: ");
    byte_string_fprintf(stdout, sig, " %02X");
    printf("\n");

    if (BLS_verify(sig, message, pub, params)) {
	printf("signature verifies\n");
    } else {
	printf("bug: signature does not verify\n");
    }

    if (BLS_verify(sig, message, pub2, params)) {
	printf("bug: signature verifies with wrong public key\n");
    }

    params_clear(params);

    IBE_clear();

    return 0;
}
