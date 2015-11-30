/* frontend to IBE_request() in ibe_lib.cc
 * (requests private keys)
 * Ben Lynn
 */
/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <string.h>

#include "netstuff.h"

#include "config.h"
#include "ibe_progs.h"

static SSL *ssl;
static void write_string(char *s);
static char *emailaddr;
static int single_request(char *hostname, int port);

static char password[128];
SSL_CTX *ssl_ctx;

int myssl_readline(SSL *ssl, void *vptr, int maxlen)
//slow; faster versions will require trickery to keep them thread-safe
//from UNP1
{
    int n, rc;
    char c, *ptr;

    ptr = vptr;
    for (n=1; n<maxlen; n++) {
	if ((rc = SSL_read(ssl, &c, 1)) == 1) {
	    *ptr++ = c;
	    if (c == '\n') break;
	} else if (rc == 0) {
	    if (n == 1) return 0;
	    else break;
	} else {
	    return rc;
	}
    }

    *ptr = '\0';
    return n;
}

void write_string(char *s)
{
//cout << "Writing " << s << endl;
    int status;
    status = SSL_write(ssl, s, strlen(s));
//cout << "status = " << status << endl;
    if (status <= 0) {
	fprintf(stderr, "write_string: failure\n");
	exit(1);
    }
}

int single_request(char *hostname, int port)
{
    int sockfd;
    int status;

    struct sockaddr_in server_addr;
    struct hostent *hostp = NULL;
    //Linux manpage says socklen_t, Solaris says size_t
    //socklen_t sockaddrsize = sizeof(struct sockaddr);
    size_t sockaddrsize = sizeof(struct sockaddr);

    char buf[1024];
    int buflen = 1024;
    int len;
    char *servername;
    char *ptr;
    int i;

    printf("opening socket...\n");
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
	perror("socket");
	return(-1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    hostp = gethostbyname(hostname);
    if (NULL == hostp) {
	hostp = gethostbyaddr(hostname, strlen(hostname), AF_INET);
	if (NULL == hostp) {
	    perror("Error resolving server address");
	    return(-1);
	}
    }

    server_addr.sin_addr = *((struct in_addr *)hostp->h_addr);

    printf("connecting...\n");
    status = connect(sockfd, (struct sockaddr *) &server_addr, sockaddrsize);
    if (-1 == status) {
        perror("connect");
	return(-1);
    }

    ssl = SSL_new(ssl_ctx);
    if (!ssl) {
	fprintf(stderr, "SSL_new: failure\n");
	return(-1);
    }

    status = SSL_set_fd(ssl, sockfd);
    if (!status) {
	fprintf(stderr, "SSL_set_fd: failure\n");
	return(-1);
    }

    status = SSL_connect(ssl);
    if (status != 1) {
	fprintf(stderr, "SSL_connect: failure\n");

	printf("status %d\n", status);
	SSL_load_error_strings();
	printf("%d: ", SSL_get_error(ssl, status));
	printf("%s\n", ERR_error_string(SSL_get_error(ssl, status), NULL));

	return(-1);
    }

    //XXX: odd hack
    write_string("GET /\n");
	
    for (;;) {
	char marker[] = "<!-- PKG: ";
	i = strlen(marker);
	len = myssl_readline(ssl, buf, buflen);
	if (len <= 0) {
	    fprintf(stderr, "Doesn't seem to be a PKG\n");
	    return(-1);
	}
	if (!strncmp(buf, marker, i)) break;
    }

    servername = &buf[i];
    ptr = strstr(servername, " -->");
    if (ptr) ptr[0] = 0;

    SSL_shutdown(ssl);
    shutdown(sockfd, 2);

    printf("server name: %s\n", servername);
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
	perror("socket");
	return(-1);
    }

    status = connect(sockfd, (struct sockaddr *) &server_addr, sockaddrsize);
    if (-1 == status) {
        perror("connect");
	return(-1);
    }

    status = SSL_set_fd(ssl, sockfd);
    if (!status) {
	fprintf(stderr, "SSL_set_fd: failure\n");
	return(-1);
    }

    status = SSL_connect(ssl);
    if (status != 1) {
	fprintf(stderr, "SSL_connect: failure\n");
	return(-1);
    }

    status = EVP_read_pw_string(password, 100, "share password: ", 1);
    if (status) {
	fprintf(stderr, "error reading password\n");
	return 1;
    }

    write_string("POST /\n");
    write_string("Content-Length: ");

    //Windows doesn't have snprintf?

    sprintf(buf, "%d\n\n", 16 + strlen(emailaddr) + strlen(password));

    write_string(buf);

    sprintf(buf, "mail=%s&password=%s\n", emailaddr, password);

    write_string(buf);

    for (;;) {
	len = myssl_readline(ssl, buf, buflen);
	if (len <= 0) break;
	printf("server: %s", buf);
    }
    status = SSL_shutdown(ssl);
    if (status != 1) {
	fprintf(stderr, "SSL_shutdown: failure\n");
	printf("status %d\n", status);
	SSL_load_error_strings();
	printf("%d: ", SSL_get_error(ssl, status));
	printf("%s\n", ERR_error_string(SSL_get_error(ssl, status), NULL));
    }

    return 1;
}

int request(int argc, char **argv)
{
    int default_port;
    char **servers;
    int status;
    int t = IBE_threshold(params);

    char *hostname[t];
    int port[t];

    int i;
    int success;

    netstartupjunk();

    if (argc < 2) {
	fprintf(stderr, "Usage: request ID\n");
	return(1);
    }

    SSL_library_init();

    ssl_ctx = SSL_CTX_new(SSLv3_method());
    if (!ssl_ctx) {
	fprintf(stderr, "SSL_CTX_new: failure\n");
	return(-1);
    }

    emailaddr = argv[1];
    default_port = GetIntParam(cnfctx, "default_port", 0, 31831);

    printf("Threshold: %d\n", t);
    servers = GetListParam(cnfctx, "servers", 0, NULL);
    if (!servers) {
	fprintf(stderr, "no servers given in config file\n");
	return(1);
    }

    SSL_library_init();
    for (i=success=0; success<t; i++) {
	int len;
	int j;
	if (!servers[i]) {
	    fprintf(stderr, "not enough working servers\n");
	    return(1);
	}
	len = strlen(servers[i]);
	for (j=0; j<len && servers[i][j] != ':'; j++);
	hostname[i] = (char *) malloc(j + 1);
	memcpy(hostname[i], servers[i], j);
	hostname[i][j] = 0;
	if (j >= len - 1) {
	    port[i] = default_port;
	} else {
	    port[i] = atoi(&servers[i][j+1]);
	}
	printf("trying: %s:%d\n", hostname[i], port[i]);
	status = single_request(hostname[i], port[i]);
	if (status == 1) success++;
    }

    for (i=0; i<t; i++) {
	free(hostname[i]);
    }

    return(0);
}
