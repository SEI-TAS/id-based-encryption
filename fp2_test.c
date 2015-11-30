
See LICENSE for license

See LICENSE for license
static int m = 59;

void fp2_random(fp2_ptr r)
//r = random element of F_p^2
{
    mpz_set_ui(r->a, random() % m);
    mpz_set_ui(r->b, random() % m);
}

int main(int argc, char **argv)
{
    mpz_t p;
    fp2_t a, b, c;

    if (argc > 1) sscanf(argv[1], "%d", &m);

    mpz_init_set_ui(p, m);
    mpz_out_str(NULL, 0, p);
    printf(" = %d\n", m);

    fp2_init(a);
    fp2_init(b);
    fp2_init(c);

    fp2_random(a);
    fp2_random(b);

    printf("a b = ");
    fp2_out_str(NULL, 0, a);
    printf(" ");
    fp2_out_str(NULL, 0, b);
    printf("\n");

    fp2_add(c, a, b, p);
    fp2_sub(c, c, a, p);
    fp2_sub(c, c, b, p);

    printf("0 = ");
    fp2_out_str(NULL, 0, c);
    printf("\n");

    fp2_inv(c, a, p);
    fp2_inv(c, c, p);
    printf("a = ");
    fp2_out_str(NULL, 0, c);
    printf("\n");

    fp2_inv(c, a, p);
    fp2_mul(c, c, a, p);
    printf("1 = ");
    fp2_out_str(NULL, 0, c);
    printf("\n");

    fp2_pow(c, c, p, p);
    printf("1 = ");
    fp2_out_str(NULL, 0, c);
    printf("\n");

    fp2_pow(c, a, p, p);
    fp2_pow(c, c, p, p);
    printf("a = ");
    fp2_out_str(NULL, 0, c);
    printf("\n");

    if (fp2_set_str(c, "[12 34]", 10)) {
	printf("fp2_set_str error!\n");
    }
    printf("c = ");
    fp2_out_str(NULL, 0, c);
    printf("\n");
    fp2_mul(c, c, c, p);
    printf("c * c = ");
    fp2_out_str(NULL, 0, c);
    printf("\n");

    fp2_clear(a);
    fp2_clear(b);
    fp2_clear(c);
    mpz_clear(p);

    return 0;
}
