/* benchmark.h, "void" version
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

#define do_nothing ((void) (0))
#define bm_put(x, y) do_nothing
#define bm_get(x) do_nothing
#define bm_get_time() do_nothing

#define bm_report_encrypt() do_nothing
#define bm_report_decrypt() do_nothing

#ifdef __cplusplus
}
#endif

#endif
