/* Tests Weil pairing code
 * (check by eye that the output is indeed bilinear)
 * Ben Lynn
 */
/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/
#include <stdlib.h>
#include "curve.h"

static mpz_t p, q;
static mpz_t p1onq;
static mpz_t p1;
static point_t P;
static point_t P1, P2, Psum;
static point_t Q;
static fp2_t r, r1, r2, r3, r4;
static curve_t curve;

void mympz_randomm(mpz_t x, mpz_t limit)
//x = random in {0, ..., limit - 1}
//not cryptographically strong, just for test purposes
{
    mpz_t n;

    mpz_init(n);

    mpz_set_ui(n, 1);
    do {
	mpz_mul_ui(n, n, rand());
    } while (mpz_cmp(n, limit) < 0);

    mpz_mod(x, n, limit);

    mpz_clear(n);
}

void test(void)
{
    //simple test
    general_point_random(P, curve);
    general_point_mul(P1, p1onq, P, curve);

    general_point_random(P, curve);
    general_point_mul(P2, p1onq, P, curve);

    general_point_random(P, curve);
    general_point_mul(Q, p1onq, P, curve);

    point_add(Psum, P1, P2, curve);

    printf("random P1: ");
    fp2_out_str(stdout, 0, P1->x);
    printf(" ");
    fp2_out_str(stdout, 0, P1->y);
    printf("\n");

    printf("random P2: ");
    fp2_out_str(stdout, 0, P2->x);
    printf(" ");
    fp2_out_str(stdout, 0, P2->y);
    printf("\n");

    printf("random Q: ");
    fp2_out_str(stdout, 0, Q->x);
    printf(" ");
    fp2_out_str(stdout, 0, Q->y);
    printf("\n");

    tate_pairing(r, P1, P1, curve);
    printf("e(P1, P1) = ");
    fp2_out_str(stdout, 0, r);
    printf("\n");

    tate_pairing(r1, P1, Q, curve);
    printf("e(P1, Q) = ");
    fp2_out_str(stdout, 0, r1);
    printf("\n");

    tate_pairing(r2, P2, Q, curve);
    printf("e(P2, Q) = ");
    fp2_out_str(stdout, 0, r2);
    printf("\n");

    fp2_mul(r, r1, r2, p);
    printf("e(P1, Q)e(P2, Q) = ");
    fp2_out_str(stdout, 0, r);
    printf("\n");

    tate_pairing(r, Psum, Q, curve);
    printf("e(P1 + P2, Q) = ");
    fp2_out_str(stdout, 0, r);
    printf("\n");

    tate_pairing(r3, Q, P1, curve);
    printf("e(Q, P1) = ");
    fp2_out_str(stdout, 0, r3);
    printf("\n");

    tate_pairing(r4, Q, P2, curve);
    printf("e(Q, P2) = ");
    fp2_out_str(stdout, 0, r4);
    printf("\n");

    fp2_mul(r, r3, r4, p);
    printf("e(Q, P1)e(Q, P2) = ");
    fp2_out_str(stdout, 0, r);
    printf("\n");

    tate_pairing(r, Q, Psum, curve);
    printf("e(Q, P1 + P2) = ");
    fp2_out_str(stdout, 0, r);
    printf("\n");

    fp2_mul(r, r1, r3, p);
    printf("e(P1, Q)e(Q, P1) = ");
    fp2_out_str(stdout, 0, r);
    printf("\n");

    fp2_mul(r, r2, r4, p);
    printf("e(P2, Q)e(Q, P2) = ");
    fp2_out_str(stdout, 0, r);
    printf("\n");
    mpz_out_str(stdout, 0, p1);
    printf("th powers should be 1\n");

    fp2_pow(r, r1, p1, p);
    fp2_out_str(stdout, 0, r);
    printf("\n");
    fp2_pow(r, r2, p1, p);
    fp2_out_str(stdout, 0, r);
    printf("\n");
    fp2_pow(r, r3, p1, p);
    fp2_out_str(stdout, 0, r);
    printf("\n");
    fp2_pow(r, r4, p1, p);
    fp2_out_str(stdout, 0, r);
    printf("\n");
}

int main(int argc, char **argv)
{
    int i, prime;
    if (argc > 1) {
	sscanf(argv[1], "%d", &prime);
    } else prime = 59;

    mpz_init(p);

    mpz_set_ui(p, prime);
    /*
    if (prime % 3 != 2 || !mpz_probab_prime_p(p, 40) || prime == 2) {
	fprintf(stderr, "only works with odd prime p = 2 mod 3\n");
	exit(1);
    }
    */
    if (prime % 12 != 11 || !mpz_probab_prime_p(p, 40) || prime == 2) {
	fprintf(stderr, "only works with odd prime p = 11 mod 12\n");
	exit(1);
    }

    mpz_init(p1);
    mpz_init(p1onq);
    mpz_init(q);
    mpz_add_ui(p1, p, 1);
    //mpz_set_ui(p1onq, 6);
    mpz_set_ui(p1onq, 12);
    mpz_divexact(q, p1, p1onq);

    curve_init(curve, p, q);

    point_init(P);
    point_init(P1);
    point_init(P2);
    point_init(Psum);
    point_init(Q);
    fp2_init(r);
    fp2_init(r1);
    fp2_init(r2);

    for (i=0; i<10; i++) test();

    point_clear(P);
    point_clear(P1);
    point_clear(P2);
    point_clear(Psum);
    point_clear(Q);
    fp2_clear(r);
    fp2_clear(r1);
    fp2_clear(r2);

    curve_clear(curve);

    return 0;
}
