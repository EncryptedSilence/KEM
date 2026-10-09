#ifndef PQC_TIMING_H
#define PQC_TIMING_H
#include <stdint.h>
/* All clocks are local and monotonic; never subtract across machines. */

uint64_t mono_ns(void);   /* CLOCK_MONOTONIC in nanoseconds              */
uint64_t cpu_ns(void);    /* CLOCK_PROCESS_CPUTIME_ID in nanoseconds     */
long peak_rss_kb(void);   /* getrusage(ru_maxrss) peak resident set (KB) */

#endif /* PQC_TIMING_H */
