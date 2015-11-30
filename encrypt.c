/* frontend to IBE_encrypt() in ibe_lib.cc
 * (performs encryption)
 * Ben Lynn
 */

/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/

#include <stdlib.h>
#include <string.h>
#include "ibe_progs.h"
#include "format.h"

int encrypt(int argc, char **argv)
{
    char **idarray;
    int i;
    int count;

    if (argc < 2) {
	printf("Usage: encrypt ID [ID ...]\n\n");
	printf("Encrypts stdin for given ID(s) and prints on stdout\n");
	return 1;
    }

    count = argc - 1;
    idarray = (char **) alloca(sizeof(char *) * count);

    for (i=0; i<count; i++) {
	idarray[i] = FMT_make_id(argv[i + 1], NULL, params);
    }

    FMT_encrypt_stream_array(idarray, count, stdin, stdout, params);

    for (i=0; i<count; i++) {
	free(idarray[i]);
    }
    return 0;
}
