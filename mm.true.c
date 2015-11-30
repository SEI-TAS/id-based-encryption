/* keep track of memory use
 * Ben Lynn
 */

/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/

#include <stdio.h>
#include <stdlib.h>
#include "mm.h"

struct mm_s {
    int count;
    char name[100];
};

static struct mm_s mmtable[1000];
static int mmcount = 0;

void mm_tally(char *s, int n, char *reason)
{
    int i;

    for (i=0; i<mmcount; i++) {
	if (!strcmp(mmtable[i].name, s)) break;
    }
    if (i == mmcount) {
	mmcount++;
	strcpy(mmtable[i].name, s);
    }

    mmtable[i].count += n;

    //fprintf(stderr, "mm: %s: %s: %d: %d\n", s, reason, n, mmtable[i].count);
}

void mm_report()
{
    int i;

    for (i=0; i<mmcount; i++) {
	fprintf(stderr, "%s: %d\n", mmtable[i].name, mmtable[i].count);
    }
}
