/* Sends encrypted email to unsuspecting user
 * Impossible in conventional public-key cryptosystems.
 * Ben Lynn
 */
/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/
#include <stdio.h>
#include <unistd.h> //for fork
#include "config.h"
#include "format.h"
#include "ibe.h"
#include <sys/wait.h> //for wait
#include <sys/stat.h> //for open
#include <fcntl.h> //(needed to run mail program)
#include <signal.h> //for kill
#include <string.h> //for strncmp
#include <stdlib.h> //for mkstemp

static char *encfile;
static char *mailprog;
static byte_string_t plainmessage;

static params_t params;

static char *get_token(char *token, char *s, char c)
//replacement for strtok
{
    int i;
    i = 0;
    if (!s[0]) return NULL;
    for(;;) {
	if (s[i] == c) {
	    token[i] = 0;
	    i++;
	    return &s[i];
	}
	token[i] = s[i];
	if (!s[i]) return &s[i];
	i++;
    }
}

char hextod(char ch)
{
    if (ch >= '0' && ch <= '9') {
	return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
	return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
	return ch - 'A' + 10;
    }
    return 0;
}

void translate(char *s)
{
    int n = strlen(s);
    int i, j;

    for (i = j = 0; i < n; i++, j++) {
	if (s[i] == '+') {
	    s[j] = ' ';
	} else if (s[i] == '%') {
	    i++;
	    if (i == n) break;
	    s[j] = hextod(s[i]) * 16;
	    i++;
	    if (i == n) break;
	    s[j] += hextod(s[i]);
	} else {
	    s[j] = s[i];
	}
    }
    s[j] = '\0';
    //printf("translation: %s\n", s);
}

int infect(char *addr, char *id)
{
    char letter[] = "/tmp/ibeXXXXXX";
    FILE *fp;
    int status;
    int i;
    FILE *infp;

    mkstemp(letter);

    fp = fopen(letter, "w");
    if (!fp) {
	printf("error opening temp file\n");
	return 0;
    }

    infp = fopen(encfile, "r");
    if (!infp) {
	fprintf(stderr, "error opening secret message\n");
    }

    //fprintf(fp, "--0123456789BOUNDARY0123456789\r\n");
    //fprintf(fp, "Content-Type: text/plain\r\n\r\n");
    byte_string_fprintf(fp, plainmessage, "%c");

    //fprintf(fp, "--0123456789BOUNDARY0123456789\r\n");
    //fprintf(fp, "Content-Type: text/plain; x-encryption=ibe-encrypt\r\n\r\n");
    fprintf(fp, "\n\n-----BEGIN IBE-----\n");

    FMT_encrypt_stream(id, infp, fp, params);

    fclose(infp);

    fprintf(fp, "-----END IBE-----\n");
    //fprintf(fp, "--0123456789BOUNDARY0123456789--\r\n");
    //fprintf(fp, "\r\n\r\n");
    fclose(fp);

    i = strlen(addr) - 1;
    printf("last: %d\n", addr[i]);
    //this hack is needed for some browsers :(
    if (addr[i] < 32) addr[i] = 0;
    //if (addr[i] == 25) addr[i] = 0;
    printf("emailing %s...\n", addr);
    printf("strlen = %d\n", strlen(addr));

    if (!fork()) {
	int fd;
	fd = open(letter, O_RDONLY);
	dup2(fd, 0);
	execl(mailprog, mailprog, "-sIBE", addr, NULL);
	//execl(mailprog, mailprog, "-aMIME-Version: 1.0",
		//"-aContent-Type: multipart/mixed; boundary = \"0123456789BOUNDARY0123456789\"", "-sIBE", addr, NULL);
    } else {
	wait(&status);
    }

    unlink(letter);
    return 1;
}

int main(int argc, char **argv)
{
    char *field;
    char *ampptr;
    char *query;
    char *token;
    int status;
    char defaultcnffile[] = "infect.cnf";
    char *cnffile, *paramsfile;
    CONF_CTX *cnfctx;
    char *unencfile;
    char *email, *subject;
    char *id;

    if (argc < 2) {
	printf("Usage: infect id=<email address>\n");
	return 0;
    }
    if (argc > 2) {
	cnffile = argv[1];
	query = argv[2];
    } else {
	cnffile = defaultcnffile;
	query = argv[1];
    }

    cnfctx = LoadConfig(cnffile);

    if (!cnfctx) {
	fprintf(stderr, "error opening %s\n", cnffile);
	fprintf(stderr, "using default values\n");
	cnfctx = constructCTX();
    }

    paramsfile = GetPathParam(cnfctx, "params", 0, "params.txt");
    mailprog = GetPathParam(cnfctx, "mail", 0, "/usr/bin/mail");
    encfile = GetPathParam(cnfctx, "messagefile", 0, "firstmessage.txt");
    unencfile = GetPathParam(cnfctx, "helpfile", 0, "instructions.txt");

    printf("Infecting %s\n", query);
    /*
    cout << "params file: " << paramsfile << endl;
    cout << "mailer: " << mailprog << endl;
    cout << "instructions: " << unencfile << endl;
    cout << "message: " << encfile << endl;
    */

    status = FMT_load_raw_byte_string(plainmessage, unencfile);
    if (status != 1) {
	fprintf(stderr, "error opening instructions\n");
    }

    IBE_init();
    status = FMT_load_params(params, paramsfile);
    if (status != 1) {
	fprintf(stderr, "Error loading params file %s\n", paramsfile);
	return(1);
    }

    email = NULL;
    subject = NULL;

    token = (char *) malloc(strlen(query) + 1);

    ampptr = query;
    ampptr = get_token(token, ampptr, '&');
    while(ampptr) {
	if (!(field = index(token, '='))) {
	    fprintf(stderr, "Malformed query\n");
	    return 1;
	}
	field[0] = 0;
	field++;
	if (!strcmp(token, "id")) {
	    email = (char *) malloc(strlen(field));
	    strcpy(email, field);
	    translate(email);
	} else if (!strcmp(token, "subject")) {
	    subject = (char *) malloc(strlen(field));
	    strcpy(subject, field);
	    translate(subject);
	}
	ampptr = get_token(token, ampptr, '&');
    }

    if (!email) {
	//char errmsg[] = "Must supply email address and password.";
	fprintf(stderr, "bad ID\n");
	return 1;
    }
    id = FMT_make_id(email, subject, params);
    infect(email, id);

    free(id);
    free(email);
    free(subject);
    free(token);

    params_clear(params);
    IBE_clear();

    return 0;
}
