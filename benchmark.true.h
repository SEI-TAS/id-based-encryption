/* benchmark.h, "true" version
 * Ben Lynn
 */

/*
Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)

See LICENSE for license
*/
#ifndef BENCHMARK_H
#define BENCHMARK_H

#define BENCHMARK

#ifdef __cplusplus
extern "C" {
#endif

void bm_put(double t, char *s);
double bm_get(char *s);
double bm_get_time();

void bm_report_encrypt();
void bm_report_decrypt();

#ifdef __cplusplus
}
#endif

#endif
