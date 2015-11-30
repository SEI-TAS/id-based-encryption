/* Command-line frontend to IBE system
 * Patches together combine, decrypt, encrypt and request
 * Ben Lynn
 */

/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/

#include <stdlib.h>
#include <string.h>

#include "format.h"
#include "ibe_progs.h"

CONF_CTX *cnfctx;

params_t params;

int show_params(int argc, char **argv)
{
    params_out(stdout, params);
    return 1;
}

int main(int argc, char **argv)
{
    int result = 0;
    char defaultcnffile[] = "ibe.cnf";
    //char defaultcnffile[100];
    char *cnffile = defaultcnffile;
    char *paramsfile;
    int status;
    char *cmd;

    //strcpy(defaultcnffile, getenv("HOME"));
    //strcat(defaultcnffile, "/.ibe/config");

    //XXX: add option to change config file
    if (argc < 2) {
	fprintf(stderr, "Usage: ibe command\n");
	exit(1);
    }
    cnfctx = LoadConfig(cnffile);

    if (!cnfctx) {
	fprintf(stderr, "error opening %s\n", cnffile);
	exit(1);
    }

    paramsfile = GetPathParam(cnfctx, "params", 0, "params.txt");

    IBE_init();
    status = FMT_load_params(params, paramsfile);
    if (status != 1) {
	fprintf(stderr, "error loading params file %s\n", paramsfile);
	exit(1);
    }

    cmd = argv[1];
    argc--;
    argv++;

    if (!strcmp(cmd, "request")) {
	request(argc, argv);
    } else if (!strcmp(cmd, "decrypt")) {
	decrypt(argc, argv);
    } else if (!strcmp(cmd, "encrypt")) {
	encrypt(argc, argv);
    } else if (!strcmp(cmd, "combine")) {
	combine(argc, argv);
    } else if (!strcmp(cmd, "imratio")) {
	imratio(argc, argv);
    } else if (!strcmp(cmd, "params")) {
	show_params(argc, argv);
    } else if (!strcmp(cmd, "extract_share")) {
	extract_share(argc, argv);
    } else if (!strcmp(cmd, "extract")) {
	extract(argc, argv);
    } else if (!strcmp(cmd, "certify")) {
	certify(argc, argv);
    } else if (!strcmp(cmd, "sign")) {
	sign(argc, argv);
    } else if (!strcmp(cmd, "verify")) {
	verify(argc, argv);
    } else if (!strcmp(cmd, "fixed_password_decrypt")) {
	fixed_password_decrypt(argc, argv);
    } else {
	fprintf(stderr, "unknown command\n");
	result = 1;
    }

    params_clear(params);

    IBE_clear();

    return result;
}
