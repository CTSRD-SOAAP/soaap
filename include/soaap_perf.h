#ifndef _SOAAP_PERF_H_
#define _SOAAP_PERF_H_

#define DATA_IN "DATA_IN"
#define DATA_OUT "DATA_OUT"

#define __data_in __attribute__((annotate(DATA_IN)))
#define __data_out  __attribute__((annotate(DATA_OUT)))
#define __sandbox_overhead(A) __attribute((annotate("perf_overhead_(" #A ")")))

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <unistd.h>


/* DPRINTF */
#ifdef DEBUG
#define DPRINTF(format, ...)				\
		fprintf(stderr, "%s [%d] " format "\n",		\
				__FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define DPRINTF(...)
#endif

#define PRINTF(format, ...)				\
		fprintf(stderr, "%s [%d] " format "\n",		\
				__FUNCTION__, __LINE__, ##__VA_ARGS__)

#define UDSOCKETS

#define MAGIC 0xd3ad
#define OP_SENDBACK 0x01

struct ctrl_msg {
    uint16_t magic;
    u_char op;
    uint32_t reqsize;
} __packed;


pid_t soaap_pid;
int pfds[2]; /* Paired descriptors used for both sockets and pipes */
char soaap_buf[PAGE_SIZE];
char soaap_tmpbuf[PAGE_SIZE];

__attribute__((used)) void
soaap_perf_create_persistent_sbox()
{
	int nbytes;
	struct ctrl_msg *cm;
	int buflen = PAGE_SIZE;

	DPRINTF("Creating a persistent sandbox.");

#ifdef PIPES
	/* Use pipes for IPC */
	pipe(pfds);
#elif defined (UDSOCKETS)
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pfds) == -1) {
		perror("socketpair");
		exit(1);
	}
#endif

	soaap_pid = fork();
	if (soaap_pid == -1) {
		perror("fork");
		return;
	}

	if (!soaap_pid) {

		DPRINTF(" SANDBOX: reading from the pipe");
        close(pfds[1]);

		/* nbytes will be zero on EOF */
		while( (nbytes = read(pfds[0], soaap_buf, buflen)) > 0) {
			DPRINTF(" SANDBOX: read %d bytes", nbytes);
			if (nbytes == sizeof(struct ctrl_msg)) {
				cm = (struct ctrl_msg *) soaap_buf;
				if (cm->magic & ~(MAGIC))
					continue;
				else {
					write(pfds[0], soaap_tmpbuf, cm->reqsize);
					DPRINTF("SANDBOX: sent back %d bytes", cm->reqsize);
				}
			}
		}

		DPRINTF(" SANDBOX: exiting");
		exit(0);
	}

    close(pfds[0]);
}

__attribute__((used)) void
soaap_perf_enter_persistent_sbox()
{

	DPRINTF("Emulating performance of creating persistent sandbox.");

}

__attribute__((used)) void
soaap_perf_enter_ephemeral_sbox()
{

	DPRINTF("Emulating performance of creating ephemeral sandbox.");
}

__attribute__((used)) void
soaap_perf_enter_datain_persistent_sbox(int datalen_in)
{

	int nbytes = 0;
	int buflen = PAGE_SIZE;

	DPRINTF("Emulating performance of using persistent sandbox.");

	DPRINTF("DATALEN OUT: %d", datalen_in);

	/*
	 * Write datalen_in bytes over the pipe in chunks of buflen (PAGESIZE).
	 * We are exiting if this function is called with a negative integer as
	 * argument.
	 */
	if (datalen_in >= 0) {
		while(datalen_in > buflen) {
			nbytes = write(pfds[1], soaap_buf, buflen);
			DPRINTF("PARENT: written to the pipe %d bytes", nbytes);
			datalen_in -= nbytes;
		}
		while (datalen_in > 0) {
			nbytes = write(pfds[1], soaap_buf, datalen_in);
			if (nbytes < 0) {
				perror("PARENT write()");
				return;
			}
			DPRINTF("PARENT: written to the pipe %d bytes", nbytes);
			datalen_in -= nbytes;
		}
		return;
	}

	/* Send EOF to the sandbox */
	//close(pfds[0]);
	close(pfds[1]);

	/* Cleanup & wait for sandbox to terminate */
	wait(NULL);
	DPRINTF("PARENT: exiting");
}

__attribute__((used)) void
soaap_perf_enter_dataout_persistent_sbox(int datalen_out)
{

	int nbytes = 0, left;
	int buflen = PAGE_SIZE;
	struct ctrl_msg cm;

	DPRINTF("Emulating performance of using persistent sandbox.");

	DPRINTF("DATALEN IN: %d", datalen_out);

	/* Initialize cm */
	cm.magic = MAGIC;
	cm.op = OP_SENDBACK;

	/*
	 * Request the sandbox to send back datalen_out bytes.
	 */
	if (datalen_out > 0) {
		left = sizeof(cm);

		cm.reqsize = datalen_out;
		while (left > 0) {
			nbytes = write(pfds[1], &cm, left);
			if (nbytes != sizeof(cm)) {
				DPRINTF("[XXX] Failed to send ctrl msg!");
				return;
			}
			left -= nbytes;
		}
		DPRINTF("PARENT: requested sandbox to send %d bytes", datalen_out);

		while( left > 0 ) {
			nbytes = read(pfds[1], soaap_tmpbuf, left);
			DPRINTF("PARENT: read from fd %d bytes", nbytes);
			left -= nbytes;
		}
		return;
	}
}

__attribute__((used)) void
soaap_perf_enter_datain_ephemeral_sbox(int datalen_in)
{

	int epfds[2], nbytes;
	pid_t pid;
	int buflen = PAGE_SIZE;

	DPRINTF("Emulating performance of using ephemeral sandbox.");

#ifdef PIPES
	/* Use pipes for IPC */
	pipe(epfds);
#elif defined (UDSOCKETS)
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, epfds) == -1) {
		perror("socketpair");
		exit(1);
	}
#endif

	DPRINTF("Creating new ephemeral sandbox.");
	pid = fork();
	if (pid == -1) {
		perror("fork");
		return;
	}

	if (!pid) {
		close(epfds[1]);
		while ((nbytes = read(epfds[0], soaap_buf, buflen)) > 0) {
			;
			DPRINTF(" SANDBOX: read %d bytes", nbytes);
		}
		DPRINTF(" SANDBOX: exiting");
		exit(0);
	} else {
		close(pfds[0]);
		/*
		 * Write datalen_in bytes over the pipe in chunks of buflen (PAGESIZE).
		 * We are exiting if this function is called with a negative integer as
		 * argument.
		 */
		if (datalen_in >= 0) {
			while(datalen_in > buflen) {
				nbytes = write(epfds[1], soaap_buf, buflen);
				DPRINTF("PARENT: written to the pipe %d bytes", nbytes);
				datalen_in -= nbytes;
			}
			while (datalen_in > 0) {
				nbytes = write(epfds[1], soaap_buf, datalen_in);
				if (nbytes < 0) {
					perror("PARENT write()");
					return;
				}
				DPRINTF("PARENT: written to the pipe %d bytes", nbytes);
				datalen_in -= nbytes;
			}
		}

		/* Send EOF to sandbox, cleanup and wait */
		close(epfds[1]);
		wait(NULL);
	}
}

__attribute__((used)) void
soaap_perf_tic(struct timespec *start_ts)
{

#ifdef PROF
	DPRINTF("SANDBOXED FUNCTION PROLOGUE -- TIC!");
	clock_gettime(CLOCK_MONOTONIC, start_ts);
#else
    ;
#endif
}

__attribute__((used)) void
soaap_perf_overhead_toc(struct timespec *sbox_ts)
{
#ifdef PROF
	DPRINTF("SANDBOXED FUNCTION OVERHEAD -- SBOX_TOC!");
	clock_gettime(CLOCK_MONOTONIC, sbox_ts);
#else
    ;
#endif
}

__attribute__((used)) void
soaap_perf_total_toc(struct timespec *start_ts, struct timespec *sbox_ts)
{
#ifdef PROF
	struct timespec end_ts, diff_ts;
	unsigned long ns_total, ns_sbox;

	ns_total = ns_sbox = 0;

	/* Get the final timestamp */
	clock_gettime(CLOCK_MONOTONIC, &end_ts);

	/* Calculate total execution time of sandboxed function */
	diff_ts.tv_sec = end_ts.tv_sec - start_ts->tv_sec;
	diff_ts.tv_nsec = end_ts.tv_nsec - start_ts->tv_nsec;

	ns_total = diff_ts.tv_nsec;
	while(diff_ts.tv_sec) {
		diff_ts.tv_sec--;
		ns_total += 1000000000;
	}

	/* Calculate the time spent in sandboxing mechanisms */
	diff_ts.tv_sec = sbox_ts->tv_sec - start_ts->tv_sec;
	diff_ts.tv_nsec = sbox_ts->tv_nsec - start_ts->tv_nsec;

	ns_sbox = diff_ts.tv_nsec;
	while(diff_ts.tv_sec) {
		diff_ts.tv_sec--;
		ns_sbox += 1000000000;
	}

	DPRINTF("[Total Execution Time]: %lu ns, [Sandboxing Time]: %lu ns, "
		"[Sandboxing Overhead]: %f%%", ns_total, ns_sbox,
		((double)ns_sbox/(double)ns_total)*100);

#else
    ;
#endif
}

__attribute__((used)) void
soaap_perf_total_toc_thres(struct timespec *start_ts, struct timespec *sbox_ts,
	int thres)
{
#ifdef PROF
	struct timespec end_ts, diff_ts;
	unsigned long ns_total, ns_sbox;
	double overhead;

	ns_total = ns_sbox = 0;

	/* Get the final timestamp */
	clock_gettime(CLOCK_MONOTONIC, &end_ts);

	/* Calculate total execution time of sandboxed function */
	diff_ts.tv_sec = end_ts.tv_sec - start_ts->tv_sec;
	diff_ts.tv_nsec = end_ts.tv_nsec - start_ts->tv_nsec;

	ns_total = diff_ts.tv_nsec;
	while(diff_ts.tv_sec) {
		diff_ts.tv_sec--;
		ns_total += 1000000000;
	}

	/* Calculate the time spent in sandboxing mechanisms */
	diff_ts.tv_sec = sbox_ts->tv_sec - start_ts->tv_sec;
	diff_ts.tv_nsec = sbox_ts->tv_nsec - start_ts->tv_nsec;

	ns_sbox = diff_ts.tv_nsec;
	while(diff_ts.tv_sec) {
		diff_ts.tv_sec--;
		ns_sbox += 1000000000;
	}

	overhead = ((double)ns_sbox/(double)ns_total)*100;

	if (overhead > (double) thres)
		fprintf(stderr, "[!!!] Sandboxing Overhead %f%% (Threshold: %d%%)\n",
			overhead, thres);

	DPRINTF("[Total Execution Time]: %lu ns, [Sandboxing Time]: %lu ns, "
		"[Sandboxing Overhead]: %f%%", ns_total, ns_sbox, overhead);
#else
    ;
#endif

}

#endif	/* _SOAAP_PERF_H_ */
