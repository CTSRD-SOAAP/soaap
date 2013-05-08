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
#define __soaap_vuln_fn(CVE) __attribute__((annotate(PAST_VULNERABILITY"_"CVE))) __attribute__((noinline))
#define __soaap_vuln_pt(CVE) __soaap_past_vulnerability_at_point(CVE);
__attribute__((noinline)) static void __soaap_past_vulnerability_at_point(char* cve) {
  int result;
  // hack to prevent LLVM from inlining cve
  __asm__ volatile( "addl %%ebx, %%eax;"
                    : "=a" (result)
                    : "a" (cve[0]), "b" (cve[1]) );
  if (result == 909092) {
    char dummybuf[1];
    dummybuf[0] = '\0';
    __soaap_past_vulnerability_at_point(dummybuf);
  }
}

// code provenance
#define __soaap_provenance(X) \
  static char* __attribute__((used)) __soaap_provenance_var = X;

#define __soaap_sandbox_persistent(N) __attribute__((annotate(SANDBOX_PERSISTENT"_"N))) __attribute__((noinline))
#define __soaap_sandbox_ephemeral(N) __attribute__((annotate(SANDBOX_EPHEMERAL"_"N))) __attribute__((noinline))
#define __soaap_var_read(N) __attribute__((annotate(VAR_READ"_"N)))
#define __soaap_var_write(N) __attribute__((annotate(VAR_WRITE"_"N)))
#define __soaap_fd_read __attribute__((annotate(FD_READ)))
#define __soaap_fd_write __attribute__((annotate(FD_WRITE)))
#define __soaap_indirect_fd_read(F) __attribute__((annotate(F##"_"##FD_READ)))
#define __soaap_indirect_fd_write(F) __attribute__((annotate(F##"_"##FD_WRITE)))
/*#define __soaap_callgates(fns...) \
  void __soaap_declare_callgates_helper(int unused, ...) { } \
	void __soaap_declare_callgates() { \
		__soaap_declare_callgates_helper(0, fns); \
	}*/

#define __soaap_classify(L) __attribute__((annotate(CLASSIFY"_"L)))
#define __soaap_clearance(L) __attribute__((annotate(CLEARANCE"_"L)))
#define __soaap_private(N) __attribute__((annotate(SANDBOX_PRIVATE"_"N)))

#define SOAAP_EPHEMERAL_SANDBOX_CREATE "SOAAP_EPHEMERAL_SANDBOX_CREATE"
#define SOAAP_EPHEMERAL_SANDBOX_KILL "SOAAP_EPHEMERAL_SANDBOX_KILL"
#define __soaap_create_ephemeral_sandbox(N) __builtin_annotation(0, SOAAP_EPHEMERAL_SANDBOX_CREATE"_"N)
#define __soaap_kill_ephemeral_sandbox(N) __builtin_annotation(0, SOAAP_EPHEMERAL_SANDBOX_KILL"_"N)

#define SOAAP_PERSISTENT_SANDBOX_CREATE "SOAAP_PERSISTENT_SANDBOX_CREATE"
#define SOAAP_PERSISTENT_SANDBOX_KILL "SOAAP_PERSISTENT_SANDBOX_KILL"
#define __soaap_create_persistent_sandbox(N) __builtin_annotation(0, SOAAP_PERSISTENT_SANDBOX_CREATE"_"N)
#define __soaap_kill_persistent_sandbox(N) __builtin_annotation(0, SOAAP_PERSISTENT_SANDBOX_KILL"_"N)

#define SOAAP_SANDBOX_CODE_START "SOAAP_SANDBOX_CODE_START"
#define SOAAP_SANDBOX_CODE_END "SOAAP_SANDBOX_CODE_END"
#define __soaap_start_sandboxed_code(N) __builtin_annotation(0, SOAAP_SANDBOX_CODE_START"_"N);
#define __soaap_end_sandboxed_code(N) __builtin_annotation(0, SOAAP_SANDBOX_CODE_END"_"N); 

#define __soaap_callgates(N, fns...) \
  void __soaap_declare_callgates_helper_##N##(int unused, ...) { } \
	void __soaap_declare_callgates_##N##() { \
		__soaap_declare_callgates_helper_##N##(0, fns); \
	} 

#define SOAAP_PRIVILEGED "PRIVILEGED"
#define __soaap_privileged __attribute__((annotate(SOAAP_PRIVILEGED)))

#define __soaap_sync_data(N)

#endif /* SOAAP_H */
