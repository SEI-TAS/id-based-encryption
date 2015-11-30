/* wrappers around alloc so I can keep track of them
 * Ben Lynn
 */

/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/

#ifndef MM_H
#define MM_H

void mm_tally(char *s, int i, char *reason);
void mm_report();

#endif //MM_H
