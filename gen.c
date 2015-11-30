/* Generates system parameters.
 * Reads config file from gen.cnf by default
 * Ben Lynn
 */

/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/

#include "ibe.h"
#include "format.h"
#include "config.h"
#include <stdlib.h>

int main(int argc, char **argv)
{
    char defaultcnffile[] = "gen.cnf";
    char *cnffile;
    CONF_CTX *cnfctx;
    int t, n;
    int bits, qbits;
    char *systemid;
    char *paramsfile;
    char **sharefile;

    char *dfltlist[3];
    char dl0[] = "share";
    char dl1[] = "share2";

    int i;

    byte_string_t master;

    params_t params;

    if (argc < 2) {
	cnffile = defaultcnffile;
    } else {
	cnffile = argv[1];
    }

    cnfctx = LoadConfig(cnffile);

    if (!cnfctx) {
	fprintf(stderr, "error opening %s\n", cnffile);
	fprintf(stderr, "using default values\n");
	cnfctx = constructCTX();
	//exit(1);
    }

    t = GetIntParam(cnfctx, "threshold", 0, 2);
    n = GetIntParam(cnfctx, "shares", 0, 2);
    bits = GetIntParam(cnfctx, "pbits", 0, 1024);
    qbits = GetIntParam(cnfctx, "qbits", 0, 160);
    systemid = GetStringParam(cnfctx, "system", 0, "noname");
    paramsfile = GetPathParam(cnfctx, "params", 0, "params.txt");
    dfltlist[0] = dl0;
    dfltlist[1] = dl1;
    sharefile = GetListParam(cnfctx, "sharefiles", 0, dfltlist);

    printf("Generating IBE public parameters and secret...\n");
    printf("Parameters:\n");
    printf("system name: %s\n", systemid);
    printf("params file: %s\n", paramsfile);
    printf("%d-out-of-%d sharing\n", t, n);
    printf("%d-bit prime\n", bits);
    printf("%d-bit subgroup\n", qbits);
    printf("share files:\n");
    for (i=0; i<n; i++) {
	if (sharefile[i] == NULL) {
	    fprintf(stderr, "not enough filenames given\n");
	    exit(1);
	}
	printf("   %s\n", sharefile[i]);
    }

    IBE_init();
    IBE_setup(params, master, bits, qbits, systemid);
    byte_string_printf(master, " %02X");

    FMT_split_master(sharefile, master, t, n, params);

    FMT_save_params(paramsfile, params);

    // test it works
    if (0) {
	byte_string_t key;
	//FMT_construct_master(key, sharefile, t);
	IBE_extract(key, master, "blynn@cs.stanford.edu", params);
	byte_string_clear(key);
    }

    byte_string_clear(master);

    params_clear(params);

    IBE_clear();

    return 0;
}
