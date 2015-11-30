
enum {

    See LICENSE for license
    qbits = 10
};

static mpz_t p, q, p1onq;
static mpz_t nq;
static mpz_t moq;
static mpz_t curve_a, curve_b;
static gmp_randstate_t rand_state;

static void evaluate_curve(mpz_t z, mpz_t x, mpz_t y)
{
    mpz_t t0, t1;

    mpz_init(t0); mpz_init(t1);

    mpz_mul(t1, y, y);
    mpz_mul(t0, x, x);
    mpz_add(t0, t0, curve_a);
    mpz_mul(t0, t0, x);
    mpz_add(t0, t0, curve_b);
    mpz_sub(t1, t1, t0);
    mpz_mod(z, t1, p);

    mpz_clear(t0); mpz_clear(t1);
}

static void point_add(mpz_t x3, mpz_t y3,
	mpz_t x1, mpz_t y1,
	mpz_t x2, mpz_t y2)
{
    mpz_t t0, t1;
    mpz_t l;

    mpz_init(t0); mpz_init(t1);
    mpz_init(l);

    mpz_sub(t0, x2, x1);
    if (!mpz_invert(t0, t0, p)) {
	printf("divide by zero!\n");
	exit(1);
    }
    mpz_sub(l, y2, y1);
    mpz_mul(l, l, t0);
    mpz_mod(l, l, p);

    mpz_mul(t0, l, l);
    mpz_sub(t0, t0, x1);
    mpz_sub(t0, t0, x2);
    mpz_mod(t0, t0, p);

    mpz_sub(t1, x1, t0);
    mpz_mul(t1, t1, l);
    mpz_sub(t1, t1, y1);

    mpz_set(x3, t0);
    mpz_mod(y3, t1, p);

    mpz_clear(l);
    mpz_clear(t0); mpz_clear(t1);
}

static void point_double(mpz_t x3, mpz_t y3, mpz_t x1, mpz_t y1)
{
    mpz_t t0, t1;
    mpz_t l;

    mpz_init(t0); mpz_init(t1);
    mpz_init(l);

    mpz_mul(l, x1, x1);
    mpz_mul_ui(l, l, 3);
    mpz_add(l, l, curve_a);

    mpz_add(t0, y1, y1);
    if (!mpz_invert(t0, t0, p)) {
	printf("/0!\n");
	exit(1);
    }
    mpz_mul(l, l, t0);
    mpz_mod(l, l, p);

    mpz_mul(t0, l, l);
    mpz_sub(t0, t0, x1);
    mpz_sub(t0, t0, x1);
    mpz_mod(t0, t0, p);

    mpz_sub(t1, x1, t0);
    mpz_mul(t1, t1, l);
    mpz_sub(t1, t1, y1);

    mpz_set(x3, t0);
    mpz_mod(y3, t1, p);

    mpz_clear(l);
    mpz_clear(t0); mpz_clear(t1);
}

static void point_mul(mpz_t x3, mpz_t y3, mpz_t a, mpz_t x1, mpz_t y1)
{
    int j;
    mpz_t t0, t1;

    mpz_init(t0); mpz_init(t1);

    j = mpz_sizeinbase(a, 2) - 1;

    mpz_set(t0, x1);
    mpz_set(t1, y1);
    j--;
    while(j >= 0) {
	point_double(t0, t1, t0, t1);
	if (mpz_tstbit(a, j)) {
	    point_add(t0, t1, t0, t1, x1, y1);
	}
	j--;
    }
    mpz_set(x3, t0);
    mpz_set(y3, t1);

    mpz_clear(t0); mpz_clear(t1);
}

static void tonelli(mpz_t x, mpz_t a)
{
    int i, m;
    int r, e;
    mpz_t qq;
    mpz_t y;
    mpz_t b;
    mpz_t t;

    mpz_init(qq);
    mpz_init(y);
    mpz_init(b);
    mpz_init(t);
    mpz_sub_ui(qq, p, 1);
    e = mpz_scan1(qq, 0);
    mpz_tdiv_q_2exp (qq, qq, e);

    mpz_powm(y, nq, qq, p);
    r = e;
    //(q-1)/2
    mpz_tdiv_q_2exp(qq, qq, 1);
    mpz_powm(x, a, qq, p);
    mpz_mul(b, x, x);
    mpz_mul(b, b, a);
    mpz_mod(b, b, p);
    mpz_mul(x, x, a);
    mpz_mod(x, x, p);
    while (mpz_cmp_ui(b, 1)) {
	m = 1;
	mpz_set(t, b);
	for (;;) {
	    mpz_mul(t, t, t);
	    mpz_mod(t, t, p);
	    if (!mpz_cmp_ui(t, 1)) break;
	    m++;
	}
	mpz_set(t, y);
	for (i=0; i<r-m-1; i++) {
	    mpz_mul(t, t, t);
	    mpz_mod(t, t, p);
	}
	mpz_mul(y, t, t);
	mpz_mod(y, y, p);
	r = m;
	mpz_mul(x, x, t);
	mpz_mod(x, x, p);
	mpz_mul(b, b, y);
	mpz_mod(b, b, p);

    }

    mpz_clear(qq);
    mpz_clear(y);
    mpz_clear(b);
    mpz_clear(t);
}

static void point_random(mpz_t x, mpz_t y)
{
    mpz_t t0;

    mpz_init(t0);
    for (;;) {
	mpz_urandomm(x, rand_state, p);
	mpz_mul(t0, x, x);
	mpz_add(t0, t0, curve_a);
	mpz_mul(t0, t0, x);
	mpz_add(t0, t0, curve_b);
	if (mpz_legendre(t0, p) == 1) break;
    }
    tonelli(y, t0);

    mpz_clear(t0);
}

void make_order_q(mpz_t x, mpz_t y)
{
    point_mul(x, y, moq, x, y);
}

static void find_curve()
{
    mpz_t x, y;
    mpz_t x2, y2;
    mpz_t j, c;
    mpz_t n;

    mpz_init(j); mpz_init(c);
    mpz_init(x); mpz_init(y);
    mpz_init(x2); mpz_init(y2);
    mpz_init(n);

    mpz_set_ui(n, 1);
    mpz_mul_2exp(n, n, qbits);

    do {
	mpz_urandomm(q, rand_state, n);
	mpz_nextprime(q, q);

	//printf("q: "); mpz_out_str(stdout, 10, q); printf("\n");
	mpz_mul(p, q, q);
	mpz_mul_ui(p, p, 28);
	mpz_add_ui(p, p, 1);
	//printf("p: "); mpz_out_str(stdout, 10, p); printf("\n");
    } while (!mpz_probab_prime_p(p, 10));
    printf("p: "); mpz_out_str(stdout, 10, p); printf("\n");
    printf("q: "); mpz_out_str(stdout, 10, q); printf("\n");
    mpz_sub_ui(p1onq, p, 1);
    mpz_divexact(p1onq, p1onq, q);
    mpz_set_ui(moq, 28);

    mpz_init(nq);
    for(;;) {
	mpz_urandomm(nq, rand_state, p);
	if (mpz_legendre(nq, p) == -1) break;
    }
    //printf("nq = "); mpz_out_str(stdout, 10, nq); printf("\n");

    mpz_sub_ui(j, p, 3375);
    mpz_sub_ui(c, j, 1728);
    mpz_invert(c, c, p);
    mpz_mul(c, c, j);
    mpz_mod(c, c, p);

    mpz_mul_si(curve_a, c, -3);
    mpz_mod(curve_a, curve_a, p);
    mpz_mul_ui(curve_b, c, 2);
    mpz_mod(curve_b, curve_b, p);

    //mpz_set_ui(p, 59);
    //mpz_set_ui(curve_a, 0);
    //mpz_set_ui(curve_b, 1);

    printf("Y^2 = X^3 + ");
    mpz_out_str(stdout, 10, curve_a);
    printf("X + ");
    mpz_out_str(stdout, 10, curve_b);
    printf("\n");

    for(;;) {
	mpz_t z;

	mpz_init(z);

	point_random(x2, y2);
	make_order_q(x2, y2);

	mpz_sub_ui(z, q, 1);
	point_mul(x, y, z, x2, y2);
//printf("(x, y) = ("); mpz_out_str(stdout, 10, x); printf(", ");
//mpz_out_str(stdout, 10, y); printf(")\n");
//printf("(x, y) = ("); mpz_out_str(stdout, 10, x2); printf(", ");
//mpz_out_str(stdout, 10, y2); printf(")\n");
	if (mpz_cmp(x, x2)) {
	    //should be order p + 3 instead, unless my conjecture's wrong
	    mpz_add_ui(z, p, 2);
	    point_mul(x, y, z, x2, y2);
	    if (mpz_cmp(x, x2)) {
		printf("COUNTEREXAMPLE!\n");
		exit(0);
	    }
	    printf("wrong curve. trying quadratic twist...\n");
	    mpz_mul(curve_a, curve_a, nq);
	    mpz_mul(curve_a, curve_a, nq);
	    mpz_mul(curve_b, curve_b, nq);
	    mpz_mul(curve_b, curve_b, nq);
	    mpz_mul(curve_b, curve_b, nq);
	} else break;

 	mpz_clear(z);
    }

    printf("E[q] lies in F_p\n");

    mpz_clear(j); mpz_clear(c);
    mpz_clear(x); mpz_clear(y);
    mpz_clear(x2); mpz_clear(y2);
    mpz_clear(n);
}

static void get_line(mpz_t v, mpz_t vd, mpz_t Qx, mpz_t Qy,
	mpz_t Ax, mpz_t Ay, mpz_t Bx, mpz_t By)
{
    mpz_t a, b, c;
    mpz_t t0, t1;

    mpz_init(a); mpz_init(b); mpz_init(c);
    mpz_init(t0); mpz_init(t1);

    //it should be
    //a = -(Q.y - P.y) / (Q.x - P.x);
    //b = 1;
    //c = -(P.y + a * P.x);
    //but we'll multiply by Q.x - P.x to avoid division
    mpz_sub(b, Bx, Ax);
    mpz_mod(b, b, p);
    mpz_sub(a, Ay, By);
    mpz_mod(a, a, p);
    mpz_mul(t0, b, Ay);
    mpz_mul(c, a, Ax);
    mpz_add(c, c, t0);
    mpz_neg(c, c);
    mpz_mod(c, c, p);

    mpz_mul(t0, a, Qx);
    mpz_mul(t1, b, Qy);
    mpz_add(t0, t0, t1);
    mpz_add(t0, t0, c);
    mpz_mod(t0, t0, p);
    mpz_mul(v, v, t0);
    mpz_mod(v, v, p);
    mpz_mul(vd, vd, b);
    mpz_mod(vd, vd, p);

    mpz_clear(a); mpz_clear(b); mpz_clear(c);
    mpz_clear(t0); mpz_clear(t1);
}

static void get_tangent(mpz_t v, mpz_t vd, mpz_t Qx, mpz_t Qy,
	mpz_t Ax, mpz_t Ay)
{
    mpz_t a, b, c;
    mpz_t t0, t1;

    mpz_init(a); mpz_init(b); mpz_init(c);
    mpz_init(t0); mpz_init(t1);
    //it should be
    //a = -slope_tangent(P.x, P.y);
    //b = 1;
    //c = -(P.y + a * P.x);
    //but we multiply by 2*P.y to avoid division

    //a = -Px * (Px + Px + Px + *twicea2) - *a4;
    //assume a2 = 0 (a4 = curve_a)
    mpz_add(a, Ax, Ax);
    mpz_add(a, a, Ax);
    mpz_mul(a, Ax, Ax);
    mpz_neg(a, a);
    mpz_sub(a, a, curve_a);
    mpz_mod(a, a, p);

    mpz_add(b, Ay, Ay);
    mpz_mod(b, b, p);

    mpz_mul(t0, b, Ay);
    mpz_mul(c, a, Ax);
    mpz_add(c, c, t0);
    mpz_neg(c, c);
    mpz_mod(c, c, p);

    mpz_mul(t0, a, Qx);
    mpz_mul(t1, b, Qy);
    mpz_add(t0, t0, t1);
    mpz_add(t0, t0, c);
    mpz_mul(v, v, t0);
    mpz_mod(v, v, p);
    mpz_mul(vd, vd, b);
    mpz_mod(vd, vd, p);

    mpz_clear(a); mpz_clear(b); mpz_clear(c);
    mpz_clear(t0); mpz_clear(t1);
}

static void get_vertical(mpz_t v, mpz_t vd, mpz_t Qx, mpz_t Ax)
{
    mpz_t t0;

    mpz_init(t0);

    mpz_sub(t0, Qx, Ax);
    mpz_mul(v, v, t0);
    mpz_mod(v, v, p);

    mpz_clear(t0);
}

void miller(mpz_ptr res, mpz_t Px, mpz_t Py, mpz_t Qx, mpz_t Qy)
//compute res = f_P(Q)
//assumes q is odd
{
    //f_1 = 1
    int m;

    mpz_t v, vd;
    mpz_t Zx, Zy;
    mpz_init(v); mpz_init(vd);
    mpz_init(Zx); mpz_init(Zy);

    mpz_set_ui(v, 1);
    mpz_set_ui(vd, 1);
    m = mpz_sizeinbase(q, 2) - 1;

    mpz_set(Zx, Px);
    mpz_set(Zy, Py);
    m--;
    get_tangent(v, vd, Qx, Qy, Zx, Zy);
    point_double(Zx, Zy, Zx, Zy);
    get_vertical(vd, v, Qx, Zx);
    for(;;) {
	//printf("v: "); mpz_out_str(stdout, 10, v); printf("\n");
	//printf("vd: "); mpz_out_str(stdout, 10, vd); printf("\n");
	if (mpz_tstbit(q, m)) {
	    if (!m) {
		get_vertical(v, vd, Qx, Zx);
		break;
	    }
	    get_line(v, vd, Qx, Qy, Zx, Zy, Px, Py);
	    point_add(Zx, Zy, Zx, Zy, Px, Py);
	    get_vertical(vd, v, Qx, Zx);
	}
	mpz_mul(v, v, v);
	mpz_mod(v, v, p);
	mpz_mul(vd, vd, vd);
	mpz_mod(vd, vd, p);
	get_tangent(v, vd, Qx, Qy, Zx, Zy);
	point_double(Zx, Zy, Zx, Zy);
	get_vertical(vd, v, Qx, Zx);
	m--;
    }

    //printf("v: "); mpz_out_str(stdout, 10, v); printf("\n");
    //printf("vd: "); mpz_out_str(stdout, 10, vd); printf("\n");

    mpz_invert(vd, vd, p);
    mpz_mul(res, v, vd);
    mpz_mod(res, res, p);

    mpz_clear(v); mpz_clear(vd);
    mpz_clear(Zx); mpz_clear(Zy);
}

void pairing(mpz_t r, mpz_t Px, mpz_t Py, mpz_t Qx, mpz_t Qy)
{
    miller(r, Px, Py, Qx, Qy);
    mpz_powm(r, r, p1onq, p);
}

void test_pairing()
{
    mpz_t Px, Py, Qx, Qy, Rx, Ry;
    mpz_t r0, r1, r2;

    mpz_init(Px); mpz_init(Py);
    mpz_init(Qx); mpz_init(Qy);
    mpz_init(Rx); mpz_init(Ry);
    mpz_init(r0); mpz_init(r1); mpz_init(r2);

    point_random(Px, Py);
    make_order_q(Px, Py);
printf("P = ("); mpz_out_str(stdout, 10, Px); printf(", ");
mpz_out_str(stdout, 10, Py); printf(")\n");
    point_random(Qx, Qy);
    make_order_q(Qx, Qy);
printf("Q = ("); mpz_out_str(stdout, 10, Qx); printf(", ");
mpz_out_str(stdout, 10, Qy); printf(")\n");
    point_random(Rx, Ry);
    make_order_q(Rx, Ry);
printf("R = ("); mpz_out_str(stdout, 10, Rx); printf(", ");
mpz_out_str(stdout, 10, Ry); printf(")\n");
    pairing(r0, Px, Py, Rx, Ry);
printf("e(P, R) = "); mpz_out_str(stdout, 10, r0); printf("\n");
    pairing(r1, Qx, Qy, Rx, Ry);
printf("e(Q, R) = "); mpz_out_str(stdout, 10, r1); printf("\n");
    point_add(Px, Py, Px, Py, Qx, Qy);
    pairing(r2, Px, Py, Rx, Ry);
printf("e(P + Q, R) = "); mpz_out_str(stdout, 10, r2); printf("\n");
    mpz_mul(r0, r0, r1);
    mpz_mod(r0, r0, p);
printf("e(P, R)e(Q, R) = "); mpz_out_str(stdout, 10, r0); printf("\n");

    mpz_clear(Px); mpz_clear(Py);
    mpz_clear(Qx); mpz_clear(Qy);
    mpz_clear(Rx); mpz_clear(Ry);
    mpz_clear(r0); mpz_clear(r1); mpz_clear(r2);
}

int main()
{
    int i;

    gmp_randinit_default(rand_state);
    mpz_init(p); mpz_init(q);
    mpz_init(p1onq);
    mpz_init(moq);
    mpz_init(curve_a); mpz_init(curve_b);

    for (i=0; i<100; i++)
    {
	find_curve();

	test_pairing();
    }

    mpz_clear(p); mpz_clear(q);
    mpz_clear(p1onq);
    mpz_clear(curve_a); mpz_clear(curve_b);
    mpz_clear(nq);
    mpz_clear(moq);

    return(0);
}
