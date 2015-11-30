/* F_p^2 routines
 * Ben Lynn
 */
/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/
#include <stdlib.h>
#include <string.h>
#include "fp2.h"
#include "mm.h"

enum {
    //constants for sliding window algorithms
    windowsize = 5,
    windowsizepower = 15	    //this is 2^(windowsize - 1) - 1
};

//we represent elements of F_p^2 with
//a + sqrt(-1)b where a, b lie in F_p

//these are inefficient?
inline void zp_mul(mpz_t x, mpz_t a, mpz_t b, mpz_t p)
//x = a * b mod p
{
    mpz_mul(x, a, b);
    mpz_mod(x, x, p);
}

inline void zp_add(mpz_t x, mpz_t a, mpz_t b, mpz_t p)
//x = a + b mod p
{
    mpz_add(x, a, b);
    if (mpz_cmp(x, p) >= 0) mpz_sub(x, x, p);
    /*
    mpz_add(x, a, b);
    mpz_mod(x, x, p);
    */
}

inline void zp_neg(mpz_t x, mpz_t b, mpz_t p)
//x = -b mod p
{
    if (mpz_sgn(b)) {
	mpz_sub(x, p, b);
    }
    /*
    mpz_sub(x, a, b);
    mpz_mod(x, x, p);
    */
}

inline void zp_sub(mpz_t x, mpz_t a, mpz_t b, mpz_t p)
//x = a - b mod p
{
    if (mpz_cmp(a, b) >= 0) {
	mpz_sub(x, a, b);
    } else {
	mpz_sub(x, a, b);
	mpz_add(x, x, p);
    }
    /*
    mpz_sub(x, a, b);
    mpz_mod(x, x, p);
    */
}

void fp2_init(fp2_ptr x)
//call before using x
//allocates space for x
{
    mm_tally("fp2", 1, "init");
    mpz_init(x->a);
    mpz_init(x->b);
}

void fp2_clear(fp2_ptr x)
//deallocates space for x
{
    mm_tally("fp2", -1, "clear");
    mpz_clear(x->a);
    mpz_clear(x->b);
}

void fp2_set(fp2_ptr x, fp2_ptr y)
//assignment
//x = y
{
    mpz_set(x->a, y->a);
    mpz_set(x->b, y->b);
}

void fp2_set_0(fp2_ptr x)
//x = 0
{
    mpz_set_ui(x->a, 0);
    mpz_set_ui(x->b, 0);
}

void fp2_set_1(fp2_ptr x)
//x = 1
{
    mpz_set_ui(x->a, 1);
    mpz_set_ui(x->b, 0);
}

void fp2_set_cbrt_unity(fp2_ptr x, mpz_t p)
{
    //Warning: depends on representation of F_p^2
    //Z_p[sqrt(-1)] version:
    mpz_t p1on4;

    mpz_set(x->a, p);
    mpz_sub_ui(x->a, x->a, 1);
    mpz_div_ui(x->a, x->a, 2);

    mpz_init_set(p1on4, p);
    mpz_add_ui(p1on4, p1on4, 1);
    mpz_div_ui(p1on4, p1on4, 4);
    mpz_set_ui(x->b, 3);
    mpz_powm(x->b, x->b, p1on4, p);
    //inefficient way of halving
    mpz_mul(x->b, x->b, x->a);
    mpz_mod(x->b, x->b, p);
    mpz_clear(p1on4);

    //Z_p[sqrt(-3)] version:
    //mpz_set(x->a, p);
    //mpz_sub_ui(x->a, x->a, 1);
    //mpz_div_ui(x->a, x->a, 2);
    //mpz_set(x->b, x->a);
}

void fp2_init_set(fp2_ptr x, fp2_ptr y)
//for convenience
//combines fp2_init, fp2_set
{
    fp2_init(x);
    fp2_set(x, y);
}

void fp2_neg(fp2_ptr x, fp2_ptr a, mpz_t p)
//x = -a
{
    mpz_sub(x->a, p, a->a);
    mpz_sub(x->b, p, a->b);
}

void fp2_add(fp2_ptr x, fp2_ptr a, fp2_ptr b, mpz_t p)
//x = a + b
{
    zp_add(x->a, a->a, b->a, p);
    zp_add(x->b, a->b, b->b, p);
}

void fp2_sub(fp2_ptr x, fp2_ptr a, fp2_ptr b, mpz_t p)
//x = a - b
{
    zp_sub(x->a, a->a, b->a, p);
    zp_sub(x->b, a->b, b->b, p);
}

void fp2_sub_ui(fp2_ptr x, fp2_ptr a, unsigned long b, mpz_t p)
//x = a - b
{
    mpz_sub_ui(x->a, a->a, b);
    mpz_mod(x->a, x->a, p);
}

void fp2_sqr(fp2_ptr x, fp2_ptr a, mpz_t p)
//x = a * a
{
    mpz_t t0, t1; //temp variables
    mpz_init(t0); mpz_init(t1);

    //sqr_mod(t0, a->a);
    //sqr_mod(t1, a->b);
    //zp_sub(t0, t0, t1, p);

    /*
    zp_sub(t0, a->a, a->b, p);
    zp_add(t1, a->a, a->b, p);
    zp_mul(t0, t0, t1, p);
    zp_mul(t1, a->a, a->b, p);
    zp_add(x->b, t1, t1, p);
    mpz_set(x->a, t0);
    */
    mpz_sub(t0, a->a, a->b);
    mpz_add(t1, a->a, a->b);
    mpz_mul(t0, t0, t1);
    mpz_mul(t1, a->a, a->b);
    mpz_mul_2exp(x->b, t1, 1);
    mpz_mod(x->b, x->b, p);
    mpz_mod(x->a, t0, p);

    mpz_clear(t0); mpz_clear(t1);
}

void fp2_mul_mpz(fp2_ptr x, fp2_ptr a, mpz_ptr b, mpz_t p)
//x = a * b
{
    zp_mul(x->a, a->a, b, p);
    zp_mul(x->b, a->b, b, p);
}

void fp2_mul(fp2_ptr x, fp2_ptr a, fp2_ptr b, mpz_t p)
//x = a * b
{
    mpz_t t0, t1, t2; //temp variables
    mpz_init(t0); mpz_init(t1); mpz_init(t2);

    /* This is slower
    zp_mul(t0, a->a, b->a);
    zp_mul(t1, a->b, b->b);
    zp_sub(t0, t0, t1);
    zp_mul(t1, a->a, b->b);
    zp_mul(t2, a->b, b->a);
    zp_add(x->b, t1, t2);
    mpz_set(x->a, t0);
    */
    mpz_mul(t0, a->a, b->a);
    mpz_mul(t1, a->b, b->b);
    mpz_sub(t0, t0, t1);
    mpz_mul(t1, a->a, b->b);
    mpz_mul(t2, a->b, b->a);
    mpz_add(x->b, t1, t2);
    mpz_mod(x->b, x->b, p);
    mpz_mod(x->a, t0, p);

    mpz_clear(t0); mpz_clear(t1); mpz_clear(t2);
}

void fp2_inv(fp2_ptr x, fp2_ptr b, mpz_t p)
//x = 1 / b
{
    mpz_t t0, t1; //temp variables
    mpz_t t2;
    mpz_init(t2);
    mpz_init(t0); mpz_init(t1);

    /*
    zp_mul(t0, b->a, b->a);
    zp_mul(t1, b->b, b->b);
    zp_add(t0, t0, t1);
    inv_mod(t1, t0);

    zp_mul(t0, t1, b->b);
    zp_neg(x->b, t0);
    zp_mul(x->a, t1, b->a);
    */
    mpz_mul(t0, b->a, b->a);
    mpz_mul(t1, b->b, b->b);
    mpz_add(t0, t0, t1);
    mpz_invert(t1, t0, p);

    mpz_mul(t0, t1, b->b);
    mpz_neg(x->b, t0);
    mpz_mod(x->b, x->b, p);
    mpz_mul(x->a, t1, b->a);
    mpz_mod(x->a, x->a, p);

    mpz_clear(t0); mpz_clear(t1);
    mpz_clear(t2);
}

void fp2_div(fp2_ptr x, fp2_ptr a, fp2_ptr b, mpz_t p)
//x = a / b
{
    mpz_t t0, t1, t2, t3; //temp variables
    mpz_init(t0); mpz_init(t1); mpz_init(t2); mpz_init(t3);

    /*
    zp_mul(t0, b->a, b->a);
    zp_mul(t1, b->b, b->b);
    zp_add(t0, t0, t1);
    inv_mod(t1, t0);

    zp_mul(t0, a->a, b->a);
    zp_mul(t2, a->b, b->b);
    zp_add(t0, t0, t2);
    zp_mul(t2, a->b, b->a);
    zp_mul(t3, a->a, b->b);
    zp_sub(t2, t2, t3);
    zp_mul(x->b, t1, t2);
    zp_mul(x->a, t1, t0);
    */
    mpz_mul(t0, b->a, b->a);
    mpz_mul(t1, b->b, b->b);
    mpz_add(t0, t0, t1);
    mpz_invert(t1, t0, p);

    mpz_mul(t0, a->a, b->a);
    mpz_mul(t2, a->b, b->b);
    mpz_add(t0, t0, t2);
    mpz_mul(t2, a->b, b->a);
    mpz_mul(t3, a->a, b->b);
    mpz_sub(t2, t2, t3);
    mpz_mul(x->b, t1, t2);
    mpz_mod(x->b, x->b, p);
    mpz_mul(x->a, t1, t0);
    mpz_mod(x->a, x->a, p);

    mpz_clear(t0); mpz_clear(t1); mpz_clear(t2); mpz_clear(t3);
}

void fp2_pow(fp2_ptr res, fp2_ptr x, mpz_t n, mpz_t p)
//exponentiation
//res = x^n
{
    int i, j, k, l, m;
    mpz_t t0, t1; //temp variables

    //use sliding-window method
    fp2_t g[2*windowsizepower+2];
    fp2_init_set(g[1], x);
    fp2_init(g[2]);
    fp2_mul(g[2], x, x, p);

    mpz_init(t0); mpz_init(t1);

    for (i=1; i<=windowsizepower; i++) {
	j = 2 * i + 1;
	fp2_init(g[j]);
	fp2_mul(g[j], g[j - 2], g[2], p);
    }
    m = mpz_sizeinbase(n, 2) - 1;

    fp2_set_1(res);

    while(m>=0) {
	if (!mpz_tstbit(n, m)) {
	    { //fp2_sqr(res,res);
		mpz_sub(t0, res->a, res->b);
		mpz_add(t1, res->a, res->b);
		mpz_mul(t0, t0, t1);
		mpz_mul(t1, res->a, res->b);
		mpz_mul_2exp(res->b, t1, 1);
		mpz_mod(res->b, res->b, p);
		mpz_mod(res->a, t0, p);
	    }
	    m--;
	} else {
	    l = m - windowsize + 1;
	    if (l < 0) l = 0;
	    l = mpz_scan1(n, l);
	    j = 1;
	    { //fp2_sqr(res,res);
		mpz_sub(t0, res->a, res->b);
		mpz_add(t1, res->a, res->b);
		mpz_mul(t0, t0, t1);
		mpz_mul(t1, res->a, res->b);
		mpz_mul_2exp(res->b, t1, 1);
		mpz_mod(res->b, res->b, p);
		mpz_mod(res->a, t0, p);
	    }
	    for (k = m - 1; k>=l; k--) {
		j = j << 1;
		if (mpz_tstbit(n, k)) j++;
		{ //fp2_sqr(res,res);
		    mpz_sub(t0, res->a, res->b);
		    mpz_add(t1, res->a, res->b);
		    mpz_mul(t0, t0, t1);
		    mpz_mul(t1, res->a, res->b);
		    mpz_mul_2exp(res->b, t1, 1);
		    mpz_mod(res->b, res->b, p);
		    mpz_mod(res->a, t0, p);
		}
	    }
	    fp2_mul(res, res, g[j], p);
	    m = l-1;
	}
    }

    fp2_clear(g[2]);
    for (i=0; i<=windowsizepower; i++) {
	j = 2 * i + 1;
	fp2_clear(g[j]);
    }

    mpz_clear(t0); mpz_clear(t1);

    /* simple method:
    int m = mpz_sizeinbase(n, 2) - 1;
    fp2_t x1;

    fp2_init_set(x1, x);

    for(;;) {
	if (mpz_tstbit(n, m)) {
	    fp2_mul(res, res, x1);
	}
	if (m == 0) break;
	fp2_mul(res, res, res);
	m--;
    }
    */
}

int fp2_is_0(fp2_ptr x)
// x == 0?
{
    return (!(mpz_size(x->a) || mpz_size(x->b)));
}

int fp2_equal(fp2_ptr x, fp2_ptr y)
// x == y?
{
    return (!(mpz_cmp(x->a, y->a) || mpz_cmp(x->b, y->b)));
}

size_t fp2_out_str(FILE *stream, int base, fp2_ptr x)
//output x on stream in given base
{
    FILE *fp;
    size_t s, status;

    if (!stream) fp = stdout;
    else fp = stream;
    status = fprintf(fp, "[");
    if (status < 0) return status;
    s = status;
    status = mpz_out_str(fp, base, x->a);
    if (status < 0) return status;
    s += status;
    status = fprintf(fp, " ");
    if (status < 0) return status;
    s += status;
    status = mpz_out_str(fp, base, x->b);
    if (status < 0) return status;
    s += status;
    status = fprintf(fp, "]");
    if (status < 0) return status;
    s += status;
    return s;
}

int fp2_set_str(fp2_t x, char *s, int base)
//set value of x from s, a NULL-terminated C string, in base `base'
//returns -1 on error
{
    int len;
    char *sp;
    char *buf;

    len = strlen(s);
    fp2_set_0(x);

    if (!len) return -1;
    if (s[0] != '[') return -1;
    if (s[len - 1] != ']') return -1;
    buf = (char *) alloca(len - 1);
    memcpy(buf, &s[1], len - 2);
    buf[len - 2] = 0;
    sp = strchr(buf, ' ');
    if (!sp) return -1;

    *sp = 0;
    sp++;
    if (mpz_set_str(x->a, buf, base)) return -1;
    if (mpz_set_str(x->b, sp, base)) return -1;

    return 0;
}
