#ifndef _SOAAP_PERF_H_
#define _SOAAP_PERF_H_

#define DATA_IN "DATA_IN"
#define DATA_OUT "DATA_OUT"

#define __data_in __attribute__((annotate(DATA_IN)))
#define __data_out  __attribute__((annotate(DATA_OUT)))
#define __sandbox_overhead(A) __attribute((annotate("perf_overhead_(" #A ")")))

void soaap_perf_enter_persistent_sbox(void);
void soaap_perf_enter_ephemeral_sbox(void);

void soaap_perf_enter_datain_persistent_sbox(int );
void soaap_perf_enter_datain_ephemeral_sbox(int );

#include <time.h>
#include <sys/timespec.h>
void soaap_perf_tic(struct timespec *);
void soaap_perf_overhead_toc(struct timespec *);
void soaap_perf_total_toc(struct timespec *, struct timespec *);

#endif	/* _SOAAP_PERF_H_ */
