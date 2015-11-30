/* byte_string routines
 * Ben Lynn
 */

/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/

#include <stdlib.h>
#include <string.h>
#include "byte_string.h"
#include "mm.h"

void byte_string_init(byte_string_t bs, int n)
{
    bs->data = (unsigned char *) malloc(n * sizeof(unsigned char));
    bs->len = n;

    mm_tally("bs", bs->origlen = n, "init");
}

void byte_string_reinit(byte_string_t bs, int n)
{
    bs->data = (unsigned char *) realloc(bs->data, n * sizeof(unsigned char));
    bs->len = n;

    mm_tally("bs", n - bs->origlen, "reinit");
    bs->origlen = n;
}

void byte_string_fprintf(FILE *fp, byte_string_t bs, char *format)
{
    int i;
    for(i=0; i<bs->len; i++) {
	fprintf(fp, format, bs->data[i]);
    }
}

void byte_string_printf(byte_string_t bs, char *format)
{
    int i;
    for(i=0; i<bs->len; i++) {
	printf(format, bs->data[i]);
    }
}

void byte_string_set(byte_string_t bs, const char *s)
{
    byte_string_init(bs, strlen(s));
    memcpy(bs->data, s, bs->len);
}

void byte_string_assign(byte_string_t bs, byte_string_t src)
{
    bs->data = src->data;
    bs->len = src->len;
    bs->origlen = src->len;
}

void byte_string_copy(byte_string_t bs, byte_string_t src)
{
    byte_string_init(bs, src->len);
    memcpy(bs->data, src->data, bs->len);
}

int byte_string_cmp(byte_string_t bs, byte_string_t bs2)
{
    int i;
    int result;

    i = bs->len < bs2->len ? bs->len : bs2->len;

    result = memcmp(bs->data, bs2->data, i);
    if (!result) {
	if (bs->len == bs2->len) return 0;
	else return bs->len > bs2->len;
    } else return result;
}

void byte_string_clear(byte_string_t bs)
{
    if (bs->len) {
	mm_tally("bs", -bs->len, "free");
	free(bs->data);
	bs->len = 0;
    } else {
	printf("BUG! double byte_string_clear()\n");
    }
}

void byte_string_set_int(byte_string_t bs, int n)
{
    byte_string_init(bs, 4);
    bs->len = 4;
    bs->data[0] = (unsigned char) n >> 24;
    bs->data[1] = (unsigned char) n >> 16;
    bs->data[2] = (unsigned char) n >> 8;
    bs->data[3] = (unsigned char) n;
}

char* charstar_from_byte_string(byte_string_t bs)
{
    char *result;

    result = malloc(bs->len + 1);
    memcpy(result, bs->data, bs->len);
    result[bs->len] = 0;
    return result;
}

int int_from_byte_string(byte_string_t bs)
{
    int result;

    if (bs->len != 4) return 0;

    result = bs->data[3] + (bs->data[2] << 8)
	+ (bs->data[1] << 16) + (bs->data[0] << 24);

    return result;
}

void byte_string_encode_array(byte_string_t bs, byte_string_t *bsa, int n)
{
    int i;
    int offset;

    bs->len = 2 + 2 * n;

    for (i=0; i<n; i++) {
	bs->len += bsa[i]->len;
    }
    bs->data = (unsigned char *) malloc(bs->len * sizeof(unsigned char));
    mm_tally("bs", bs->origlen = bs->len, "encode");

    bs->data[0] = (unsigned char) (n >> 8);
    bs->data[1] = (unsigned char) n;
    offset = 2;
    for (i=0; i<n; i++) {
	bs->data[offset++] = (unsigned char) (bsa[i]->len >> 8);
	bs->data[offset++] = (unsigned char) bsa[i]->len;
    }
    for (i=0; i<n; i++) {
	memcpy(&bs->data[offset], bsa[i]->data, bsa[i]->len);
	offset += bsa[i]->len;
    }

    /* alternative representation: harder to check
    bs->data[0] = (unsigned char) (n >> 8);
    bs->data[1] = (unsigned char) n;
    offset = 2;
    for (i=0; i<n; i++) {
	bs->data[offset] = (unsigned char) (bsa[i]->len >> 8);
	bs->data[offset + 1] = (unsigned char) bsa[i]->len;
	memcpy(&bs->data[offset + 2], bsa[i]->data, bsa[i]->len);
	offset += bsa[i]->len + 2;
    }
    */
}

void byte_string_decode_array(byte_string_t **bsarray, int *n, byte_string_t bs)
{
    int i;
    int offset;
    int total;
    byte_string_t *bsa;

    if (bs->len < 2) {
	*n = 0;
	*bsarray = NULL;
	return;
    }

    *n = (bs->data[0] << 8) + bs->data[1];


    if (bs->len < 2 + 2 * *n) {
	*n = 0;
	*bsarray = NULL;
	return;
    }

    bsa = (byte_string_t *) malloc(*n * sizeof(byte_string_t));

    offset = 2;
    total = 0;

    for (i=0; i<*n; i++) {
	bsa[i]->len = (bs->data[offset] << 8) + bs->data[offset + 1];
	offset += 2;
	total += bsa[i]->len;
    }

    if (bs->len != total + offset) {
	*n = 0;
	*bsarray = NULL;
	free(bsa);
	return;
    }

    for (i=0; i<*n; i++) {
	bsa[i]->data = (unsigned char *) malloc(bsa[i]->len * sizeof(unsigned char));
	mm_tally("bs", bsa[i]->origlen = bsa[i]->len, "decode");
	memcpy(bsa[i]->data, &bs->data[offset], bsa[i]->len);
	offset += bsa[i]->len;
    }

    /* alternative serialization scheme
    for (i=0; i<*n; i++) {
	bsa[i]->len = (bs->data[offset] << 8) + bs->data[offset + 1];
	bsa[i]->data = (unsigned char *) malloc(bsa[i]->len * sizeof(unsigned char));
	memcpy(bsa[i]->data, &bs->data[offset + 2], bsa[i]->len);
	offset += bsa[i]->len + 2;
    }
    */
    *bsarray = bsa;
}

//for convenience: same version of above,
//but when there's only 2 byte_strings
void byte_string_join(byte_string_t bs, byte_string_t bs1, byte_string_t bs2)
{
    byte_string_t bsa[2];

    byte_string_assign(bsa[0], bs1);
    byte_string_assign(bsa[1], bs2);

    byte_string_encode_array(bs, bsa, 2);
}

void byte_string_split(byte_string_t bs1, byte_string_t bs2, byte_string_t bs)
{
    byte_string_t *bsa;
    int n;

    byte_string_decode_array(&bsa, &n, bs);

    if (n == 2) {
	byte_string_assign(bs1, bsa[0]);
	byte_string_assign(bs2, bsa[1]);
    } else {
	int i;

	bs1->len = 0;
	bs2->len = 0;

	for(i=0; i<n; i++) {
	    byte_string_clear(bsa[i]);
	}
    }

    free(bsa);
}
