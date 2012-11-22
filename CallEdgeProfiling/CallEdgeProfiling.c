/*
 * DynCGPass.c
 *
 *  Created on: Oct 22, 2012
 *      Author: khilan
 */

#include "Profiling.h"
#include <stdlib.h>

static unsigned* ArrayStart;
static unsigned NumElements;

/* CallEdgeProfAtExitHandler - When the program exits, just write out the profiling
 * data.
 */
static void CallEdgeProfAtExitHandler(void) {
  //printf("NumElements: %d\n", NumElements);
  write_profiling_data(CallEdgeInfo, ArrayStart, NumElements);
}

/* llvm_start_edge_profiling - This is the main entry point of the call edge
 * profiling library.  It is responsible for setting up the atexit handler.
 */
int llvm_start_call_edge_profiling(int argc, const char **argv,
                              unsigned* arrayStart, unsigned numElements) {
  int Ret = save_arguments(argc, argv);
  //printf("numElements: %d\n", numElements);
  ArrayStart = arrayStart;
  NumElements = numElements;
  atexit(CallEdgeProfAtExitHandler);
  return Ret;
}
