/* Operations in F_p^2
 * header file
 * Ben Lynn
 */
/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/
#ifndef FP2_H
#define FP2_H

#include <stdio.h>
#include <gmp.h>

#ifdef __cplusplus
extern "C" {
#endif

//for F_p^2 we use the representation a + sqrt(-nqr)b
struct fp2_s {
    mpz_t a, b;
};

typedef struct fp2_s fp2_t[1];
typedef struct fp2_s *fp2_ptr;

//these assume the arguments are in {0,...,p-1}

void zp_add(mpz_t x, mpz_t a, mpz_t b, mpz_t p);
//x = a + b mod p

void zp_neg(mpz_t x, mpz_t b, mpz_t p);
//x = -b mod p

void zp_sub(mpz_t x, mpz_t a, mpz_t b, mpz_t p);
//x = a - b mod p

void zp_mul(mpz_t x, mpz_t a, mpz_t b, mpz_t p);
//x = a * b mod p

void fp2_init(fp2_ptr x);
//call before using x
//allocates space for x

void fp2_clear(fp2_ptr x);
//deallocates space for x

void fp2_set(fp2_ptr x, fp2_ptr y);
//x = y

void fp2_set_0(fp2_ptr x);
//x = 0

void fp2_set_1(fp2_ptr x);
//x = 1

void fp2_set_cbrt_unity(fp2_ptr x, mpz_t p);
//x = nonunit cube root of unity

void fp2_init_set(fp2_ptr x, fp2_ptr y);
//for convenience
//combines fp2_init, fp2_set

void fp2_neg(fp2_ptr x, fp2_ptr a, mpz_t p);
//x = -a

void fp2_add(fp2_ptr x, fp2_ptr a, fp2_ptr b, mpz_t p);
//x = a + b

void fp2_sub(fp2_ptr x, fp2_ptr a, fp2_ptr b, mpz_t p);
//x = a - b

void fp2_sub_ui(fp2_ptr x, fp2_ptr a, unsigned long b, mpz_t p);
//x = a - b

void fp2_sqr(fp2_ptr x, fp2_ptr a, mpz_t p);
//x = a * a

void fp2_mul_mpz(fp2_ptr x, fp2_ptr a, mpz_ptr b, mpz_t p);
//x = a * b

void fp2_mul(fp2_ptr x, fp2_ptr a, fp2_ptr b, mpz_t p);
//x = a * b

void fp2_inv(fp2_ptr x, fp2_ptr b, mpz_t p);
//x = 1 / b

void fp2_div(fp2_ptr x, fp2_ptr a, fp2_ptr b, mpz_t p);
//x = a / b

void fp2_pow(fp2_ptr res, fp2_ptr x, mpz_t n, mpz_t p);
//res = x^n

int fp2_equal(fp2_ptr x, fp2_ptr y);
//returns true if x equals y, false otherwise

int fp2_is_0(fp2_ptr x);
//returns true if x equals 0, false otherwise

size_t fp2_out_str(FILE *stream, int base, fp2_ptr x);
//output x on stream in base base

int fp2_set_str(fp2_t x, char *s, int base);
//set value of x from s, a NULL-terminated C string, in base `base'
//returns -1 on error

#ifdef __cplusplus
}
#endif

#endif //FP2_H
