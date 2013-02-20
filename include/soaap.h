/*
 * File: soaap.h
 *
 *      This is a sample header file that is global to the entire project.
 *      It is located here so that everyone will find it.
 */

#ifndef SOAAP_H
#define SOAAP_H

//#include "valgrind/taintgrind.h"
#include <stdio.h>

// types of sandboxes
#define SANDBOX_PERSISTENT "SANDBOX_PERSISTENT"
#define SANDBOX_EPHEMERAL "SANDBOX_EPHEMERAL"

// permissions for variables
#define VAR_READ "VAR_READ"
#define VAR_READ_MASK 0x1
#define VAR_WRITE "VAR_WRITE"
#define VAR_WRITE_MASK 0x2

// permissions for file descriptors
#define FD_READ "FD_READ"
#define FD_READ_MASK 0x1
#define FD_WRITE "FD_WRITE"
#define FD_WRITE_MASK 0x2

// classification and clearance to access data
#define CLASSIFY "CLASSIFY"
#define CLEARANCE "CLEARANCE"

// sandbox-private data
#define SANDBOX_PRIVATE "SANDBOX_PRIVATE"

// past vulnerabilities
#define PAST_VULNERABILITY "PAST_VULNERABILITY"
#define __soaap_past_vulnerability_func __attribute__((annotate(PAST_VULNERABILITY))) __attribute__((noinline))
#define __soaap_past_vulnerability_point __soaap_past_vulnerability_at_point();
static void __soaap_past_vulnerability_at_point() {}

// code provenance
#define __soaap_provenance(X) \
  static char* __attribute__((used)) __soaap_provenance_var = X;

#define __soaap_sandbox __sandbox_persistent
#define __soaap_sandbox_persistent __attribute__((annotate(SANDBOX_PERSISTENT))) __attribute__((noinline))
#define __soaap_sandbox_persistent_named(N) __attribute__((annotate(SANDBOX_PERSISTENT"_"N))) __attribute__((noinline))
#define __soaap_sandbox_ephemeral __attribute__((annotate(SANDBOX_EPHEMERAL))) __attribute__((noinline))
#define __soaap_var_allow_read __attribute__((annotate(VAR_READ)))
#define __soaap_var_allow_write __attribute__((annotate(VAR_WRITE)))
#define __soaap_fd_allow_read __attribute__((annotate(FD_READ)))
#define __soaap_indirect_fd_read(F) __attribute__((annotate(F##"_"##FD_READ)))
#define __soaap_indirect_fd_write(F) __attribute__((annotate(F##"_"##FD_WRITE)))
#define __soaap_fd_allow_write __attribute__((annotate(FD_WRITE)))
#define __soaap_callgates(fns...) \
  void __soaap_declare_callgates_helper(int unused, ...) { } \
	void __soaap_declare_callgates() { \
		__soaap_declare_callgates_helper(0, fns); \
	} 

#define __soaap_classify(L) __attribute__((annotate(CLASSIFY"_"L)))
#define __soaap_clearance(L) __attribute__((annotate(CLEARANCE"_"L)))
#define __soaap_sandbox_private(N) __attribute__((annotate(SANDBOX_PRIVATE"_"N)))


#endif /* SOAAP_H */
