
    See LICENSE for license

    See LICENSE for license
    byte_string_t bs1, bs2, bs;

    byte_string_set(bs1, "Hello");
    byte_string_set(bs2, "World");
    byte_string_join(bs, bs1, bs2);
    byte_string_clear(bs1);
    byte_string_clear(bs2);
    byte_string_fprintf(stdout, bs, " %02X");
    printf("\n");

    byte_string_split(bs1, bs2, bs);
    byte_string_fprintf(stdout, bs1, " %02X");
    printf("\n");
    byte_string_fprintf(stdout, bs2, " %02X");
    printf("\n");
    byte_string_clear(bs);
    byte_string_clear(bs1);
    byte_string_clear(bs2);
}

void test2()
{
    int i;

    byte_string_t bsa[4];
    byte_string_t bs;

    byte_string_set(bsa[0], "This");
    byte_string_set(bsa[1], "is");
    byte_string_set(bsa[2], "a");
    byte_string_set(bsa[3], "test");

    byte_string_encode_array(bs, bsa, 4);

    byte_string_fprintf(stdout, bs, " %02X");
    printf("\n");

    for (i=0; i<4; i++) {
	byte_string_clear(bsa[i]);
    }

    byte_string_clear(bs);
}

int main()
{
    test1();
    test2();

    return 0;
}
