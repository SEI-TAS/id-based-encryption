/* Private Key Generator/simple secure HTML server
 * Ben Lynn
 */
/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/

#include <openssl/ssl.h>
#include <stdio.h>
#include <sys/types.h>

#include "netstuff.h"

#include <unistd.h> //for fork
#include <sys/wait.h> //for wait
#include <syslog.h> //for openlog, syslog
#include <stdarg.h> //for (*fmt, ...)
#include <errno.h>
#include "config.h"
#include "format.h"
#include "ibe.h"

#include <sys/stat.h> //for open
#include <fcntl.h> //(needed to run mail program)

#include <signal.h> //for kill

#include <string.h> //for strncmp

#include <stdlib.h> //for mkstemp

static char *servername;
static char *sharefile;
static char *mailprog;
static byte_string_t bshtml;
static char htmlcomment[80];
static char *bannedfile;

static SSL_CTX *ssl_ctx;

static int daemon_flag = 0;

static byte_string_t mshare;

static params_t params;

void daemon_init(void)
//see UNIX Network Programming by Stevens
//XXX: add error handler
{
    int i;
    pid_t pid;

    daemon_flag = 1;
    if ((pid = fork()) != 0) exit(0);

    setsid();

    signal(SIGHUP, SIG_IGN);
    if ((pid = fork()) != 0) exit(0);

    //umask(0);

    for (i=0; i<64; i++) close(i);

    openlog("pkg", LOG_PID, LOG_DAEMON);
}

void debug_message(const char *fmt, ...)
{
    va_list ap;
    char buf[81];

    va_start(ap, fmt);
    vsnprintf(buf, 80, fmt, ap);
    va_end(ap);

    if (daemon_flag) {
	syslog(LOG_DEBUG, buf);
    } else {
	printf("%s\n", buf);
    }
}

int myssl_readline(SSL *ssl, void *vptr, int maxlen)
//slow; faster versions will require trickery to keep them thread-safe
//from UNP1
{
    int n, rc;
    char c, *ptr;

    ptr = vptr;
    for (n=1; n<=maxlen; n++) {
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
    if (n > 0) {
	debug_message("read '%s'", vptr);
    } else {
	debug_message("returning %d", n);
    }
    return n;
}

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

int old_email_share(char *addr, char *id, char *passwd)
{
    FILE *fp = tmpfile();
    byte_string_t kshare;
    int i;
    int status;

    if (!fp) {
	return 0;
    }

    IBE_extract_share(kshare, mshare, id, params);

    fprintf(fp, "This is a key share\n");
    fprintf(fp, "Server: %s\n\n", servername);
    FMT_crypt_save_fp(fp, kshare, passwd);

    fflush(fp);
    rewind(fp);

    i = strlen(addr) - 1;
    if (addr[i] < 32) addr[i] = 0;

    debug_message("emailing %s...", addr);

    if (!fork()) {
	int fd;
	fd = fileno(fp);
	dup2(fd, 0);
	//fprintf(fp, "MIME-Version: 1.0\r\n");
	//fprintf(fp, "Content-Type: application/x-ibe-keyshare\r\n\r\n");
	//execl(mailprog, mailprog, "-sSHARE", addr, NULL);
	execl(mailprog, mailprog, "-sSHARE", addr, NULL);
    } else {
	wait(&status);
    }
    fclose(fp);

    debug_message("email: done");

    return 0;
}

int email_share(char *addr, char *id, char *passwd)
{
    FILE *fp;
    byte_string_t kshare;
    int i;
    //int status;
    char mailcommand[1024];

    i = strlen(addr) - 1;
    if (addr[i] < 32) addr[i] = 0;

    debug_message("emailing %s...", addr);

    snprintf(mailcommand, 1024, "%s -sSHARE %s", mailprog, addr);

    fp = popen(mailcommand, "w");
    if (!fp) {
	return 0;
    }

    IBE_extract_share(kshare, mshare, id, params);

    fprintf(fp, "This is a key share\n");
    fprintf(fp, "Server: %s\n\n", servername);
    FMT_crypt_save_fp(fp, kshare, passwd);

    fflush(fp);
    rewind(fp);

    fclose(fp);

    debug_message("email: done");

    return 0;
}

//XXX: make more efficient, mutex
int banned_check(char *id)
{
    char buf[200];

    FILE *fp = fopen(bannedfile, "r");
    if (!fp) return 2;

    fgets(buf, 200, fp);
    while (!feof(fp)) {
	buf[strlen(buf) - 1] = '\0'; //remove newline
	if (!strcmp(buf, id)) return 1;
	fgets(buf, 200, fp);
    }

    fclose(fp);

    fp = fopen(bannedfile, "a");
    if (!fp) return 4;
    fprintf(fp, "%s\n", id);
    fclose(fp);
    return 0;
}

int process(SSL *ssl, char *query)
{
    char *email = NULL, *passwd = NULL, *id, *token;
    char *ampptr, *field;

    token = (char *) malloc(strlen(query) + 1);

    ampptr = query;
    ampptr = get_token(token, ampptr, '&');
    while(ampptr) {
	if (!(field = index(token, '='))) {
	    fprintf(stderr, "Malformed query\n");
	    return 0;
	}
	field[0] = 0;
	field++;
	if (!strcmp(token, "mail")) {
	    email = (char *) malloc(strlen(field));
	    strcpy(email, field);
	    translate(email);
	} else if (!strcmp(token, "password")) {
	    passwd = (char *) malloc(strlen(field));
	    strcpy(passwd, field);
	    translate(passwd);
	}
	ampptr = get_token(token, ampptr, '&');
    }

    if (!email || !passwd) {
	char errmsg[] = "Must supply email address and password.";
	SSL_write(ssl, errmsg, strlen(errmsg));
	return 0;
    }
    id = FMT_make_id(email, NULL, params);

    if (banned_check(id)) {
	//char errmsg[] = "This ID has already been requested!";
	//SSL_write(ssl, errmsg, strlen(errmsg));
	debug_message("WARNING: banned ID");
	//return;
    }
    email_share(email, id, passwd);

    debug_message("mailed key share to %s", email);
    SSL_write(ssl, "Key share mailed to ", 20);
    SSL_write(ssl, email, strlen(email));
    SSL_write(ssl, "\n", 1);

    free(id);
    free(email);
    free(passwd);
    free(token);
    return 1;
}

void *handle_session(int newfd)
{
    char head[] = "HTTP/1.0 200 ok\r\nContent-type: text/html\r\n\r\n";
    SSL *ssl;
    int status;
    int len;
    int bufmax = 1024;
    char buf[bufmax];

    //send(newfd, "Hello World!\n", 14, 0);

    ssl = SSL_new(ssl_ctx);
    if (!ssl) {
	debug_message("SSL_new: failure");
	return NULL;
    }
    debug_message("SSL_new: success");

    status = SSL_set_fd(ssl, newfd);
    if (!status) {
	debug_message("SSL_set_fd: failure");
	return NULL;
    }
    debug_message("SSL_set_fd %d: success", newfd);

    SSL_set_accept_state(ssl);

    debug_message("SSL_set_acc_state");
    status = SSL_accept(ssl);
    if (1 != status) {
	debug_message("SSL handshake failed");
    }
    debug_message("SSL handshake successful");

    len = myssl_readline(ssl, buf, bufmax);
    debug_message("SSL read %d", len);

    if (len < 4) {
	debug_message("too short for GET/POST!");
	return NULL;
    } else if (!strncmp(buf, "GET ", 4)) {
	debug_message("got a GET");
	SSL_write(ssl, head, strlen(head));
	SSL_write(ssl, htmlcomment, strlen(htmlcomment));
	SSL_write(ssl, bshtml->data, bshtml->len);
    } else if (!strncmp(buf, "POST", 4))  {
	char clstr[] = "Content-Length: ";
	int i = strlen(clstr);
	int clength = 0;

	debug_message("got a POST");

	for(;;) {
	    len = myssl_readline(ssl, buf, bufmax);
	    if (len <= 0) {
		debug_message("Content-Length not found");
		return NULL;
	    }
	    if (!strncasecmp(buf, clstr, i)) {
		break;
	    }
	}

	for (;;) {
	    if (buf[i] < '0' || buf[i] > '9') {
		break;
	    }
	    clength *= 10;
	    clength += buf[i] - '0';
	    i++;
	}
	debug_message("Content-Length = %d", clength);
	for(;;) {
	    len = myssl_readline(ssl, buf, bufmax);
	    if (len <= 0) {
		debug_message("blank line not found");
		return NULL;
	    }
	    if (!strncmp(buf, "\n", 1) || !strncmp(buf, "\r\n", 2)) {
		break;
	    }
	}
	debug_message("blank line found");
	len = myssl_readline(ssl, buf, clength);
	if (len <= 0) {
	    debug_message("Content not found");
	    return NULL;
	}
	for(;;) {
	    if (len == 0) return NULL;
	    if (index("\r\n", buf[len-1])) {
		len--;
		buf[len] = 0;;
	    } else break;
	}
	if (strlen(buf) != clength) {
	    debug_message("warning: content length mismatch: %d != %d",
		clength, strlen(buf));
	}
	process(ssl, buf);
    }
    debug_message("shutting down connection");
    //if it's IE, don't bother calling shutdown
    SSL_shutdown(ssl);
    close(newfd);
    debug_message("child returning...");
    return NULL;
}

void sig_child(int signo)
{
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
	debug_message("child %d terminated", pid);
    }
    return;
}

int main(int argc, char **argv)
{
    int sockfd;
    int status;
    char defaultcnffile[] = "pkg.cnf";
    char *cnffile;
    CONF_CTX *cnfctx;
    int port;
    char *certfile, *keyfile, *paramsfile, *htmlfile;
    int on;
    struct sockaddr_in my_addr;
    struct sockaddr_in their_addr;
    int newfd;
    pid_t childpid;

    //Linux manpage says socklen_t, Solaris says size_t
    //socklen_t sockaddrsize = sizeof(struct sockaddr);
    size_t sockaddrsize = sizeof(struct sockaddr);

    netstartupjunk();

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
    }

    port = GetIntParam(cnfctx, "port", 0, 31831);
    bannedfile = GetPathParam(cnfctx, "banned", 0, "banned");
    certfile = GetPathParam(cnfctx, "cert", 0, "ca.cert");
    keyfile = GetPathParam(cnfctx, "key", 0, "ca.priv");
    paramsfile = GetPathParam(cnfctx, "params", 0, "params.txt");
    //int maxspawn = GetIntParam(cnfctx, "maxspawn", 0, 100);
    sharefile = GetPathParam(cnfctx, "sharefile", 0, "share");
    htmlfile = GetPathParam(cnfctx, "htmlfile", 0, "pkgform.html");
    mailprog = GetPathParam(cnfctx, "mail", 0, "/usr/bin/mail");
    servername = GetStringParam(cnfctx, "servername", 0, "server 0");

    printf("Private Key Generator\n");
    printf("running on port %d\n", port);
    printf("certificate file: %s\n", certfile);
    printf("key file: %s\n", keyfile);
    printf("params file: %s\n", paramsfile);
    printf("share file: %s\n", sharefile);
    printf("mailer: %s\n", mailprog);
    printf("html file: %s\n", htmlfile);
    printf("banned file: %s\n", bannedfile);
    //printf("max processes: " << maxspawn << endl;

    SSL_library_init();

    IBE_init();
    status = FMT_load_params(params, paramsfile);
    if (status != 1) {
	fprintf(stderr, "error loading params file %s\n", paramsfile);
	return(1);
    }

    status = FMT_load_byte_string(sharefile, mshare);
    if (status != 1) {
	fprintf(stderr, "error loading share file %s\n", sharefile);
	return(1);
    }

    sprintf(htmlcomment, "<!-- PKG: %s -->\n", servername);

    status = FMT_load_raw_byte_string(bshtml, htmlfile);
    if (status != 1) {
	fprintf(stderr, "error reading %s\n", htmlfile);
	exit(1);
    }

    ssl_ctx = SSL_CTX_new(SSLv23_method());
    if (!ssl_ctx) {
	fprintf(stderr, "SSL_CTX_new: failure\n");
	exit(1);
    }

    status = SSL_CTX_use_certificate_file(ssl_ctx, certfile, SSL_FILETYPE_PEM);
    if (status != 1) {
	fprintf(stderr, "SSL_CTX_use_certificate_file: failure\n");
	exit(1);
    }
    status = SSL_CTX_use_PrivateKey_file(ssl_ctx, keyfile, SSL_FILETYPE_PEM);
    if (status != 1) {
	fprintf(stderr, "SSL_CTX_use_PrivateKey_file: failure\n");
	exit(1);
    }

    daemon_init();

    debug_message("opening socket...");
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
	debug_message(strerror(errno));
	exit(1);
    }

    on = 1;
    status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
        (void *) &on, sizeof(on));

    if (-1 == status) {
	debug_message(strerror(errno));
	exit(1);
    }

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    debug_message("binding...");
    status = bind(sockfd, (struct sockaddr *) &my_addr, sockaddrsize);
    if (-1 == status) {
        debug_message("bind error");
        debug_message(strerror(errno));
	exit(1);
    }
    debug_message("listening...");
    status = listen(sockfd, 20);
    if (-1 == status) {
        debug_message("listen error");
        debug_message(strerror(errno));
	exit(1);
    }

    signal(SIGCHLD, sig_child);
    for(;;) {
	debug_message("accepting...");
	newfd = accept(sockfd, (struct sockaddr *) &their_addr, &sockaddrsize);
	if (-1 == newfd) {
	    if (errno == EINTR) continue;
	    else {
		debug_message("accept error");
		debug_message(strerror(errno));
		exit(1);
	    }
	}
	debug_message("connection from %s", inet_ntoa(their_addr.sin_addr));
	if ((childpid = fork()) == 0) {
	    handle_session(newfd);
	    exit(0);
	} else {
	    debug_message("created child %d", childpid);
	}
	close(newfd);
    }
}
