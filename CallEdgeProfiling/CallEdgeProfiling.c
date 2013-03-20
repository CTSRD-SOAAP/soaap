/*
 * DynCGPass.c
 *
 *  Created on: Oct 22, 2012
 *      Author: khilan
 */

#include "Profiling.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <err.h>

//#ifdef DEBUG
#define DPRINTF(format, ...)        \
    fprintf(stderr, "%d: %s [%d] " format "\n",   \
            getpid(), __FUNCTION__, __LINE__, ##__VA_ARGS__)
//#else
//#define DPRINTF(format, ...)
//#endif
    
// profiling counters array
static unsigned* ArrayStart;
static unsigned NumElements;

// pid of privileged parent process
static unsigned parentPid;

// lock
static unsigned* lockState;
void lock(void);
void unlock(void);

/* CallEdgeProfAtExitHandler - When the program exits, just write out the profiling
 * data.
 */
static void soaap_call_edge_prof_atexit_handler(void) {
  DPRINTF("handler called by %d", getpid());
  if (getpid() == parentPid) {
    DPRINTF("Writing profiling data to file");
    write_profiling_data(CallEdgeInfo, ArrayStart, NumElements);
    munmap(ArrayStart, NumElements);
    DPRINTF("Writing complete");
  }
}

/* soaap_start_edge_profiling - This is the main entry point of the call edge
 * profiling library.  It is responsible for setting up the atexit handler.
 */
int soaap_start_call_edge_profiling(int argc, const char **argv,
                              unsigned* arrayStart, unsigned numElements) {
  DPRINTF("profiling started by %d", getpid());

  int Ret = save_arguments(argc, argv);
  NumElements = numElements;

  // create shared memory for counts and lock
  if ((ArrayStart = mmap(NULL, (numElements+1)*sizeof(unsigned), PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0)) == MAP_FAILED) {
    errx(1, "mmap failed");
  }
  lockState = &ArrayStart[numElements];

  parentPid = getpid();
  
  // force the llvmprof.out file to be created in the privileged parent
  //int outFile = getOutFile(); 
  DPRINTF("NumElements: %d\n");
  
  // setup atexit handler
  atexit(soaap_call_edge_prof_atexit_handler);
  return Ret;
}

void soaap_increment_call_edge_counter(int callerId, int calleeId, int numFuncs) {
  lock();
  DPRINTF("Incrementing %d -> %d (%d)", callerId, calleeId, numFuncs);
  ArrayStart[callerId*(numFuncs+1)+calleeId]++;
  unlock();
}

void lock() {
  while(__sync_bool_compare_and_swap(lockState, 0, 1)) { }
  DPRINTF("Lock acquired");
}

void unlock() {
  *lockState = 0;
  __sync_synchronize();
  DPRINTF("Lock released");
}
