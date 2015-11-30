/* Computes Weil pairing, Tate pairing using Miller's algorithm
 * Ben Lynn
 *
 * For speed, point_random assumes the curve is y^2 = x^3 + 1 and p = 2 mod 3
 */

/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/

#include <stdlib.h>
#include "curve.h"
#include "benchmark.h"
#include "mm.h"
#include "crypto.h" //for random functions
#include <assert.h>

enum {
    //constants for sliding window algorithms
    windowsize = 5,
    windowsizepower = 15,	    //this is 2^(windowsize-1) - 1
};

void point_init(point_ptr P)
//allocates memory for a point
{
    fp2_init(P->x);
    fp2_init(P->y);
    P->infinity = 0;

    mm_tally("point", 1, "init");
}

void point_clear(point_ptr P)
//deallocates memory for a point
{
    fp2_clear(P->x);
    fp2_clear(P->y);
    mm_tally("point", -1, "clear");
}

size_t point_out_str(FILE *stream, int base, point_ptr P)
{
    FILE *fp;
    size_t s, status;
    if (!stream) fp = stdout;
    else fp = stream;
    status = fp2_out_str(fp, base, P->x);
    if (status < 0) return status;
    s = status;
    status = fprintf(fp, " ");
    if (status < 0) return status;
    s += status;
    status = fp2_out_str(fp, base, P->y);
    if (status < 0) return status;
    s += status;
    return s;
}

void point_set(point_ptr src, point_ptr dst)
//src = dst
{
    fp2_set(src->x, dst->x);
    fp2_set(src->y, dst->y);
    src->infinity = dst->infinity;
}

void point_set_O(point_ptr P)
//P = O
{
    fp2_set_0(P->x);
    fp2_set_1(P->y);
    P->infinity = 1;
}

int point_equal(point_t P, point_t Q)
//P == Q?
{
    if (P->infinity) return Q->infinity;
    return fp2_equal(P->x, Q->x) && fp2_equal(P->y, Q->y);
}

void curve_init(curve_t curve, mpz_t prime, mpz_t qprime)
//initializes system parameters
//not thread-safe
{
    int i;
    int m;
    int c0 = 0, c1;
    int count = 0;
    int j;

    mpz_init(curve->p);
    mpz_init(curve->q);
    mpz_init(curve->p1onq);
    mpz_init(curve->cbrtpwr);
    mpz_init(curve->tatepwr);

    mpz_set(curve->p, prime);
    mpz_set(curve->q, qprime);

    m = mpz_sizeinbase(curve->q, 2);

    //(uses NAF algorithm)
    for (i=0; i<=m; i++) {
	c1 = (mpz_tstbit(curve->q, i) + mpz_tstbit(curve->q, i+1) + c0) >> 1;
	j = mpz_tstbit(curve->q, i) + c0 - 2 * c1;
	if (j != 0) {
	    if (count >= 3) {
		curve->solinasa = 0;
		curve->solinasb = 0;
		break;
	    }
	    if (i == 0) {
		curve->solinasa = j;
	    } else if (count == 1) {
		curve->solinasb = i * j;
	    } else {
		curve->solinasa *= i;
	    }
	    count++;
	}
	c0 = c1;
    }
    if (count == 2) {
	curve->solinasa *= curve->solinasb;
	curve->solinasb = 0;
    }

    //printf("Solinas: a, b : %d %d \n", curve->solinasa, curve->solinasb);

    //(p + 1) / q
    mpz_add_ui(curve->p1onq, curve->p, 1);
    mpz_div(curve->p1onq, curve->p1onq, curve->q);

    //(2*p - 1)/3;
    mpz_mul_ui(curve->cbrtpwr, curve->p, 2);
    mpz_sub_ui(curve->cbrtpwr, curve->cbrtpwr, 1);
    mpz_div_ui(curve->cbrtpwr, curve->cbrtpwr, 3);

    //(p^2-1)/q
    // = (p-1)p1onq
    mpz_sub_ui(curve->tatepwr, curve->p, 1);
    mpz_mul(curve->tatepwr, curve->tatepwr, curve->p1onq);

    curve->pre_x = (mpz_t *) malloc(sizeof(mpz_t) * (m + 1));
    curve->pre_y = (mpz_t *) malloc(sizeof(mpz_t) * (m + 1));

    for (i=0; i<=m; i++) {
	mpz_init(curve->pre_x[i]);
	mpz_init(curve->pre_y[i]);
    }
}

void curve_clear(curve_t curve)
{
    int i;
    int m = mpz_sizeinbase(curve->q, 2);

    mpz_clear(curve->p);
    mpz_clear(curve->q);
    mpz_clear(curve->p1onq);
    mpz_clear(curve->cbrtpwr);
    mpz_clear(curve->tatepwr);

    for (i=0; i<=m; i++) {
	mpz_clear(curve->pre_x[i]);
	mpz_clear(curve->pre_y[i]);
    }
    free(curve->pre_x);
    free(curve->pre_y);
}

void miller_cache_init(miller_cache_t mc, curve_t curve)
{
    int i;
    int m = mpz_sizeinbase(curve->q, 2);

    mc->numa = (mpz_t *) malloc(sizeof(mpz_t) * m);
    mc->numc = (mpz_t *) malloc(sizeof(mpz_t) * m);
    mc->denomc = (mpz_t *) malloc(sizeof(mpz_t) * m);

    mpz_init(mc->denoms1);
    mpz_init(mc->denomsb);
    mpz_init(mc->numl1a);
    mpz_init(mc->numl1c);
    mpz_init(mc->denoml1c);
    mpz_init(mc->numl2c);
    for (i=0; i<m; i++) {
	mpz_init(mc->numa[i]);
	mpz_init(mc->numc[i]);
	mpz_init(mc->denomc[i]);
    }
    mc->count = m;
}

void miller_cache_clear(miller_cache_t mc)
{
    int i;
    int m = mc->count;

    mpz_clear(mc->denoms1);
    mpz_clear(mc->denomsb);
    mpz_clear(mc->numl1a);
    mpz_clear(mc->numl1c);
    mpz_clear(mc->denoml1c);
    mpz_clear(mc->numl2c);
    for (i=0; i<m; i++) {
	mpz_clear(mc->numa[i]);
	mpz_clear(mc->numc[i]);
	mpz_clear(mc->denomc[i]);
    }

    free(mc->numa);
    free(mc->numc);
    free(mc->denomc);

}

void x_from_y(mpz_t x, mpz_t y, curve_t curve)
{
    //x = cuberoot(y^2 - 1)
    mpz_mul(x, y, y);
    mpz_sub_ui(x, x, 1);
    mpz_mod(x, x, curve->p);
    mpz_powm(x, x, curve->cbrtpwr, curve->p);
}

void fp2_random(fp2_t r, mpz_t p)
//r = random element of F_p^2
{
    mympz_randomm(r->a, p);
    mympz_randomm(r->b, p);
}

void point_random(point_ptr P, curve_t curve)
//P = random point on E/F_p
{
    //this only works for p = 2 mod 3
    //and y^2 = x^3 + 1
    fp2_t x, y;

    fp2_init(x);
    fp2_init(y);

    mpz_set_ui(x->b, 0);
    mpz_set_ui(y->b, 0);

    mympz_randomm(y->a, curve->p);

    x_from_y(x->a, y->a, curve);

    fp2_set(P->x, x);
    fp2_set(P->y, y);

    fp2_clear(x);
    fp2_clear(y);
}

void general_point_random(point_ptr P, curve_t curve)
//P = random point on E/F_p^2
{
    fp2_t zeta;

    point_t P2;

    point_init(P2);

    point_random(P, curve);
    point_random(P2, curve);

    fp2_init(zeta);
    fp2_set_cbrt_unity(zeta, curve->p);
    
    fp2_mul(P2->x, P2->x, zeta, curve->p);
    point_add(P, P, P2, curve);

    point_clear(P2);
    fp2_clear(zeta);
}


void point_add(point_ptr R, point_ptr P, point_ptr Q, curve_t curve)
//R = P + Q
{
    mpz_ptr p = curve->p;
    fp2_t lambda, temp, temp2;

    if (P->infinity) {
	point_set(R, Q);
	return;
    }
    if (Q->infinity) {
	point_set(R, P);
	return;
    }

    R->infinity = 0;

    fp2_init(lambda);
    fp2_init(temp);
    fp2_init(temp2);

    if (fp2_equal(P->x, Q->x)) { // Px == Py
	fp2_neg(temp, Q->y, p);
	if (fp2_equal(P->y, temp)) { // Py == -Qy
	    point_set_O(R);
	} else { //Py == Qy
	    //line: Y - (lambda X + mu)

	    //lambda = (x * (x + x + x + *twicea2) + *a4) / (y + y);
	    //we assume twicea2 = 0, a4 = 0
	    fp2_add(lambda, P->x, P->x, p);
	    fp2_add(lambda, lambda, P->x, p);
	    fp2_mul(lambda, lambda, P->x, p);
	    fp2_add(temp, P->y, P->y, p);
	    fp2_div(lambda, lambda, temp, p);

	    //Rx = lambda^2 - 2Px
	    fp2_set(temp, P->x); //in case &P = &R

	    fp2_sqr(R->x, lambda, p);
	    fp2_add(temp2, temp, temp, p);
	    fp2_sub(R->x, R->x, temp2, p);

	    //Ry = (Px - Rx) * lambda - Py
	    fp2_sub(temp, temp, R->x, p);
	    fp2_mul(temp, temp, lambda, p);
	    fp2_sub(R->y, temp, P->y, p);
	}
    } else {
	//line: Y - (lambda X + mu)

	//lambda = (Qy - Py) / (Qx - Px);
	fp2_sub(lambda, Q->y, P->y, p);
	fp2_sub(temp, Q->x, P->x, p);
	fp2_div(lambda, lambda, temp, p);

	//Rx = lambda^2 - Px - Qx
	fp2_set(temp, P->x); //in case &P = &R

	fp2_sqr(temp2, lambda, p);
	fp2_sub(temp2, temp2, temp, p);
	fp2_sub(R->x, temp2, Q->x, p);

	//Ry = (Px - Rx) * lambda - Py
	fp2_sub(temp, temp, R->x, p);
	fp2_mul(temp, temp, lambda, p);
	fp2_sub(R->y, temp, P->y, p);

    }

    fp2_clear(lambda);
    fp2_clear(temp);
    fp2_clear(temp2);
}

static void proj_double(mpz_t x, mpz_t y, mpz_t z, mpz_t p)
//(x, y, z) *= 2
//see Blake, Seroussi & Smart, Fig IV.2
//assumes (x, y, z) is not O, or a point of order 2 (i.e. y != 0)
//we have a = 0 in our curve
{
    mpz_t t1, t2, t3, t4, t5;
    mpz_init(t1);
    mpz_init(t2);
    mpz_init(t3);
    mpz_init(t4);
    mpz_init(t5);

    //t1 = 3x^2
    mpz_mul(t1, x, x);
    mpz_add(t2, t1, t1);
    mpz_add(t1, t1, t2);
    mpz_mod(t1, t1, p);

    //z' = 2yz
    mpz_mul(z, z, y);
    mpz_add(z, z, z);
    mpz_mod(z, z, p);

    //t2 = 4xy^2, t5 holds y^2
    mpz_mul(t5, y, y);
    mpz_mod(t5, t5, p);
    mpz_mul(t2, t5, x);
    mpz_mul_2exp(t2, t2, 2);
    mpz_mod(t2, t2, p);

    //x' = t1^2 - 2t2
    mpz_mul(t3, t1, t1);
    //mpz_mod(t3, t3, p);
    mpz_add(t4, t2, t2);
    mpz_sub(x, t3, t4);
    mpz_mod(x, x, p);

    //t3 = 8y^2 (recall t5 holds y^2)
    mpz_mul(t3, t5, t5);
    //mpz_mod(t3, t3, p);
    mpz_mul_2exp(t3, t3, 3);
    mpz_mod(t3, t3, p);

    //y' = t1(t2 - x) - t3
    mpz_sub(t4, t2, x);
    mpz_mul(t4, t4, t1);
    mpz_sub(y, t4, t3);
    mpz_mod(y, y, p);

    mpz_clear(t1);
    mpz_clear(t2);
    mpz_clear(t3);
    mpz_clear(t4);
    mpz_clear(t5);
}

static void proj_mix_in(mpz_t x, mpz_t y, mpz_t z, mpz_t a, mpz_t b, mpz_t p)
//(x, y, z) += (a, b, 1)
//assumes neither is O, and they are distinct points
//for now also assume their sum is not O
//see Blake, Seroussi & Smart, Fig IV.1
{
    //we take z_2 = 1, so t1 = x, t4 = y

    mpz_t t2, t3, t5, t6, t7, t8;
    mpz_init(t2);
    mpz_init(t3);
    mpz_init(t5);
    mpz_init(t6);
    mpz_init(t7);
    mpz_init(t8);

    //lambda_2 = x_2 * z_1^2
    //t8 holds z^2 until t5 has been computed
    mpz_mul(t8, z, z);
    mpz_mod(t8, t8, p);
    mpz_mul(t2, t8, a);
    mpz_mod(t2, t2, p);

    //lambda_3 = lambda_1 - lambda_2
    mpz_sub(t3, x, t2);
    //if (!mpz_size(t3)) {
	//answer is O
    //}

    //lambda_5 = y_2 * z_1^3
    mpz_mul(t5, t8, z);
    mpz_mod(t5, t5, p);
    mpz_mul(t5, t5, b);
    mpz_mod(t5, t5, p);

    //lambda_6 = lambda_4 - lambda_5
    mpz_sub(t6, y, t5);

    //lambda_7 = lambda_1 + lambda_2
    mpz_add(t7, x, t2);

    //lambda_8 = lambda_4 + lambda_5
    mpz_add(t8, y, t5);

    //z_3 = z_1 z_2 lambda_3
    mpz_mul(z, z, t3);
    mpz_mod(z, z, p);

    //x_3 = lambda_6^2 - lambda_7 lambda_3^2
    //t2, t5 no longer needed
    //t2 holds t3^2
    mpz_mul(t5, t6, t6);
    mpz_mul(t2, t3, t3);
    mpz_mod(t2, t2, p);
    mpz_mul(x, t2, t7);
    mpz_sub(x, t5, x);
    mpz_mod(x, x, p);

    //lambda_9 = lambda_7 lambda_3^2 - 2 x_3
    //t5 doubles as t9
    //t7 no longer needed after first line
    mpz_mul(t5, t7, t2);
    mpz_add(t7, x, x);
    mpz_sub(t5, t5, t7);
    mpz_mod(t5, t5, p);

    //y_3 = (lambda_9 lambda_6 - lambda_8 lambda_3^3)/2
    //t8 no longer needed after second line
    mpz_mul(t7, t5, t6);
    mpz_mul(t8, t8, t2);
    mpz_mod(t8, t8, p);
    mpz_mul(t8, t8, t3);
    mpz_sub(y, t7, t8);
    mpz_mod(y, y, p);
    if (mpz_odd_p(y)) {
	mpz_add(y, y, p);
    }
    mpz_fdiv_q_2exp(y, y, 1);
    //is divexact better here?

    mpz_clear(t2);
    mpz_clear(t3);
    mpz_clear(t5);
    mpz_clear(t6);
    mpz_clear(t7);
    mpz_clear(t8);
}

static void tate_power(fp2_t res, curve_t curve)
{
    fp2_t t0;

    fp2_init(t0);

    fp2_pow(t0, res, curve->p1onq, curve->p);
    mpz_set(res->a, t0->a);
    mpz_sub(res->b, curve->p, t0->b);
    fp2_div(res, res, t0, curve->p);

    fp2_clear(t0);
}

static void pts_get_vertical(fp2_ptr v,
	point_ptr A, point_ptr P, mpz_ptr z, mpz_t p)
{
    mpz_t z2;
    fp2_t temp;

    mpz_init(z2);
    fp2_init(temp);

    assert(!A->infinity);
    assert(!P->infinity); // (could handle with a = b = 0; c = 1;)

    //a = 1; b = 0; c = -P.x;
    zp_mul(z2, z, z, p);
    fp2_mul_mpz(temp, A->x, z2, p);
    zp_sub(temp->a, temp->a, P->x->a, p);
    fp2_mul(v, v, temp, p);

    mpz_clear(z2);
    fp2_clear(temp);
}

static void pts_get_tangent(fp2_ptr v,
	point_ptr A, point_ptr P, mpz_t z, mpz_t p)
{
    //note: z is in F_p so we can multiply by arbitrary
    //powers of z because it gets killed by Tate exp.
    //to derive what I have: x --> x/z^2, y/z^3 and then multiply
    //by appropriate power of z
    mpz_t a, b, c;
    mpz_t temp, temp2;
    fp2_t f1, f2;

    fp2_init(f1);
    fp2_init(f2);
    //it should be
    //a = -slope_tangent(P.x, P.y);
    //b = 1;
    //c = -(P.y + a * P.x);
    //but we multiply by 2*P.y to avoid division

    assert(!P->infinity); //(could handle with a = b = 0; c = 1;)
    assert(!fp2_is_0(P->y)); // need to be wary of points of order 2
    //could handle with pts_get_vertical(v, A, P, z);

    mpz_init(a);
    mpz_init(b);
    mpz_init(c);
    mpz_init(temp);
    mpz_init(temp2);

    //a = -3x^2 * z^2 (assume a2 = a4 = 0)
#if 1
    zp_mul(temp2, P->x->a, P->x->a, p);
    zp_add(temp, temp2, temp2, p);
    zp_add(temp, temp, temp2, p);
    zp_neg(a, temp, p);
#else
    mpz_mul(temp2, P->x->a, P->x->a);
    mpz_add(temp, temp2, temp2);
    mpz_add(temp, temp, temp2);
    mpz_neg(a, temp);
    mpz_mod(a, a, p);
#endif
    zp_mul(temp2, z, z, p);
    zp_mul(a, a, temp2, p);

    zp_mul(temp2, temp2, z, p);

    //b = (y + y)z^3;
    zp_add(b, P->y->a, P->y->a, p);
    zp_mul(b, b, temp2, p);

    //c = - (2y) * y - (-3x^2) * x;
    zp_mul(c, temp, P->x->a, p);
    zp_mul(temp2, P->y->a, P->y->a, p);
    zp_add(temp2, temp2, temp2, p);
    zp_sub(c, c, temp2, p);

    assert(!A->infinity);

    fp2_mul_mpz(f1, A->x, a, p);
    fp2_mul_mpz(f2, A->y, b, p);
    fp2_add(f1, f1, f2, p);
    //zp_mul(b, A->y->a, b, p);
    //zp_add(f1->a, f1->a, b, p);
    zp_add(f1->a, f1->a, c, p);
    fp2_mul(v, v, f1, p);

    fp2_clear(f1);
    fp2_clear(f2);

    mpz_clear(a);
    mpz_clear(b);
    mpz_clear(c);
    mpz_clear(temp);
    mpz_clear(temp2);
    return;
}

static void tate_get_vertical(fp2_ptr v, point_ptr A, point_ptr P, mpz_t p)
{
    fp2_t temp;
    fp2_init(temp);

    if (A->infinity) {
	fprintf(stderr, "can't evaluate at O!\n");
	//fp2_set_0(v);
	return;
    }
    if (P->infinity) {
	//a = b = 0; c = 1;
	return;
    }
    //a = 1; b = 0; c = -P.x;
    fp2_sub(temp, A->x, P->x, p);
    fp2_mul(v, v, temp, p);

    fp2_clear(temp);
}

static void tate_get_tangent(fp2_ptr v, point_ptr A, point_ptr P, mpz_t p)
{
    fp2_t a, b, c;
    fp2_t temp, temp2;
    //it should be
    //a = -slope_tangent(P.x, P.y);
    //b = 1;
    //c = -(P.y + a * P.x);
    //but we multiply by 2*P.y to avoid division

    //a = -Px * (Px + Px + Px + *twicea2) - *a4;
    //assume a2 = a4 = 0

    if (P->infinity) {
	//a = b = 0; c = 1;
	return;
    }

    fp2_init(temp);

    //need to be wary of points of order 2
    if (fp2_is_0(P->y)) {
	//a = 1; b = 0; c = -P.x;
	fp2_sub(temp, A->x, P->x, p);
	fp2_mul(v, v, temp, p);

	fp2_clear(temp);
	return;
    }

    fp2_init(a);
    fp2_init(b);
    fp2_init(c);
    fp2_init(temp2);

    fp2_add(temp, P->x, P->x, p);
    fp2_add(temp, temp, P->x, p);
    fp2_mul(temp, temp, P->x, p);
    fp2_neg(a, temp, p);

    //b = Py + Py;
    fp2_add(b, P->y, P->y, p);

    //c = - b * Py - a * Px;
    fp2_mul(temp, b, P->y, p);
    fp2_mul(c, a, P->x, p);
    fp2_add(c, c, temp, p);
    fp2_neg(c, c, p);

    if (A->infinity) {
	fprintf(stderr, "can't evaluate at O!\n");
	//fp2_mul(v, v, b, p);
    } else {
	fp2_mul(temp, a, A->x, p);
	fp2_mul(temp2, b, A->y, p);
	fp2_add(temp, temp, temp2, p);
	fp2_add(temp, temp, c, p);
	fp2_mul(v, v, temp, p);
    }

    fp2_clear(a);
    fp2_clear(b);
    fp2_clear(c);
    fp2_clear(temp);
    fp2_clear(temp2);
    return;
}

static void tate_get_line(fp2_ptr v, point_ptr A, point_ptr P, point_ptr Q, mpz_t p)
{
    fp2_t a, b, c;
    fp2_t temp, temp2;

    //cases involving O
    if (P->infinity) {
	tate_get_vertical(v, A, Q, p);
	return;
    }
    if (Q->infinity) {
	tate_get_vertical(v, A, P, p);
	return;
    }

    fp2_init(temp);

    //check if we need a tangent or vertical
    if (fp2_equal(P->x, Q->x)) {
	fp2_neg(temp, Q->y, p);
	if (fp2_equal(P->y, temp)) {
	    //a = 1; b = 0; c = -P.x;
	    fp2_sub(temp, A->x, P->x, p);
	    fp2_mul(v, v, temp, p);

	    fp2_clear(temp);
	    return;
	}
	//othewise P = Q
	fp2_clear(temp);
	tate_get_tangent(v, A, P, p);
	return;

    }
    //normal case (P != Q)
    fp2_init(a);
    fp2_init(b);
    fp2_init(c);
    fp2_init(temp2);

    //it should be
    //a = -(Q.y - P.y) / (Q.x - P.x);
    //b = 1;
    //c = -(P.y + a * P.x);
    //but we'll multiply by Q.x - P.x to avoid division
    fp2_sub(b, Q->x, P->x, p);
    fp2_sub(a, P->y, Q->y, p);
    fp2_mul(temp, b, P->y, p);
    fp2_mul(c, a, P->x, p);
    fp2_add(c, c, temp, p);
    fp2_neg(c, c, p);

    if (A->infinity) {
	fprintf(stderr, "can't evaluate at O!\n");
    } else {
	fp2_mul(temp, a, A->x, p);
	fp2_mul(temp2, b, A->y, p);
	fp2_add(temp, temp, temp2, p);
	fp2_add(temp, temp, c, p);
	fp2_mul(v, v, temp, p);
    }

    fp2_clear(a);
    fp2_clear(b);
    fp2_clear(c);
    fp2_clear(temp);
    fp2_clear(temp2);
}

static void get_vertical(fp2_ptr v, fp2_ptr vdenom,
	point_ptr A, point_ptr B, point_ptr P, mpz_t p)
{
    fp2_t temp;
    fp2_init(temp);

    if (A->infinity || B->infinity) {
	fprintf(stderr, "can't evaluate at O!\n");
	//fp2_set_0(v);
	//fp2_set_0(vdenom);
	return;
    }
    if (P->infinity) {
	//a = b = 0; c = 1;
	return;
    }
    //a = 1; b = 0; c = -P.x;
    fp2_sub(temp, A->x, P->x, p);
    fp2_mul(v, v, temp, p);

    fp2_sub(temp, B->x, P->x, p);
    fp2_mul(vdenom, vdenom, temp, p);

    fp2_clear(temp);
}

static void get_tangent(fp2_ptr v, fp2_ptr vdenom,
	point_ptr A, point_ptr B, point_ptr P, mpz_t p)
{
    fp2_t a, b, c;
    fp2_t temp, temp2;
    //it should be
    //a = -slope_tangent(P.x, P.y);
    //b = 1;
    //c = -(P.y + a * P.x);
    //but we multiply by 2*P.y to avoid division

    //a = -Px * (Px + Px + Px + *twicea2) - *a4;
    //assume a2 = a4 = 0

    if (P->infinity) {
	//a = b = 0; c = 1;
	return;
    }

    fp2_init(temp);

    //need to be wary of points of order 2
    if (fp2_is_0(P->y)) {
	//a = 1; b = 0; c = -P.x;
	fp2_sub(temp, A->x, P->x, p);
	fp2_mul(v, v, temp, p);

	fp2_sub(temp, B->x, P->x, p);
	fp2_mul(vdenom, vdenom, temp, p);

	fp2_clear(temp);
	return;
    }

    fp2_init(a);
    fp2_init(b);
    fp2_init(c);
    fp2_init(temp2);

    fp2_add(temp, P->x, P->x, p);
    fp2_add(temp, temp, P->x, p);
    fp2_mul(temp, temp, P->x, p);
    fp2_neg(a, temp, p);

    //b = Py + Py;
    fp2_add(b, P->y, P->y, p);

    //c = - b * Py - a * Px;
    fp2_mul(temp, b, P->y, p);
    fp2_mul(c, a, P->x, p);
    fp2_add(c, c, temp, p);
    fp2_neg(c, c, p);

    if (A->infinity) {
	fprintf(stderr, "can't evaluate at O!\n");
    } else {
	fp2_mul(temp, a, A->x, p);
	fp2_mul(temp2, b, A->y, p);
	fp2_add(temp, temp, temp2, p);
	fp2_add(temp, temp, c, p);
	fp2_mul(v, v, temp, p);
    }
    if (B->infinity) {
	fprintf(stderr, "can't evaluate at O!\n");
    } else {
	fp2_mul(temp, a, B->x, p);
	fp2_mul(temp2, b, B->y, p);
	fp2_add(temp, temp, temp2, p);
	fp2_add(temp, temp, c, p);
	fp2_mul(vdenom, vdenom, temp, p);
    }

    fp2_clear(a);
    fp2_clear(b);
    fp2_clear(c);
    fp2_clear(temp);
    fp2_clear(temp2);
    return;
}

static void get_line(fp2_ptr v, fp2_ptr vdenom,
	point_ptr A, point_ptr B, point_ptr P, point_ptr Q, mpz_t p)
{
    fp2_t a, b, c;
    fp2_t temp, temp2;

    //cases involving O
    if (P->infinity) {
	get_vertical(v, vdenom, A, B, Q, p);
	return;
    }
    if (Q->infinity) {
	get_vertical(v, vdenom, A, B, P, p);
	return;
    }

    fp2_init(temp);

    //check if we need a tangent or vertical
    if (fp2_equal(P->x, Q->x)) {
	fp2_neg(temp, Q->y, p);
	if (fp2_equal(P->y, temp)) {
	    //a = 1; b = 0; c = -P.x;
	    fp2_sub(temp, A->x, P->x, p);
	    fp2_mul(v, v, temp, p);

	    fp2_sub(temp, B->x, P->x, p);
	    fp2_mul(vdenom, vdenom, temp, p);

	    fp2_clear(temp);
	    return;
	}
	//othewise P = Q
	fp2_clear(temp);
	get_tangent(v, vdenom, A, B, P, p);
	return;

    }
    //normal case (P != Q)
    fp2_init(a);
    fp2_init(b);
    fp2_init(c);
    fp2_init(temp2);

    //it should be
    //a = -(Q.y - P.y) / (Q.x - P.x);
    //b = 1;
    //c = -(P.y + a * P.x);
    //but we'll multiply by Q.x - P.x to avoid division
    fp2_sub(b, Q->x, P->x, p);
    fp2_sub(a, P->y, Q->y, p);
    fp2_mul(temp, b, P->y, p);
    fp2_mul(c, a, P->x, p);
    fp2_add(c, c, temp, p);
    fp2_neg(c, c, p);

    if (A->infinity) {
	fprintf(stderr, "can't evaluate at O!\n");
	//fp2_mul(v, v, b, p);
    } else {
	fp2_mul(temp, a, A->x, p);
	fp2_mul(temp2, b, A->y, p);
	fp2_add(temp, temp, temp2, p);
	fp2_add(temp, temp, c, p);
	fp2_mul(v, v, temp, p);
    }
    if (B->infinity) {
	fprintf(stderr, "can't evaluate at O!\n");
	//fp2_mul(vdenom, vdenom, b, p);
    } else {
	fp2_mul(temp, a, B->x, p);
	fp2_mul(temp2, b, B->y, p);
	fp2_add(temp, temp, temp2, p);
	fp2_add(temp, temp, c, p);
	fp2_mul(vdenom, vdenom, temp, p);
    }

    fp2_clear(a);
    fp2_clear(b);
    fp2_clear(c);
    fp2_clear(temp);
    fp2_clear(temp2);
}

int simple_miller(fp2_ptr res, point_ptr P, point_ptr Phat,
	point_ptr Qhat, point_ptr R1, point_ptr R2, curve_t curve)
{
    int m;
    fp2_t f1;
    fp2_t v, vdenom;
    point_t Z;
    mpz_ptr p = curve->p;

    point_init(Z);
    point_set_O(Z);

    fp2_init(f1);
    fp2_init(v);
    fp2_init(vdenom);

    fp2_set_1(v);
    fp2_set_1(vdenom);

    get_vertical(v, vdenom, Qhat, R2, Phat, p);
    get_line(v, vdenom, R2, Qhat, P, R1, p);

    fp2_div(f1, v, vdenom, p);

    fp2_set_1(v);
    fp2_set_1(vdenom);

    m = mpz_sizeinbase(curve->q, 2) - 1;
    for(;;) {
	if (mpz_tstbit(curve->q, m)) {
	    fp2_mul(v, v, f1, p);
	    //g
	    get_line(v, vdenom, Qhat, R2, Z, P, p);
	    //h
	    point_add(Z, Z, P, curve);
	    get_vertical(v, vdenom, R2, Qhat, Z, p);
	}
	if (m == 0) break;
	m--;
	fp2_sqr(v, v, p);
	fp2_sqr(vdenom, vdenom, p);
	//g
	get_line(v, vdenom, Qhat, R2, Z, Z, p);
	//h
	point_add(Z, Z, Z, curve);
	get_vertical(v, vdenom, R2, Qhat, Z, p);
    }
    fp2_div(res, v, vdenom, p);

    fp2_clear(f1);
    fp2_clear(v);
    fp2_clear(vdenom);

    point_clear(Z);

    return 1;
}

int general_miller(fp2_ptr res, point_ptr P, point_ptr Phat,
	point_ptr Qhat, point_ptr R1, point_ptr R2, curve_t curve)
{
    //TODO: use NAF
    //use sliding-window method
    fp2_t g[2*windowsizepower+2];
    point_t bP[2*windowsizepower+2];

    fp2_t vdenom, v;
    point_t Z;
    int i, j, k, l, m;
    mpz_ptr p = curve->p;

    fp2_init(vdenom);
    fp2_init(v);

    fp2_set_1(v);
    fp2_set_1(vdenom);

    //work out f_1
    get_vertical(v, vdenom, Qhat, R2, Phat, p);
    get_line(v, vdenom, R2, Qhat, P, R1, p);
    if (fp2_is_0(v) || fp2_is_0(vdenom)) goto bad_R;
    fp2_init(g[1]);
    fp2_div(g[1], v, vdenom, p);
    point_init(bP[1]);
    point_set(bP[1], P);
    fp2_set_1(v);
    fp2_set_1(vdenom);

    //work out f_2
    point_init(bP[2]);
    point_add(bP[2], P, P, curve);
    //get_line(v, vdenom, Qhat, R2, P, P);
    get_tangent(v, vdenom, Qhat, R2, P, p);
    get_vertical(v, vdenom, R2, Qhat, bP[2], p);
    if (fp2_is_0(v) || fp2_is_0(vdenom)) goto bad_R;
    fp2_init(g[2]);
    fp2_sqr(g[2], g[1], p);
    fp2_mul(g[2], g[2], v, p);
    fp2_div(g[2], g[2], vdenom, p);
    fp2_set_1(v);
    fp2_set_1(vdenom);

    //work out rest of g[], bP[]
    for (i=1; i<=windowsizepower; i++) {
	j = 2 * i + 1;
	k = j - 2;
	point_init(bP[j]);
	point_add(bP[j], bP[k], bP[2], curve);
	get_line(v, vdenom, Qhat, R2, bP[k], bP[2], p);
	get_vertical(v, vdenom, R2, Qhat, bP[j], p);
	if (fp2_is_0(v) || fp2_is_0(vdenom)) goto bad_R;
	fp2_init(g[j]);
	fp2_mul(g[j], g[k], g[2], p);
	fp2_mul(g[j], g[j], v, p);
	fp2_div(g[j], g[j], vdenom, p);

	fp2_set_1(v);
	fp2_set_1(vdenom);
    }

    point_init(Z);
    point_set_O(Z);

    m = mpz_sizeinbase(curve->q, 2) - 1;
    while(m>=0) {
	if (!mpz_tstbit(curve->q, m)) {
	    fp2_sqr(v, v, p);
	    fp2_sqr(vdenom, vdenom, p);
	    get_tangent(v, vdenom, Qhat, R2, Z, p);
	    point_add(Z, Z, Z, curve);
	    get_vertical(v, vdenom, R2, Qhat, Z, p);
	    m--;
	} else {
	    l = m - windowsize + 1;
	    if (l < 0) l = 0;
	    l = mpz_scan1(curve->q, l);
	    j = 1;
	    fp2_sqr(v, v, p);
	    fp2_sqr(vdenom, vdenom, p);
	    get_tangent(v, vdenom, Qhat, R2, Z, p);
	    point_add(Z, Z, Z, curve);
	    get_vertical(v, vdenom, R2, Qhat, Z, p);
	    for (k = m - 1; k>=l; k--) {
		j = j << 1;
		if (mpz_tstbit(curve->q, k)) j++;
		fp2_sqr(v, v, p);
		fp2_sqr(vdenom, vdenom, p);
		get_tangent(v, vdenom, Qhat, R2, Z, p);
		point_add(Z, Z, Z, curve);
		get_vertical(v, vdenom, R2, Qhat, Z, p);
	    }
	    fp2_mul(v, v, g[j], p);
	    get_line(v, vdenom, Qhat, R2, Z, bP[j], p);
	    point_add(Z, Z, bP[j], curve);
	    get_vertical(v, vdenom, R2, Qhat, Z, p);
	    m = l-1;
	}
    }

    if (fp2_is_0(v) || fp2_is_0(vdenom)) goto bad_R;
    else fp2_div(res, v, vdenom, p);

    // check with simple algorithm
    if (0) {
	fp2_t check;

	fp2_init(check);

	simple_miller(check, P, Phat, Qhat, R1, R2, curve);

	if (!fp2_equal(check, res)) {
	    printf("res check = ");
	    fp2_out_str(NULL, 0, res);
	    printf(" ");
	    fp2_out_str(NULL, 0, check);
	    printf("\n");
	    fprintf(stderr, "BUG: miller() is incorrectly implemented\n");
	    //fp2_set(res, check);
	}

	fp2_clear(check);
    }

    fp2_clear(v);
    fp2_clear(vdenom);
    point_clear(Z);

    fp2_clear(g[2]);
    point_clear(bP[2]);
    for (i=0; i<=windowsizepower; i++) {
	j = 2*i + 1;
	point_clear(bP[j]);
	fp2_clear(g[j]);
    }

    return 1;

bad_R:
    printf("bad R\n");

    fp2_clear(v);
    fp2_clear(vdenom);

    /* TODO: memory leak if jumped here after g[] inited
    point_clear(Z);
    fp2_clear(g[2]);
    point_clear(bP[2]);
    for (i=0; i<=windowsizepower; i++) {
	j = 2*i + 1;
	point_clear(bP[j]);
	fp2_clear(g[j]);
    }
    */

    return 0;
}

static void pts_preprocess_vertical(miller_cache_t mc,
	int i, point_t P, mpz_t z, mpz_t p)
{
    mpz_t t0;

    mpz_init(t0);

    mpz_mul(t0, z, z);
    mpz_invert(t0, t0, p);

    mpz_mul(t0, t0, P->x->a);
    mpz_neg(t0, t0);
    mpz_mod(mc->denomc[i], t0, p);

    mpz_clear(t0);
}

void pts_preprocess_tangent(miller_cache_t mc,
	int i, point_t P, mpz_t z, mpz_t p)
{
    mpz_t t0;
    mpz_t *numa, *numc;

    mpz_init(t0);
    numa = mc->numa;
    numc = mc->numc;

    assert(mpz_cmp_ui(P->y->a, 0)); //assume P not of order 2
    //could handle with:
    //{
    //	mpz_set_ui(numa[i], 1);
    //	mpz_set(numc[i], P->x->a);
    //	return;
    //}

    //a = -3x^2/2yz
    //c = -(y + a * xz)/z^3;

    mpz_mul_2exp(t0, P->y->a, 1);
    mpz_mul(t0, t0, z);
    mpz_invert(t0, t0, p);
    mpz_mul(t0, t0, P->x->a);
    mpz_mul(t0, t0, P->x->a);
    mpz_mul_si(t0, t0, -3);
    mpz_mod(numa[i], t0, p);

    mpz_mul(t0, z, z);
    mpz_mul(t0, t0, z);
    mpz_invert(t0, t0, p);
    mpz_mul(numc[i], numa[i], P->x->a);
    mpz_mul(numc[i], numc[i], z);
    mpz_add(numc[i], numc[i], P->y->a);
    mpz_neg(numc[i], numc[i]);
    mpz_mul(t0, numc[i], t0);
    mpz_mod(numc[i], t0, p);

    mpz_clear(t0);
}

void pts_preprocess_line(mpz_t a, mpz_t c, point_t P, point_t Q, mpz_t p)
{
    mpz_t t0;

    mpz_init(t0);

    //assume P.x != Q.x

    //a = -(Q.y - P.y) / (Q.x - P.x);
    //c = -(P.y + a * P.x);

    mpz_sub(c, Q->x->a, P->x->a);
    mpz_invert(c, c, p);
    mpz_sub(a, P->y->a, Q->y->a);
    mpz_mul(a, a, c);
    mpz_mod(a, a, p);

    mpz_mul(c, a, P->x->a);
    mpz_add(c, c, P->y->a);
    mpz_neg(c, c);
    mpz_mod(c, c, p);

    mpz_clear(t0);
}

void tate_preprocess(miller_cache_t mc, point_ptr P, curve_t curve)
//for primes of the form 2^a +- 2^b +- 1
//uses proj. coords, assumes P is a point over F_p
//and that order of group = Solinas prime
{
    //specialized for Solinas primes
    int a, b;
    point_t bP;

    point_t Z;
    int i;
    mpz_t z, temp;
    mpz_ptr p = curve->p;

    mpz_init(z);
    mpz_init(temp);
    mpz_set_ui(z, 1);

    a = abs(curve->solinasa);
    b = abs(curve->solinasb);

    point_init(Z);
    point_init(bP);

    point_set(Z, P);
    i = 0;
    if (b != 0) {
	//work out f_2^b
	for(;i<b; i++) {
	    //g
	    //tate_get_tangent(v, Qhat, Z, p);
	    //pts_get_tangent(v, Qhat, Z, z);
	    pts_preprocess_tangent(mc, i, Z, z, p);
	    //h
	    //point_add(Z, Z, Z, curve);
	    proj_double(Z->x->a, Z->y->a, z, p);
	    //tate_get_vertical(vdenom, Qhat, Z, p);
	    //pts_get_vertical(vdenom, Qhat, Z, z, p);
	    pts_preprocess_vertical(mc, i, Z, z, p);
	}

	mpz_invert(z, z, p);
	zp_mul(temp, z, z, p);
	fp2_mul_mpz(Z->x, Z->x, temp, p);
	zp_mul(temp, temp, z, p);
	fp2_mul_mpz(Z->y, Z->y, temp, p);
	mpz_set_ui(z, 1);

	point_set(bP, Z);

	if (curve->solinasb < 0) {
	    //tate_get_vertical(fbdenom, Qhat, Z);
	    mpz_neg(mc->denomsb, Z->x->a);
	    fp2_neg(bP->y, bP->y, p);
	}
    }

    //work out f_2^a
    for(; i<a; i++) {
	//g
	//pts_get_tangent(v, Qhat, Z, z);
	pts_preprocess_tangent(mc, i, Z, z, p);
	//h
	proj_double(Z->x->a, Z->y->a, z, p);
	//pts_get_vertical(vdenom, Qhat, Z, z);
	pts_preprocess_vertical(mc, i, Z, z, p);
    }

    mpz_invert(z, z, p);
    zp_mul(temp, z, z, p);
    fp2_mul_mpz(Z->x, Z->x, temp, p);
    zp_mul(temp, temp, z, p);
    fp2_mul_mpz(Z->y, Z->y, temp, p);
    mpz_set_ui(z, 1);

    //work out f_(2^a +- 2^b +- 1)
    if (b != 0) {
	//g
	//tate_get_line(v, Qhat, Z, bP);
	pts_preprocess_line(mc->numl1a, mc->numl1c, Z, bP, p);
	//h
	point_add(Z, Z, bP, curve);
	//tate_get_vertical(vdenom, Qhat, Z);
	mpz_sub(mc->denoml1c, p, Z->x->a);
    }
    //the sign of solinasa records whether it's +1 or -1
    if (curve->solinasa < 0) {
	//tate_get_vertical(vdenom, Qhat, Z);
	mpz_sub(mc->denoms1, p, Z->x->a);
    }

    //g
    //tate_get_line(v, Qhat, Z, cP);
    //now Z = -cP so g is vertical
    mpz_sub(mc->numl2c, p, Z->x->a);
    //h
    //point_add(Z, Z, cP, curve);
    //now Z = O, so h = 1
    //tate_get_vertical(vdenom, Qhat, Z);

    point_clear(Z);
    point_clear(bP);

    mpz_clear(z);
    mpz_clear(temp);
}

void miller_postprocess(fp2_ptr res, miller_cache_t mc,
	point_ptr Q, curve_t curve)
//for primes of the form 2^a +- 2^b +- 1
//uses proj. coords, assumes P is a point over F_p
//and that order of group = Solinas prime
{
    //specialized for Solinas primes
    int a, b;
    fp2_t fb, fbdenom;
    fp2_t t0;
    fp2_t vdenom, v;
    int i;
    mpz_t *numa, *numc;
    mpz_t *denomc;
    mpz_ptr p = curve->p;

    numa = mc->numa;
    numc = mc->numc;
    denomc = mc->denomc;

    a = abs(curve->solinasa);
    b = abs(curve->solinasb);

    fp2_init(t0);
    fp2_init(vdenom);
    fp2_init(v);
    fp2_init(fb);
    fp2_init(fbdenom);

    fp2_set_1(v);
    fp2_set_1(vdenom);

    //f_1 = 1

    i = 0;
    if (b != 0) {
	//work out f_2^b
	for(;i<b; i++) {
	    fp2_sqr(v, v, p);
	    fp2_sqr(vdenom, vdenom, p);
	    //g
	    //tate_get_tangent(v, Qhat, Z, p);
	    //pts_get_tangent(v, Qhat, Z, z, p);
	    {
		fp2_mul_mpz(t0, Q->x, numa[i], p);
		fp2_add(t0, t0, Q->y, p);
		mpz_add(t0->a, t0->a, numc[i]);
		fp2_mul(v, v, t0, p);
	    }
	    //h
	    //tate_get_vertical(vdenom, Qhat, Z, p);
	    //pts_get_vertical(vdenom, Qhat, Z, z, p);
	    {
		fp2_set(t0, Q->x);
		mpz_add(t0->a, t0->a, denomc[i]);
		fp2_mul(vdenom, vdenom, t0, p);
	    }
	}

	if (curve->solinasb < 0) {
	    fp2_set(fbdenom, v);
	    fp2_set(fb, vdenom);
	    //tate_get_vertical(fbdenom, Qhat, Z, p);
	    {
		fp2_set(t0, Q->x);
		mpz_add(t0->a, t0->a, mc->denomsb);
		fp2_mul(fbdenom, fbdenom, t0, p);
	    }
	} else {
	    fp2_set(fb, v);
	    fp2_set(fbdenom, vdenom);
	}
    }

    //work out f_2^a
    for(; i<a; i++) {
	fp2_sqr(v, v, p);
	fp2_sqr(vdenom, vdenom, p);
	//g
	//pts_get_tangent(v, Qhat, Z, z, p);
	{
	    fp2_mul_mpz(t0, Q->x, numa[i], p);
	    fp2_add(t0, t0, Q->y, p);
	    mpz_add(t0->a, t0->a, numc[i]);
	    fp2_mul(v, v, t0, p);
	}
	//h
	//pts_get_vertical(vdenom, Qhat, Z, z, p);
	{
	    fp2_set(t0, Q->x);
	    mpz_add(t0->a, t0->a, denomc[i]);
	    fp2_mul(vdenom, vdenom, t0, p);
	}
    }

    //work out f_(2^a +- 2^b +- 1)
    if (b != 0) {
	fp2_mul(v, v, fb, p);
	fp2_mul(vdenom, vdenom, fbdenom, p);
	//g
	//tate_get_line(v, Qhat, Z, bP, p);
	{
	    fp2_mul_mpz(t0, Q->x, mc->numl1a, p);
	    fp2_add(t0, t0, Q->y, p);
	    mpz_add(t0->a, t0->a, mc->numl1c);
	    fp2_mul(v, v, t0, p);
	}
	//h
	//tate_get_vertical(vdenom, Qhat, Z, p);
	{
	    fp2_set(t0, Q->x);
	    mpz_add(t0->a, t0->a, mc->denoml1c);
	    fp2_mul(vdenom, vdenom, t0, p);
	}
    }

    if (curve->solinasa < 0) {
    //the sign of solinasa records whether it's +1 or -1
	//tate_get_vertical(vdenom, Qhat, P);
	{
	    fp2_set(t0, Q->x);
	    mpz_add(t0->a, t0->a, mc->denoms1);
	    fp2_mul(vdenom, vdenom, t0, p);
	}
    }

    //g
    //tate_get_line(v, Qhat, Z, cP);
    {
	fp2_set(t0, Q->x);
	mpz_add(t0->a, t0->a, mc->numl2c);
	fp2_mul(v, v, t0, p);
    }
    //h
    //tate_get_vertical(vdenom, Qhat, Z);
    fp2_div(res, v, vdenom, p);

    fp2_clear(v);
    fp2_clear(vdenom);
    fp2_clear(fb);
    fp2_clear(fbdenom);
    fp2_clear(t0);
}

void tate_postprocess(fp2_ptr res, miller_cache_t mc, point_ptr Q, curve_t curve)
{
    bm_put(bm_get_time(), "miller0");
    miller_postprocess(res, mc, Q, curve);
    bm_put(bm_get_time(), "miller1");
    tate_power(res, curve);
}

void tate_solinas_miller(fp2_ptr res, point_ptr P, point_ptr Qhat, curve_t curve)
//for primes of the form 2^a +- 2^b +- 1
//uses proj. coords, assumes P is a point over F_p
//and that order of group = Solinas prime
{
    //specialized for Solinas primes
    int a, b;
    fp2_t fb, fbdenom;
    point_t bP;

    fp2_t vdenom, v;
    point_t Z;
    int i;
    mpz_t z, temp;
    mpz_ptr p = curve->p;

    mpz_init(z);
    mpz_init(temp);
    mpz_set_ui(z, 1);

    a = abs(curve->solinasa);
    b = abs(curve->solinasb);

    point_init(Z);
    point_init(bP);
    fp2_init(vdenom);
    fp2_init(v);
    fp2_init(fb);
    fp2_init(fbdenom);

    fp2_set_1(v);
    fp2_set_1(vdenom);

    //f_1 = 1

    point_set(Z, P);
    i = 0;
    if (b != 0) {
	//work out f_2^b
	for(;i<b; i++) {
	    fp2_sqr(v, v, p);
	    fp2_sqr(vdenom, vdenom, p);
	    //g
	    //tate_get_tangent(v, Qhat, Z);
	    pts_get_tangent(v, Qhat, Z, z, p);
	    //h
	    //point_add(Z, Z, Z, curve);
	    proj_double(Z->x->a, Z->y->a, z, p);
	    //tate_get_vertical(vdenom, Qhat, Z);
	    pts_get_vertical(vdenom, Qhat, Z, z, p);
	}

	mpz_invert(z, z, p);
	zp_mul(temp, z, z, p);
	fp2_mul_mpz(Z->x, Z->x, temp, p);
	zp_mul(temp, temp, z, p);
	fp2_mul_mpz(Z->y, Z->y, temp, p);
	mpz_set_ui(z, 1);

	point_set(bP, Z);

	if (curve->solinasb < 0) {
	    fp2_set(fbdenom, v);
	    fp2_set(fb, vdenom);
	    tate_get_vertical(fbdenom, Qhat, Z, p);
	    fp2_neg(bP->y, bP->y, p);
	} else {
	    fp2_set(fb, v);
	    fp2_set(fbdenom, vdenom);
	}
    }

    //work out f_2^a
    for(; i<a; i++) {
	fp2_sqr(v, v, p);
	fp2_sqr(vdenom, vdenom, p);
	//g
	pts_get_tangent(v, Qhat, Z, z, p);
	//h
	proj_double(Z->x->a, Z->y->a, z, p);
	pts_get_vertical(vdenom, Qhat, Z, z, p);
    }

    mpz_invert(z, z, p);
    zp_mul(temp, z, z, p);
    fp2_mul_mpz(Z->x, Z->x, temp, p);
    zp_mul(temp, temp, z, p);
    fp2_mul_mpz(Z->y, Z->y, temp, p);
    mpz_set_ui(z, 1);

    //work out f_(2^a +- 2^b +- 1)
    if (b != 0) {
	fp2_mul(v, v, fb, p);
	fp2_mul(vdenom, vdenom, fbdenom, p);
	//g
	tate_get_line(v, Qhat, Z, bP, p);
	//h
	point_add(Z, Z, bP, curve);
	tate_get_vertical(vdenom, Qhat, Z, p);
    }

    if (curve->solinasa < 0) {
    //the sign of solinasa records whether it's +1 or -1
	tate_get_vertical(vdenom, Qhat, P, p);
    }

    //g
    tate_get_vertical(v, Qhat, Z, p);
    //h
    fp2_div(res, v, vdenom, p);

    point_clear(Z);
    point_clear(bP);
    fp2_clear(v);
    fp2_clear(vdenom);
    fp2_clear(fb);
    fp2_clear(fbdenom);
    mpz_clear(z);
    mpz_clear(temp);
}

int point_valid_p(point_t P, curve_t curve)
{
    int result = 1;
    mpz_ptr p = curve->p;
    fp2_t error;
    fp2_t temp;

    if (P->infinity) return result;

    fp2_init(error);
    fp2_init(temp);
    fp2_mul(error, P->x, P->x, p);
    fp2_mul(error, error, P->x, p);
    fp2_mul(temp, P->y, P->y, p);
    fp2_sub(error, temp, error, p);
    fp2_sub_ui(error, error, 1, p);

    if (mpz_cmp_ui(error->a, 0) || mpz_cmp_ui(error->b, 0)) {
	//printf("error = ");
	//fp2_out_str(NULL, 0, error);
	//printf("\n");
	result = 0;
    }

    fp2_clear(error);
    fp2_clear(temp);
    return result;
}

int point_special_p(point_t P, curve_t curve)
//check that P is in E/F_p and finite
{
    int result = 1;
    mpz_ptr p = curve->p;
    mpz_t error;
    mpz_t temp;

    if (P->infinity) return 0;

    if (mpz_cmp_ui(P->x->b, 0)) return 0;
    if (mpz_cmp_ui(P->y->b, 0)) return 0;

    mpz_init(error);
    mpz_init(temp);
    mpz_mul(error, P->x->a, P->x->a);
    mpz_mod(error, error, p);
    mpz_mul(error, error, P->x->a);
    mpz_mod(error, error, p);

    mpz_mul(temp, P->y->a, P->y->a);
    mpz_sub(error, temp, error);
    mpz_sub_ui(error, error, 1);
    mpz_mod(error, error, p);

    if (mpz_cmp_ui(error, 0)) {
	//printf("error = ");
	//fp2_out_str(NULL, 0, error);
	//printf("\n");
	result = 0;
    }

    mpz_clear(error);
    mpz_clear(temp);
    return result;
}

void tate_pairing(fp2_ptr res, point_ptr P, point_ptr Q, curve_t curve)
// res = e(P,Q) where e is the Tate pairing
// assume P in E/F_p
{
    bm_put(bm_get_time(), "miller0");
    tate_solinas_miller(res, P, Q, curve);
    bm_put(bm_get_time(), "miller1");

    tate_power(res, curve);
}

#if 0
void weil_pairing(fp2_ptr res, point_ptr P, point_ptr Q)
// res = e(P,Q) where e is the Weil pairing
{
    point_t Phat, Qhat;
    fp2_t num, denom;

    point_init(Phat);
    point_init(Qhat);
    fp2_init(num);
    fp2_init(denom);

    if (!miller_randomized_flag) {
	new_random_Rs();
    }

    for(;;) {
	point_add(Phat, P, curve->R1, curve);
	point_add(Qhat, Q, curve->R2, curve);
	if (!solinas_miller(num, P, Phat, Qhat, curve->R1, curve->R2)) goto millerfail;
	if (!solinas_miller(denom, Q, Qhat, Phat, curve->R2, curve->R1)) goto millerfail;
	break;
millerfail:
	new_random_Rs();
    }

    point_clear(Phat);
    point_clear(Qhat);
    fp2_div(res, num, denom, p);
    fp2_clear(num);
    fp2_clear(denom);
}
#endif

void zzp_point_double(mpz_t x, mpz_t y, mpz_t Px, mpz_t Py, mpz_t p)
//(x,y,1) = 2(Px, Py,1)
//assume points are on E/F_p
//and we don't expect to get O, either as input or output
{
    //line: Y - (lambda X + mu)
    //we assume a2 = a4 = 0
    mpz_t lambda, temp;
    mpz_init(lambda);
    mpz_init(temp);

    mpz_add(lambda, Px, Px);
    mpz_add(lambda, lambda, Px);
    mpz_mul(lambda, lambda, Px);
    mpz_mod(lambda, lambda, p);
    mpz_add(temp, Py, Py);
    mpz_invert(temp, temp, p);
    mpz_mul(lambda, lambda, temp);
    mpz_mod(lambda, lambda, p);

    mpz_set(temp, Px);
    mpz_mul(x, lambda, lambda);
    mpz_sub(x, x, temp);
    mpz_sub(x, x, temp);
    mpz_mod(x, x, p);

    mpz_sub(y, temp, x);
    mpz_mul(y, y, lambda);
    mpz_sub(y, y, Py);
    mpz_mod(y, y, p);

    mpz_clear(temp);
    mpz_clear(lambda);
}

void zzp_point_add(mpz_t x, mpz_t y, mpz_t Px, mpz_t Py,
	mpz_t Qx, mpz_t Qy, mpz_t p)
//(x, y) = (Px, Py) + (Qx, Qy)
//assume points are on E/F_p, assume output != O
{
    mpz_t lambda, temp;
    mpz_init(lambda);
    mpz_init(temp);

    //line: Y - (lambda X + mu)
    mpz_sub(lambda, Qy, Py);
    mpz_sub(temp, Qx, Px);
    mpz_invert(temp, temp, p);
    mpz_mul(lambda, lambda, temp);
    mpz_mod(lambda, lambda, p);

    mpz_set(temp, Px);
    mpz_mul(x, lambda, lambda);
    mpz_sub(x, x, temp);
    mpz_sub(x, x, Qx);
    mpz_mod(x, x, p);

    mpz_sub(y, temp, x);
    mpz_mul(y, y, lambda);
    mpz_sub(y, y, Py);
    mpz_mod(y, y, p);

    mpz_clear(temp);
    mpz_clear(lambda);
}

void point_mul_preprocess(point_ptr P, curve_t curve)
//get ready for multiplications on P
//set up signed sliding-windowS
{
    int i;
    int m;

    assert(point_special_p(P, curve));

    m = mpz_sizeinbase(curve->q, 2);

    //compute x[1], y[1]
    mpz_set(curve->pre_x[0], P->x->a);
    mpz_set(curve->pre_y[0], P->y->a);

    for (i=1; i<=m; i++) {
	zzp_point_double(curve->pre_x[i], curve->pre_y[i], curve->pre_x[i-1], curve->pre_y[i-1], curve->p);
    }
}

void point_mul_postprocess(point_ptr R, mpz_t n, curve_t curve)
//R = nP (P has been preprocessed)
//P must lie on E/F_p, n must be positive
//assumes nP != O (?)
//uses signed sliding-window method
{
    //compute NAF form (see Blake, Seroussi & Smart IV.2.4)
    int j;
    int m = mpz_sizeinbase(n, 2);
    int *s = (int *) malloc(sizeof(int) * (m+1));
    int c0 = 0, c1;

    mpz_t Rx, Ry, Rz;
    mpz_t x0, y0;
    mpz_ptr p = curve->p;

    assert(mpz_cmp_ui(n, 0) > 0);
    assert(mpz_cmp(n, curve->q) < 0);

    for (j=0; j<=m; j++) {
	c1 = (mpz_tstbit(n, j) + mpz_tstbit(n, j+1) + c0) >> 1;
	s[j] = mpz_tstbit(n, j) + c0 - 2 * c1;
	c0 = c1;
    }

    mpz_init(Rx); mpz_init(Ry); mpz_init(Rz);
    mpz_init(x0); mpz_init(y0);

    //first bit might not be used after NAF conversion
    if (!s[m]) m--;

    mpz_set_ui(Rz, 1);
    //now work in projective coordinates

    mpz_set(Rx, curve->pre_x[m]);
    mpz_set(Ry, curve->pre_y[m]);

    m--;
    while(m>=0) {
	if (s[m]) {
	    if (s[m] < 0) {
		mpz_sub(x0, p, curve->pre_y[m]);
		proj_mix_in(Rx, Ry, Rz, curve->pre_x[m], x0, p);
	    } else {
		proj_mix_in(Rx, Ry, Rz, curve->pre_x[m], curve->pre_y[m], p);
	    }
	}
	m--;
    }

    //convert back to affine
    mpz_invert(Rz, Rz, p);

    mpz_mul(x0, Rz, Rz);
    mpz_mod(x0, x0, p);
    mpz_mul(Rx, Rx, x0);
    mpz_mod(Rx, Rx, p);
    mpz_mul(y0, x0, Rz);
    mpz_mod(y0, y0, p);
    mpz_mul(Ry, Ry, y0);
    mpz_mod(Ry, Ry, p);

    mpz_set_ui(R->x->b, 0);
    mpz_set_ui(R->y->b, 0);
    mpz_set(R->x->a, Rx);
    mpz_set(R->y->a, Ry);
    R->infinity = 0;

    mpz_clear(Rx); mpz_clear(Ry); mpz_clear(Rz);
    mpz_clear(x0); mpz_clear(y0);
    free(s);
}

void point_mul(point_ptr R, mpz_t n, point_ptr P, curve_t curve)
//R = nP
//P must lie on E/F_p, 0 < n
//uses signed sliding-window method
//TODO: handle cases when you get O's during the computation
{
    //compute NAF form (see Blake, Seroussi & Smart IV.2.4)
    int i, j, k, l;
    int m = mpz_sizeinbase(n, 2);
    int *s = (int *) malloc(sizeof(int) * (m+1));
    int c0 = 0, c1;

    mpz_t x[2*windowsizepower+2];
    mpz_t y[2*windowsizepower+2];
    mpz_t Rx, Ry, Rz;
    mpz_ptr p = curve->p;

    assert(mpz_cmp_ui(n, 0) > 0);
    assert(point_special_p(P, curve));

    for (j=0; j<=m; j++) {
	c1 = (mpz_tstbit(n, j) + mpz_tstbit(n, j+1) + c0) >> 1;
	s[j] = mpz_tstbit(n, j) + c0 - 2 * c1;
	c0 = c1;
    }

    mpz_init(Rx); mpz_init(Ry); mpz_init(Rz);

    if (0) {
	if (mpz_size(P->x->b) || mpz_size(P->y->b) || n <= 0) {
	    fprintf(stderr, "BUG: point_mul can't handle points not on E/F_p\n");
	    //exit(1);
	}
    }

    mpz_init(y[0]); //we need a temp. variable later

    //compute x[1], y[1]
    mpz_init(x[1]);
    mpz_init(y[1]);
    mpz_set(x[1], P->x->a);
    mpz_set(y[1], P->y->a);

    //compute x[2], y[2]
    mpz_init(x[2]);
    mpz_init(y[2]);
    zzp_point_double(x[2], y[2], x[1], y[1], p);

    for (i=1; i<=windowsizepower; i++) {
	j = 2 * i + 1;
	k = j - 2;
	mpz_init(x[j]);
	mpz_init(y[j]);
	zzp_point_add(x[j], y[j], x[k], y[k], x[2], y[2], p);
    }

    //first bit might not be used after NAF conversion
    if (!s[m]) m--;

    mpz_set_ui(Rz, 1);
    //now work in projective coordinates

    l = m - windowsize + 1;
    if (l < 0) l = 0;
    for (; l<m; l++) {
	if (s[l]) break;
    }
    j = s[l];
    i = 1;
    for (k=l+1; k<=m; k++) {
	i = i << 1;
	j += s[k] * i;
    }
    if (j < 0) {
	j = -j;
	mpz_set(Rx, x[j]);
	//assumes y[j] != 0
	mpz_sub(Ry, p, y[j]);
    } else {
	mpz_set(Rx, x[j]);
	mpz_set(Ry, y[j]);
    }

    m = l-1;

    while(m>=0) {

	if (!s[m]) {
	    proj_double(Rx, Ry, Rz, p);
	    m--;
	} else {
	    l = m - windowsize + 1;
	    if (l < 0) l = 0;
	    for (; l<m; l++) {
		if (s[l]) break;
	    }
	    j = s[l];
	    proj_double(Rx, Ry, Rz, p);
	    i = 1;
	    for (k=l+1; k<=m; k++) {
		i = i << 1;
		j += s[k] * i;
		proj_double(Rx, Ry, Rz, p);
	    }
	    if (j < 0) {
		j = -j;
		mpz_sub(y[0], p, y[j]);
		proj_mix_in(Rx, Ry, Rz, x[j], y[0], p);
	    } else {
		proj_mix_in(Rx, Ry, Rz, x[j], y[j], p);
	    }
	    m = l-1;
	}
    }

    //convert back to affine
    mpz_invert(Rz, Rz, p);

    //x[1], y[1] no longer needed
    mpz_mul(x[1], Rz, Rz);
    mpz_mod(x[1], x[1], p);
    mpz_mul(Rx, Rx, x[1]);
    mpz_mod(Rx, Rx, p);
    mpz_mul(y[1], x[1], Rz);
    mpz_mod(y[1], y[1], p);
    mpz_mul(Ry, Ry, y[1]);
    mpz_mod(Ry, Ry, p);

    mpz_set_ui(R->x->b, 0);
    mpz_set_ui(R->y->b, 0);
    mpz_set(R->x->a, Rx);
    mpz_set(R->y->a, Ry);
    R->infinity = 0;

    mpz_clear(Rx); mpz_clear(Ry); mpz_clear(Rz);

    mpz_clear(x[2]); mpz_clear(y[2]);
    for (i=0; i<=windowsizepower; i++) {
	j = 2 * i + 1;
	mpz_clear(x[j]); mpz_clear(y[j]);
    }
    mpz_clear(y[0]);
    free(s);
}

void general_point_mul(point_t Q, mpz_t a, point_t P, curve_t curve)
//Q = aP
//can handle P on E/F_p^2, any integer a
{
    point_t R;
    int i, j, k, l, m;
    mpz_t n;

    point_t g[2*windowsizepower+2];

    mpz_init(n);

    point_init(R);
    point_set_O(R);

    mpz_mod(n, a, curve->q);

    if (P->infinity) {
	point_set_O(Q);
	return;
    }
    if (!mpz_cmp_ui(a, 0)) {
	point_set_O(Q);
	return;
    }

    assert(point_valid_p(P, curve));

    //use sliding-window method
    point_init(g[1]);
    point_set(g[1], P);
    point_init(g[2]);
    point_add(g[2], P, P, curve);

    for (i=1; i<=windowsizepower; i++) {
	j = 2 * i + 1;
	point_init(g[j]);
	point_add(g[j], g[j-2], g[2], curve);
    }

    m = mpz_sizeinbase(n, 2) - 1;

    while(m>=0) {
	if (!mpz_tstbit(n, m)) {
	    point_add(R, R, R, curve);
	    m--;
	} else {
	    l = m - windowsize + 1;
	    if (l < 0) l = 0;
	    l = mpz_scan1(n, l);
	    j = 1;
	    point_add(R, R, R, curve);
	    for (k = m - 1; k>=l; k--) {
		j = j << 1;
		if (mpz_tstbit(n, k)) j++;
		point_add(R, R, R, curve);
	    }
	    point_add(R, R, g[j], curve);
	    m = l-1;
	}
    }
    /* check (repeated squaring) */
    if (0) {
	point_t Rcheck;
	point_init(Rcheck);
	point_set_O(Rcheck);
	m = mpz_sizeinbase(n, 2) - 1;
	for(;;) {
	    if (mpz_tstbit(n, m)) {
		point_add(Rcheck, Rcheck, P, curve);
	    }
	    if (m == 0) break;
	    point_add(Rcheck, Rcheck, Rcheck, curve);
	    m--;
	}
	if (!point_equal(R, Rcheck)) {
	    fprintf(stderr, "point_mul(): BUG!\n");
	    //exit(1);
	}
    }

    point_clear(g[2]);
    for (i=0; i<=windowsizepower; i++) {
	j = 2 * i + 1;
	point_clear(g[j]);
    }

    mpz_clear(n);

    point_set(Q, R);
    point_clear(R);
}
