#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "soaap_perf.h"

/* DPRINTF */
#define DEBUG
#ifdef DEBUG
#define DPRINTF(format, ...)				\
		fprintf(stderr, "%s [%d] " format "\n",		\
				__FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define DPRINTF(...)
#endif

void
soaap_perf_enter_persistent_sbox()
{

	DPRINTF("Emulating performance of creating persistent sandbox.");

}

void
soaap_perf_enter_ephemeral_sbox()
{

	DPRINTF("Emulating performance of creating ephemeral sandbox.");
}

void
soaap_perf_enter_datain_persistent_sbox(int datalen_in)
{

	int nbytes = 0;
	static char *buf;
	static pid_t pid;
	static int pfds[2], flag, buflen;

	DPRINTF("Emulating performance of using persistent sandbox.");

	if(!flag) {
		buflen = getpagesize();
		buf = malloc(buflen*sizeof(char));

		/* Use pipes for IPC */
		pipe(pfds);

		pid = fork();
		if (pid == -1) {
			perror("fork");
			return;
		}
		flag = 1;
	}

	if (!pid) {

		static int clflag;

		DPRINTF(" SANDBOX: reading from the pipe");
		if(!clflag) {
			close(pfds[1]);
			clflag = 1;
		}

		/* nbytes will be zero on EOF */
		while( (nbytes = read(pfds[0], buf, buflen)) > 0) {
			;
			DPRINTF(" SANDBOX: read %d bytes", nbytes);
		}

		DPRINTF(" SANDBOX: exiting");
		free(buf);
		exit(0);
	} else {
		DPRINTF("DATALEN: %d", datalen_in);

		/*
		 * Write datalen_in bytes over the pipe in chunks of buflen (PAGESIZE).
		 * We are exiting if this function is called with a negative integer as
		 * argument.
		 */
		if (datalen_in >= 0) {
			while(datalen_in > buflen) {
				nbytes = write(pfds[1], buf, buflen);
				DPRINTF("PARENT: written to the pipe %d bytes", nbytes);
				datalen_in -= nbytes;
			}
			while (datalen_in > 0) {
				nbytes = write(pfds[1], buf, datalen_in);
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
		close(pfds[0]);
		close(pfds[1]);

		/* Cleanup & wait for sandbox to terminate */
		free(buf);
		wait(NULL);
		DPRINTF("PARENT: exiting");
	}
}

void
soaap_perf_enter_datain_ephemeral_sbox(int datalen_in)
{

	int pfds[2], buflen, nbytes;
	char *buf;
	pid_t pid;

	DPRINTF("Emulating performance of using ephemeral sandbox.");

	/* Use pipes for IPC */
	pipe(pfds);

	/* Allocate resources */
	buflen = getpagesize();
	buf = malloc(buflen*sizeof(char));

	DPRINTF("Creating new ephemeral sandbox.");
	pid = fork();
	if (pid == -1) {
		perror("fork");
		return;
	}

	if (!pid) {
		int nbytes;
		close(pfds[1]);
		while ((nbytes = read(pfds[0], buf, buflen)) > 0) {
			;
			DPRINTF(" SANDBOX: read %d bytes", nbytes);
		}
		free(buf);
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
				nbytes = write(pfds[1], buf, buflen);
				DPRINTF("PARENT: written to the pipe %d bytes", nbytes);
				datalen_in -= nbytes;
			}
			while (datalen_in > 0) {
				nbytes = write(pfds[1], buf, datalen_in);
				if (nbytes < 0) {
					perror("PARENT write()");
					return;
				}
				DPRINTF("PARENT: written to the pipe %d bytes", nbytes);
				datalen_in -= nbytes;
			}
		}

		/* Send EOF to sandbox, cleanup and wait */
		close(pfds[1]);
		free(buf);
		wait(NULL);
	}
}
