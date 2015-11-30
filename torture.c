/* IBE torture test program
 * Ben Lynn
 */
/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "ibe.h"
#include "crypto.h"

enum {
    test_kem = 0,
    test_split,
    test_key,
    test_combine,
    test_bls,
    test_sig,
    test_crypto,

    test_random,
    test_max,

    metatest_single = 0,
    metatest_multiple,
    metatest_thread,
    metatest_max
};

typedef int (*torturefn)(params_t params, byte_string_t master);
typedef void (*metatorturefn)(int test_no);

struct torture_s {
    int index;
    torturefn f;
    char *name;
};

struct metatorture_s {
    int index;
    metatorturefn f;
    char *name;
};

struct two_ints_s {
    int i, j;
};

static struct torture_s torture_table[test_max];
static struct metatorture_s metatorture_table[metatest_max];

static void random_charstar(char *id, int len)
{
    int i, n;

    if (len < 3) return;

    n = (rand() % (len - 2)) + 1;

    for (i=0; i<n; i++) {
	id[i] = (rand() % 127) + 1;
    }
    id[n] = 0;
}

static int split_test(params_t params, byte_string_t master)
{
    int t = 10, n = 20;
    byte_string_t master2;
    int i;
    int result = 1;

    byte_string_t mshare[n];
    IBE_split_master(mshare, master, t, n, params);

    for (i=0; i<=n-t; i++) {
	IBE_construct_master(master2, &mshare[i], params);
	if (byte_string_cmp(master, master2)) {
	    result = 0;
	}
	byte_string_clear(master2);
    }

    params_robust_clear(params);
    for (i=0; i<n; i++) {
	byte_string_clear(mshare[i]);
    }
    return result;
}

static int combine_test(params_t params, byte_string_t master)
{
    char id[1024];
    int i, j;
    int t = 5, n = 10;
    byte_string_t key;
    byte_string_t key2;
    byte_string_t mshare[n];
    byte_string_t kshare[t];
    int result = 1;

    IBE_split_master(mshare, master, t, n, params);

    random_charstar(id, 1024);

    for (j=0; j<=n-t; j++) {

	for (i=0; i<t; i++) {
	    IBE_extract_share(kshare[i], mshare[i + j], id, params);
	}
	IBE_combine(key, kshare, params);

	IBE_extract(key2, master, id, params);

	if (byte_string_cmp(key, key2)) {
	    printf("BUG! combine_test() failed\n");
	    result = 0;
	}
	byte_string_clear(key);
	byte_string_clear(key2);

	for (i=0; i<t; i++) {
	    byte_string_clear(kshare[i]);
	}
    }

    for (i=0; i<n; i++) {
	byte_string_clear(mshare[i]);
    }
    params_robust_clear(params);
    return result;
}

static int bls_test(params_t params, byte_string_t master)
{
    byte_string_t priv, pub;
    char s[1024];
    byte_string_t message;
    byte_string_t sig;
    int result = 1;

    BLS_keygen(priv, pub, params);

    random_charstar(s, 1024);
    byte_string_set(message, s);

    BLS_sign(sig, message, priv, params);

    if (!BLS_verify(sig, message, pub, params)) {
	printf("BUG! bls_test failed!\n");
	result = 0;
    }

    byte_string_clear(priv);
    byte_string_clear(pub);
    byte_string_clear(message);
    byte_string_clear(sig);

    return result;
}

static int sig_test(params_t params, byte_string_t master)
{
    byte_string_t priv, pub;
    char id[1024];
    char s[1024];
    byte_string_t message;
    byte_string_t cert;
    byte_string_t sig;
    int result = 1;

    random_charstar(id, 1024);
    random_charstar(s, 1024);
    byte_string_set(message, s);

    IBE_keygen(priv, pub, params);

    IBE_certify(cert, master, pub, id, params);

    IBE_sign(sig, message, priv, cert, params);

    if (!IBE_verify(sig, message, pub, id, params)) {
	printf("BUG! sig_test failed!\n");
	result = 0;
    }

    byte_string_clear(priv);
    byte_string_clear(pub);
    byte_string_clear(message);
    byte_string_clear(cert);
    byte_string_clear(sig);

    return result;
}

static int random_test(params_t params, byte_string_t master)
{
    int i = rand() % test_random;
    printf("<%s>", torture_table[i].name);
    return torture_table[i].f(params, master);
}

static int kem_test(params_t params, byte_string_t master)
{
    char id[1024];
    int i;
    byte_string_t U;
    byte_string_t key;
    byte_string_t K, K2;
    int result = 1;

    random_charstar(id, 1024);

    IBE_extract(key, master, id, params);
    for (i=0; i<8; i++) {
	IBE_KEM_encrypt(K, U, id, params);
	IBE_KEM_decrypt(K2, U, key, params);
	byte_string_clear(U);

	if (byte_string_cmp(K, K2)) {
	    //printf("BUG! KEM is broken!\n");
	    result = 0;
	}
	byte_string_clear(K);
	byte_string_clear(K2);
    }
    byte_string_clear(key);
    return result;
}

static int crypto_test(params_t params, byte_string_t master)
{
    int bufsize = 1024;
    byte_string_t K;
    unsigned char inbuf[bufsize];
    unsigned char cbuf[bufsize + 100];
    unsigned char outbuf[bufsize];
    int ci, oi, i;
    int inc, inc2;

    crypto_ctx_t c, c2;

    crypto_generate_key(K);
    crypto_rand_bytes(inbuf, bufsize);

    crypto_ctx_init(c);
    crypto_ctx_init(c2);

    crypto_encrypt_init(c, K);
    crypto_decrypt_init(c2, K);

    if (rand() % 2) {
	//use random chunk sizes for encrypt_update
	i = 0;
	oi = 0;
	for (;;) {
	    inc = 1 + (rand() % 24);
	    if (i + inc > bufsize) inc = bufsize - i;
	    crypto_encrypt_update(c, cbuf, &ci, &inbuf[i], inc);
	    i += inc;
	    crypto_decrypt_update(c2, &outbuf[oi], &inc, cbuf, ci);
	    oi += inc;
	    if (i == bufsize) break;
	}

	crypto_encrypt_final(c, cbuf, &ci);
	crypto_decrypt_update(c2, &outbuf[oi], &inc, cbuf, ci);
	oi += inc;
	crypto_decrypt_final(c2, &outbuf[oi], &inc);
	oi += inc;
    } else {
	//use random chunk sizes for decrypt_update
	crypto_encrypt_update(c, cbuf, &ci, inbuf, bufsize);
	crypto_encrypt_final(c, &cbuf[ci], &inc);
	ci += inc;

	i = 0;
	oi = 0;
	for (;;) {
	    inc = 1 + (rand() % 24);
	    //inc = 1; //byte by byte
	    if (i + inc > ci) inc = ci - i;
	    crypto_decrypt_update(c2, &outbuf[oi], &inc2, &cbuf[i], inc);
	    i += inc;
	    oi += inc2;
	    if (i == ci) break;
	}

	crypto_decrypt_final(c2, &outbuf[oi], &inc);
	oi += inc;
    }

    crypto_ctx_clear(c);
    crypto_ctx_clear(c2);
    byte_string_clear(K);

    if (memcmp(inbuf, outbuf, bufsize)) {
	printf("BUG! crypto_test() failed\n");
	return 0;
    }

    return 1;
}

static int key_test(params_t params, byte_string_t master)
{
    char id[1024];
    int i;
    byte_string_t U, V;
    byte_string_t key;
    byte_string_t K, K2;
    int result = 1;

    random_charstar(id, 1024);

    IBE_extract(key, master, id, params);
    for (i=0; i<8; i++) {
	if (1 != crypto_generate_key(K)) {
	    printf("BUG! generate_key failed!\n");
	    result = 0;
	} else if (1 != IBE_hide_key(U, V, id, K, params)) {
	    printf("BUG! hide_key failed!\n");
	    byte_string_clear(K);
	    byte_string_clear(U);
	    byte_string_clear(V);
	    result = 0;
	} else if (1 != IBE_reveal_key(K2, U, V, key, params)) {
	    printf("BUG! reveal_key failed!\n");
	    byte_string_clear(K);
	    byte_string_clear(U);
	    byte_string_clear(V);
	    result = 0;
	} else {
	    if (byte_string_cmp(K, K2)) {
		printf("BUG! key mismatch!\n");
		result = 0;
	    }
	    byte_string_clear(K);
	    byte_string_clear(K2);
	    byte_string_clear(U);
	    byte_string_clear(V);
	}
    }
    byte_string_clear(key);

    return result;
}

static void single_system_torture(int test_no)
{
    params_t params;
    byte_string_t master;
    int trial;
    int i;
    int status = 1;

    for (trial=0; trial<50; trial++) {
	printf("System #%d setup...\n", trial);
	IBE_setup(params, master, 512, 160, "test");
	printf("Testing");
	fflush(stdout);
	for (i=0; i<10; i++) {
	    if (torture_table[test_no].f(params, master) == 0) {
		printf("!");
		status = 0;
	    } else {
		printf("."); fflush(stdout);
	    }
	}
	if (status) {
	    printf("passed\n");
	} else {
	    printf("FAILED\n");
	}

	byte_string_clear(master);
	params_clear(params);
    }
}

static void multiple_system_torture(int test_no)
{
    int syscount = 10;
    byte_string_t master[syscount];
    params_t params[syscount];
    int i, j;
    char s[80];

    //initialize different IBE systems
    for (i=0; i<syscount; i++) {
	printf("Generating system #%d\n", i);
	snprintf(s, 80, "System #%d", i);
	IBE_setup(params[i], master[i], 512, 160, s);
    }

    for (j=0; j<10; j++) {
	for (i=0; i<syscount; i++) {
	    printf("i = %d\n", i);
	    torture_table[test_no].f(params[i], master[i]);
	}
    }

    for (i=0; i<syscount; i++) {
	params_clear(params[i]);
	byte_string_clear(master[i]);
    }
}

static void *thread_function(void *arg)
{
    struct two_ints_s *s = (struct two_ints_s *) arg;
    params_t params;
    byte_string_t master;
    int trial;
    int index = s->i;
    torturefn f = torture_table[s->j].f;
    int i;

    for (trial=0; trial<5; trial++) {
	printf("Thread #%d round %d setup...\n", index, trial);
	IBE_setup(params, master, 512, 160, "test");
	printf("Thread #%d round %d setup complete\n", index, trial);
	for (i=0; i<10; i++) {
	    if (f(params, master) == 1) {
		printf("Thread #%d round %d-%d successful\n", index, trial, i);
	    } else {
		printf("TEST FAILED!\n");
	    }
	}
	printf("Thread #%d round %d complete\n", index, trial);
	byte_string_clear(master);
	params_clear(params);
    }

    return NULL;
}

static void thread_torture(int test_no)
{
    int tcount = 5;
    struct two_ints_s s[tcount];
    pthread_t tid[tcount];
    int i;

    for (i=0; i<tcount; i++) {
	s[i].j = test_no;
	s[i].i = i;
	pthread_create(&tid[i], NULL, thread_function, (void *) &s[i]);
    }

    for (i=0; i<tcount; i++) {
	pthread_join(tid[i], NULL);
    }
}

static void register_test(int index, char *name, torturefn f)
{
    struct torture_s *t = &torture_table[index];

    t->name = name;
    t->index = index;
    t->f = f;
}

static void register_metatest(int index, char *name, metatorturefn f)
{
    struct metatorture_s *t = &metatorture_table[index];

    t->name = name;
    t->index = index;
    t->f = f;
}

static void init_torture_table()
{
    register_test(test_bls, "BLS", bls_test);
    register_test(test_sig, "signature", sig_test);
    register_test(test_kem, "KEM", kem_test);
    register_test(test_key, "key", key_test);
    register_test(test_split, "split", split_test);
    register_test(test_combine, "combine", combine_test);
    register_test(test_crypto, "crypto", crypto_test);
    register_test(test_random, "random", random_test);

    register_metatest(metatest_single, "single", single_system_torture);
    register_metatest(metatest_multiple, "multiple", multiple_system_torture);
    register_metatest(metatest_thread, "threaded", thread_torture);
}

static int atoi_clipped(char *s, int lower, int upper)
{
    int result = atoi(s);

    if (result < lower) result = lower;
    if (result > upper) result = upper;

    return result;
}

static void help_screen()
{
    int i;
    printf("IBE library torture prorgram\n");
    printf("Usage: torture [OPTION...]\n");
    printf("  -h       help\n");
    printf("  -t NUM   torture number\n");
    printf("  -m NUM   metatorture number\n");
    printf("\n");
    printf("Tortures\n");
    for (i=0; i<test_max; i++) {
	printf(" %d: %s\n", i, torture_table[i].name);
    }

    printf("\n");
    printf("Metatortures\n");
    for (i=0; i<metatest_max; i++) {
	printf(" %d: %s\n", i, metatorture_table[i].name);
    }
}

int main(int argc, char **argv)
{
    int i, j;

    init_torture_table();

    i = test_random; j = 0; //default test style

    for(;;) {
	int c;
	c = getopt(argc, argv, "t:m:h");

	if (c == -1) break;

	switch (c) {
	    case 't':
		i = atoi_clipped(optarg, 0, test_max - 1);
		break;
	    case 'm':
		j = atoi_clipped(optarg, 0, metatest_max - 1);
		break;
	    case 'h':
		help_screen();
		return 0;
	    default:
		printf("use -h for help\n");
		return 0;
	}
    }

    IBE_init();

    printf("IBE library torture\n");

    printf("Test %d-%d: ", j, i);
    printf("%s %s\n", metatorture_table[j].name, torture_table[i].name);

    metatorture_table[j].f(i);

    IBE_clear();
    return 0;
}
