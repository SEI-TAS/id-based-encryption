/* byte_string.h
 * Ben Lynn
 */

/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/
#ifndef BYTE_STRING_H
#define BYTE_STRING_H

#include <stdio.h>

struct byte_string_s {
    unsigned char *data;
    int len;
    int origlen; //for debugging
};

typedef struct byte_string_s byte_string_t[1];
typedef struct byte_string_s *byte_string_ptr;

void byte_string_init(byte_string_t bs, int n);
void byte_string_reinit(byte_string_t bs, int n);
void byte_string_assign(byte_string_t bs, byte_string_t src);
int byte_string_cmp(byte_string_t bs, byte_string_t bs2);
void byte_string_copy(byte_string_t bs, byte_string_t src);
void byte_string_clear(byte_string_t bs);

void byte_string_set(byte_string_t bs, const char *s);
#ifndef __KERNEL__
void byte_string_fprintf(FILE *fp, byte_string_t bs, char *format);
#endif
void byte_string_printf(byte_string_t bs, char *format);
void byte_string_join(byte_string_t bs, byte_string_t bs1, byte_string_t bs2);
void byte_string_split(byte_string_t bs1, byte_string_t bs2, byte_string_t bs);

void byte_string_encode_array(byte_string_t bs, byte_string_t *bsa, int n);
//encode a byte_string array as a single byte_string
void byte_string_decode_array(byte_string_t **bsarray, int *n, byte_string_t bs);
//decode an encoded byte_string array
//maps invalid representations to empty byte_string array

int int_from_byte_string(byte_string_t bs);
char* charstar_from_byte_string(byte_string_t bs);
void byte_string_set_int(byte_string_t bs, int n);

#endif //BYTE_STRING_H
