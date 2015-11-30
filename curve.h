/* Include file for curve.c
 * Defines the Point class, and the Fp2 class
 * Ben Lynn
 */
/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/
#ifndef CURVE_H
#define CURVE_H

#include "fp2.h"

#ifdef __cplusplus
extern "C" {
#endif

struct curve_s {
    mpz_t p, q, p1onq;
    mpz_t cbrtpwr;
    mpz_t tatepwr;
    int solinasa; //holds Solinas decomposition.
    int solinasb;
//let a = abs(curve->solinasa), b = abs(curve->solinasb),
// s_a = sign(curve->solinasa), s_b = sign(curve->solinasb)
//this represents 2^a + s_b*2^b + s_a

    mpz_t *pre_x;
    mpz_t *pre_y;
};

typedef struct curve_s curve_t[1];

void curve_init(curve_t curve, mpz_t prime, mpz_t qprime);

void curve_clear(curve_t curve);

struct point_s {
    fp2_t x, y;
    int infinity;
};

typedef struct point_s point_t[1];
typedef struct point_s *point_ptr;

void point_init(point_ptr P);
//allocates memory for a point

size_t point_out_str(FILE *stream, int base, point_ptr P);

void point_clear(point_ptr P);
//deallocates memory for a point

void point_set(point_ptr src, point_ptr dst);
//src = dst

void point_set_O(point_ptr P);
//P = O

int point_equal(point_t P, point_t Q);
//P == Q?

void point_add(point_ptr R, point_ptr P, point_ptr Q, curve_t curve);
//R = P + Q

void x_from_y(mpz_t x, mpz_t y, curve_t curve);
//x = cuberoot(y^2 - 1)

void mympz_randomm(mpz_t x, mpz_t limit);
//x = random in {0, ..., limit - 1}

void fp2_random(fp2_ptr r, mpz_t p);
//r = random element of F_p^2

void general_point_random(point_ptr P, curve_t curve);
//P = random point on E/F_p^2

void point_random(point_ptr P, curve_t curve);
//P = random point

/*
void weil_pairing(fp2_ptr res, point_ptr P, point_ptr Q);
//res = e(P, Q) where e is the Weil pairing
*/

void tate_pairing(fp2_ptr res, point_ptr P, point_ptr Q, curve_t curve);
// res = e(P,Q) where e is the Tate pairing

void point_mul(point_ptr R, mpz_t n, point_ptr P, curve_t curve);
//R = nP
//P must lie on E/F_p, n must be positive

void general_point_mul(point_t R, mpz_t n, point_t P, curve_t curve);
//R = nP
//can handle P on E/F_p^2, any integer n

void point_mul_preprocess(point_ptr P, curve_t curve);
void point_mul_postprocess(point_ptr res, mpz_t n, curve_t curve);

int point_valid_p(point_t P, curve_t curve);
//returns 1 if P is a valid point on the curve
//(i.e. its coordinates satisfy the curve equation)

struct miller_cache_s {
    mpz_t *numa, *numc, *denomc;
    mpz_t denomsb;
    mpz_t denoms1;
    mpz_t numl1a, numl1c, denoml1c;
    mpz_t numl2c;
    int count;
};

typedef struct miller_cache_s miller_cache_t[1];

void miller_cache_init(miller_cache_t mc, curve_t curve);
void miller_cache_clear(miller_cache_t mc);

void tate_preprocess(miller_cache_t mc, point_ptr P, curve_t curve);
void miller_postprocess(fp2_ptr res, miller_cache_t mc, point_ptr Q, curve_t curve);
void tate_postprocess(fp2_ptr res, miller_cache_t mc, point_ptr Q, curve_t curve);

int simple_miller(fp2_ptr res, point_ptr P, point_ptr Phat,
	point_ptr Qhat, point_ptr R1, point_ptr R2, curve_t curve);

#ifdef __cplusplus
}
#endif

#endif //CURVE_H
