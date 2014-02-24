#ifndef _SOAAP_PERF_H_
#define _SOAAP_PERF_H_

#define DATA_IN "DATA_IN"
#define DATA_OUT "DATA_OUT"

#ifndef IN_SOAAP_INSTRUMENTER


#define __soaap_data_in __attribute__((annotate(DATA_IN)))
#define __soaap_data_out  __attribute__((annotate(DATA_OUT)))
#define __soaap_overhead(A) __attribute((annotate("perf_overhead_(" #A ")")))

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

#define DEBUG
//#define PROF

/* DPRINTF */
#ifdef DEBUG
#define DPRINTF(format, ...)        \
    fprintf(stderr, "%s [%d] " format "\n",   \
        __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define DPRINTF(...)
#endif

#define PRINTF(format, ...)       \
    fprintf(stderr, "%s [%d] " format "\n",   \
        __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define UDSOCKETS

#define MAGIC 0xcafebabe
#define OP_SENDBACK 0x0001
#define OP_SENDRECEIVE 0x0010

#ifndef PAGE_SIZE
#define PAGE_SIZE _SC_PAGE_SIZE
#endif

#define SOAAP_BUF_LEN PAGE_SIZE

struct ctrl_msg {
    uint32_t magic;
    uint16_t op;
    uint32_t sbox_datain_len;
    uint32_t sbox_dataout_len;
} __packed;


pid_t soaap_pid;
int pfds[2]; /* Paired descriptors used for both sockets and pipes */
char soaap_buf[SOAAP_BUF_LEN];
char soaap_tmpbuf[SOAAP_BUF_LEN];

__attribute__((used)) static void
soaap_perf_create_persistent_sbox(void)
{
  int nbytes, nbytes_left;
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
      /* XXX IM: ctrl msg might not be read at once -- FIX this */
      if (nbytes >= (int) sizeof(struct ctrl_msg)) {
        cm = (struct ctrl_msg *) soaap_buf;
        if (cm->magic & ~(MAGIC))
          continue;

        switch (cm->op) {
        case OP_SENDRECEIVE:
          nbytes_left = cm->sbox_datain_len + sizeof(struct ctrl_msg) - nbytes;
          DPRINTF("SANDBOX: waiting to receive %d", nbytes_left);

          /* Chew all incoming data */
          while (nbytes_left) {
            if ((nbytes = read(pfds[0], soaap_buf, SOAAP_BUF_LEN)) > 0)
              nbytes_left -= nbytes;
          }
        case OP_SENDBACK:
          /* Fallback */
          /* Send back data */
          DPRINTF("SANDBOX: sending back %d bytes", cm->sbox_dataout_len);
          while (cm->sbox_dataout_len > SOAAP_BUF_LEN) {
            nbytes = write(pfds[0], soaap_tmpbuf, SOAAP_BUF_LEN);
            cm->sbox_dataout_len -= nbytes;
          }
          while (cm->sbox_dataout_len > 0) {
            nbytes = write(pfds[0], soaap_tmpbuf, cm->sbox_dataout_len);
            cm->sbox_dataout_len -= nbytes;
          }
          break;
        default:
          DPRINTF("Unknown operation");
        }
      }
    }

    DPRINTF(" SANDBOX: exiting");
    exit(0);
  }

  close(pfds[0]);
}

__attribute__((used)) static void
soaap_perf_enter_persistent_sbox()
{

  DPRINTF("Emulating performance of entering persistent sandbox.");
  DPRINTF("Sending request over RPC.");
  write(pfds[1], soaap_buf, 1);
  //DPRINTF("PARENT: written to the pipe %d bytes", nbytes);
  
}

__attribute__((used)) static void
soaap_perf_enter_ephemeral_sbox()
{

  DPRINTF("Emulating performance of entering ephemeral sandbox.");
}

__attribute__((used)) static void
soaap_perf_enter_datain_persistent_sbox(int datalen_in)
{

  int nbytes = 0;

  DPRINTF("Emulating performance of using persistent sandbox.");

  DPRINTF("DATALEN OUT: %d", datalen_in);

  /*
   * Write datalen_in bytes over the pipe in chunks of buflen (PAGESIZE).
   * We are exiting if this function is called with a negative integer as
   * argument.
   */
  if (datalen_in >= 0) {
    while(datalen_in > SOAAP_BUF_LEN) {
      nbytes = write(pfds[1], soaap_buf, SOAAP_BUF_LEN);
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

__attribute__((used)) static void
soaap_perf_enter_dataout_persistent_sbox(int datalen_out)
{

  int nbytes = 0, nbytes_left;
  struct ctrl_msg cm;

  DPRINTF("Emulating performance of using persistent sandbox.");

  if (datalen_out <= 0) {
    DPRINTF("Zero or negative request for data");
    return;
  }

  DPRINTF("DATALEN IN: %d", datalen_out);

  /* Initialize cm */
  cm.magic = MAGIC;
  cm.op = OP_SENDBACK;
  cm.sbox_datain_len = 0; // Sandbox doesn't need to wait for more data
  cm.sbox_dataout_len = datalen_out;

  /*
   * Request the sandbox to send back datalen_out bytes.
   */
  nbytes_left = sizeof(cm);
  while (nbytes_left > 0) {
    nbytes = write(pfds[1], &cm, nbytes_left);
    nbytes_left -= nbytes;
  }
  DPRINTF("PARENT: requested sandbox to send %d bytes", datalen_out);

  nbytes_left = cm.sbox_dataout_len;
  while( nbytes_left > 0 ) {
    nbytes = read(pfds[1], soaap_tmpbuf, SOAAP_BUF_LEN);
    DPRINTF("PARENT: read from fd %d bytes", nbytes);
    nbytes_left -= nbytes;
  }
}

__attribute__((used)) static void
soaap_perf_enter_datainout_persistent_sbox(int datalen_in, int datalen_out)
{

  int nbytes = 0, nbytes_left, tmpnbytes;
  uint32_t *magicptr;
  struct ctrl_msg cm;

  DPRINTF("Emulating performance of using persistent sandbox.");

  DPRINTF("DATALEN IN: %d", datalen_out);

  if (!datalen_in && !datalen_out)
    return;

  /* Initialize cm */
  cm.magic = MAGIC;
  cm.op = OP_SENDRECEIVE;
  cm.sbox_datain_len = datalen_in; // Sandbox doesn't need to wait for more data
  cm.sbox_dataout_len = datalen_out; // Sandbox doesn't need to wait for more data

  /* Initialize ctrl message and required data */
  nbytes_left = sizeof(cm) + datalen_in;
  memmove(soaap_buf, &cm, sizeof(cm));
  magicptr = (uint32_t *) soaap_buf;

  /*
   * Send and receive data from/to the sandbox
   */
  while(nbytes_left > SOAAP_BUF_LEN) {
    nbytes = write(pfds[1], soaap_buf, SOAAP_BUF_LEN);

    /* Ensure that the ctrl message is sent */
    while (nbytes <  (int) sizeof(cm)) {
      tmpnbytes = write(pfds[1], &soaap_buf, sizeof(cm) - nbytes);
      nbytes += tmpnbytes;
    }

    /* Unset the magic */
    *magicptr &= ~(MAGIC);

    nbytes_left -= nbytes;
  }

  while (nbytes_left > 0) {
    nbytes = write(pfds[1], soaap_buf, nbytes_left);
    nbytes_left -= nbytes;
  }
  DPRINTF("PARENT: sent to sandbox %d bytes and "
    "requested back %d bytes", datalen_in, datalen_out);

  /* Chew the data that sent from the sandbox */
  nbytes_left = cm.sbox_dataout_len;
  while( nbytes_left > 0 ) {
    nbytes = read(pfds[1], soaap_tmpbuf, SOAAP_BUF_LEN);
    DPRINTF("PARENT: read from fd %d bytes", nbytes);
    nbytes_left -= nbytes;
  }
}

__attribute__((used)) static void
soaap_perf_enter_datain_ephemeral_sbox(int datalen_in)
{
  int nbytes;
  int epfds[2];
  pid_t pid;

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
    while ((nbytes = read(epfds[0], soaap_buf, SOAAP_BUF_LEN)) > 0) {
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
      while(datalen_in > SOAAP_BUF_LEN) {
        nbytes = write(epfds[1], soaap_buf, SOAAP_BUF_LEN);
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
    //wait(NULL);
  }
}

__attribute__((used)) static void
soaap_perf_enter_datainout_ephemeral_sbox(int datalen_in, int datalen_out)
{
  int nbytes, nbytes_left;
  int epfds[2];
  pid_t pid;
  uint32_t *magicptr;
  struct ctrl_msg cm, *cmptr;

  DPRINTF("Emulating performance of using ephemeral sandbox.");

  if (!datalen_in && !datalen_out)
    return;

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
    /* SANDBOX */
    close(epfds[1]);
    while ((nbytes = read(epfds[0], soaap_buf, SOAAP_BUF_LEN)) > 0) {
      DPRINTF(" SANDBOX: read %d bytes", nbytes);
      if (nbytes >= (int) sizeof(struct ctrl_msg)) {
        cmptr = (struct ctrl_msg *) soaap_buf;
        if (cmptr->magic & ~(MAGIC))
          continue;

        switch (cmptr->op) {
        case OP_SENDRECEIVE:
          nbytes_left = cmptr->sbox_datain_len + sizeof(struct ctrl_msg) - nbytes;
          DPRINTF("SANDBOX: waiting to receive %d", nbytes_left);

          /* Chew all incoming data */
          while (nbytes_left) {
            if ((nbytes = read(epfds[0], soaap_buf, SOAAP_BUF_LEN)) > 0)
              nbytes_left -= nbytes;
          }
        case OP_SENDBACK:
          /* Fallback */
          /* Send back data */
          DPRINTF("SANDBOX: sending back %d bytes", cmptr->sbox_dataout_len);
          while (cmptr->sbox_dataout_len > SOAAP_BUF_LEN) {
            nbytes = write(epfds[0], soaap_tmpbuf, SOAAP_BUF_LEN);
            cmptr->sbox_dataout_len -= nbytes;
          }
          while (cmptr->sbox_dataout_len > 0) {
            nbytes = write(epfds[0], soaap_tmpbuf, cmptr->sbox_dataout_len);
            cmptr->sbox_dataout_len -= nbytes;
          }
          break;
        default:
          DPRINTF("Unknown operation");
        }
      }
    }
    DPRINTF(" SANDBOX: exiting");
    exit(0);

  } else {
    /* PARENT */
    close(pfds[0]);

    /* Initialize cm */
    cm.magic = MAGIC;
    cm.op = OP_SENDRECEIVE;
    cm.sbox_datain_len = datalen_in; // Sandbox doesn't need to wait for more data
    cm.sbox_dataout_len = datalen_out; // Sandbox doesn't need to wait for more data

    /* Initialize ctrl message and required data */
    nbytes_left = sizeof(cm) + datalen_in;
    memmove(soaap_buf, &cm, sizeof(cm));
    magicptr = (uint32_t *) soaap_buf;

    /*
     * Write datalen_in bytes over the pipe in chunks of buflen (PAGESIZE).
     * We are exiting if this function is called with a negative integer as
     * argument.
     */
    while(nbytes_left > SOAAP_BUF_LEN) {
      nbytes = write(epfds[1], soaap_buf, SOAAP_BUF_LEN);
      if (nbytes < 0) {
        perror("PARENT write()");
        return;
      }
      DPRINTF("PARENT: written to the pipe %d bytes", nbytes);

      /* Unset the magic */
      *magicptr &= ~(MAGIC);

      /* Update remaining bytes */
      nbytes_left -= nbytes;
    }
    while (nbytes_left > 0) {
      nbytes = write(epfds[1], soaap_buf, nbytes_left);
      if (nbytes < 0) {
        perror("PARENT write()");
        return;
      }
      DPRINTF("PARENT: written to the pipe %d bytes", nbytes);
      nbytes_left -= nbytes;
    }

    /* Chew the data that sent from the sandbox */
    nbytes_left = cm.sbox_dataout_len;
    while( nbytes_left > 0 ) {
      nbytes = read(epfds[1], soaap_tmpbuf, SOAAP_BUF_LEN);
      DPRINTF("PARENT: read from fd %d bytes", nbytes);
      nbytes_left -= nbytes;
    }

    /* Send EOF to sandbox, cleanup and wait */
    close(epfds[1]);
    //wait(NULL);
  }
}

__attribute__((used)) static void
soaap_perf_tic(struct timespec *start_ts)
{

#ifdef PROF
  DPRINTF("SANDBOXED FUNCTION PROLOGUE -- TIC!");
  clock_gettime(CLOCK_MONOTONIC, start_ts);
#else
    ;
#endif
}

__attribute__((used)) static void
soaap_perf_overhead_toc(struct timespec *sbox_ts)
{
#ifdef PROF
  DPRINTF("SANDBOXED FUNCTION OVERHEAD -- SBOX_TOC!");
  clock_gettime(CLOCK_MONOTONIC, sbox_ts);
#else
    ;
#endif
}

__attribute__((used)) static void
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

__attribute__((used)) static void
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

#endif  /* IN_SOAAP_INSTRUMENTER */

#endif  /* _SOAAP_PERF_H_ */
