/* benchmark inversions and multiplications in F_p^2
 * (combines private key shares into a private key)
 * Ben Lynn
 */

/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/

#include "get_time.h"
#include "curve.h"
#include "format.h"
#include "ibe_progs.h"

enum {
    trials = 5000
};

int imratio(int argc, char **argv)
{
    int i;
    fp2_t a[trials], b[trials], c;
    mpz_t x[trials], y[trials], z;
    double t0, t1;
    double R, M, I, S;
    mpz_ptr p = params->p;

    for (i=0; i<trials; i++) {
	fp2_init(a[i]);
	fp2_init(b[i]);
	mpz_init(x[i]);
	mpz_init(y[i]);
    }
    fp2_init(c);

    mpz_init(z);

    printf("F_p^2 operations\n");
    t0 = get_time();
    for (i=0; i<trials; i++) {
	fp2_random(a[i], p);
	fp2_random(b[i], p);
    }
    t1 = get_time();
    R = t1 - t0;
    printf("%d 2R = %f\n", trials, R);

    t0 = get_time();
    for (i=0; i<trials; i++) {
	fp2_mul(c, a[i], b[i], p);
    }
    t1 = get_time();
    M = t1 - t0;
    printf("%d M = %f\n", trials, M);

    t0 = get_time();
    for (i=0; i<trials; i++) {
	fp2_sqr(c, a[i], p);
    }
    t1 = get_time();
    S = t1 - t0;
    printf("%d S = %f\n", trials, S);

    t0 = get_time();
    for (i=0; i<trials; i++) {
	fp2_inv(c, a[i], p);
    }
    t1 = get_time();
    I = t1 - t0;
    printf("%d I = %f\n", trials, I);

    printf("I/M = %f\n", I/M);

    printf("F_p operations\n");

    t0 = get_time();
    for (i=0; i<trials; i++) {
	mympz_randomm(x[i], params->p);
	mympz_randomm(y[i], params->p);
    }
    t1 = get_time();
    R = t1 - t0;
    printf("%d 2R = %f\n", trials, R);

    t0 = get_time();
    for (i=0; i<trials; i++) {
	mpz_mul(z, x[i], y[i]);
    }
    t1 = get_time();
    M = t1 - t0;
    printf("%d M = %f\n", trials, M);

    t0 = get_time();
    for (i=0; i<trials; i++) {
	mpz_invert(z, x[i], p);
    }
    t1 = get_time();
    I = t1 - t0;
    printf("%d I = %f\n", trials, I);

    printf("I/M = %f\n", I/M);

    for (i=0; i<trials; i++) {
	fp2_clear(a[i]);
	fp2_clear(b[i]);
	mpz_clear(x[i]);
	mpz_clear(y[i]);
    }
    fp2_clear(c);

    mpz_clear(z);

    return 0;
}
