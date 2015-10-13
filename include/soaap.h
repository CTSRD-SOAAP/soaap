/*
 * Copyright (c) 2013-2015 Khilan Gudka
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * File: soaap.h
 *
 *      This is a sample header file that is global to the entire project.
 *      It is located here so that everyone will find it.
 */

#ifndef SOAAP_H
#define SOAAP_H

//#include "valgrind/taintgrind.h"
#pragma GCC system_header

#define __weak_reference2(sym,alias) \
  extern __typeof (sym) alias __attribute__ ((weak, __alias__ (#sym)))

// types of sandboxes
#define SANDBOX_PERSISTENT "SANDBOX_PERSISTENT"
#define SANDBOX_EPHEMERAL "SANDBOX_EPHEMERAL"

// permissions for variables
#define VAR_READ "VAR_READ"
#define VAR_READ_MASK 0x1
#define VAR_WRITE "VAR_WRITE"
#define VAR_WRITE_MASK 0x2

// permissions for file descriptors
#define SOAAP_FD "SOAAP_FD"
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
#define __soaap_vuln_fn(CVE) __attribute__((annotate(PAST_VULNERABILITY "_" CVE))) __attribute__((noinline))
#define __soaap_vuln_pt(CVE) __soaap_past_vulnerability_at_point(CVE);
__attribute__((noinline)) static void __soaap_past_vulnerability_at_point(const char* cve) {
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

#define __soaap_live_vuln_fn(CVE) __soaap_vuln_fn(CVE)
#define __soaap_live_vuln_pt(CVE) __soaap_vuln_pt(CVE)

// code provenance
#define __soaap_provenance(X) \
  static char* __attribute__((used)) __soaap_provenance_var = X;

#define __soaap_sandbox_persistent(N) __attribute__((annotate(SANDBOX_PERSISTENT "_" N))) __attribute__((noinline))
#define __soaap_sandbox_ephemeral(N) __attribute__((annotate(SANDBOX_EPHEMERAL "_" N))) __attribute__((noinline))
#define __soaap_var_read(N) __attribute__((annotate(VAR_READ "_" N)))
#define __soaap_var_write(N) __attribute__((annotate(VAR_WRITE "_" N)))
#define __soaap_fd_read __soaap_fd_permit(read)
#define __soaap_fd_write __soaap_fd_permit(write)
#define __soaap_indirect_fd_read(F) __attribute__((annotate(F "_" FD_READ)))
#define __soaap_indirect_fd_write(F) __attribute__((annotate(F "_" FD_WRITE)))
#define __soaap_fd_permit(...) __attribute__((annotate(SOAAP_FD "_" #__VA_ARGS__)))
/*#define __soaap_callgates(fns...) \
  void __soaap_declare_callgates_helper(int unused, ...) { } \
	void __soaap_declare_callgates() { \
		__soaap_declare_callgates_helper(0, fns); \
	}*/

#define __soaap_classify(L) __attribute__((annotate(CLASSIFY "_" L)))
#define __soaap_clearance(L) __attribute__((annotate(CLEARANCE "_" L)))
#define __soaap_private(N) __attribute__((annotate(SANDBOX_PRIVATE "_" N)))
__attribute__((noinline)) static void __soaap_declassify(void* v) { }

#define __soaap_sync_data(N)

#define SOAAP_EPHEMERAL_SANDBOX_CREATE "SOAAP_EPHEMERAL_SANDBOX_CREATE"
#define SOAAP_EPHEMERAL_SANDBOX_KILL "SOAAP_EPHEMERAL_SANDBOX_KILL"
#define __soaap_create_ephemeral_sandbox(N) __builtin_annotation(0, SOAAP_EPHEMERAL_SANDBOX_CREATE "_" N)
#define __soaap_kill_ephemeral_sandbox(N) __builtin_annotation(0, SOAAP_EPHEMERAL_SANDBOX_KILL "_" N)

#define SOAAP_PERSISTENT_SANDBOX_CREATE "SOAAP_PERSISTENT_SANDBOX_CREATE"
#define SOAAP_PERSISTENT_SANDBOX_KILL "SOAAP_PERSISTENT_SANDBOX_KILL"
#define __soaap_create_persistent_sandbox(N) __builtin_annotation(0, SOAAP_PERSISTENT_SANDBOX_CREATE "_" N)
#define __soaap_kill_persistent_sandbox(N) __builtin_annotation(0, SOAAP_PERSISTENT_SANDBOX_KILL "_" N)

#define SOAAP_SANDBOX_REGION_START "SOAAP_SANDBOX_REGION_START"
#define SOAAP_SANDBOX_REGION_END "SOAAP_SANDBOX_REGION_END"
#define __soaap_sandboxed_region_start(N) __builtin_annotation(0, SOAAP_SANDBOX_REGION_START "_" N);
#define __soaap_sandboxed_region_end(N) __builtin_annotation(0, SOAAP_SANDBOX_REGION_END "_" N); 

#define __soaap_callgates(N, fns...) \
  void __soaap_declare_callgates_helper_##N(int unused, ...) { \
    __asm__ volatile( "nop" ); \
  } \
	void __soaap_declare_callgates_##N() { \
		__soaap_declare_callgates_helper_##N(0, fns); \
	} 

#define SOAAP_PRIVILEGED "SOAAP_PRIVILEGED"
#define __soaap_privileged __attribute__((annotate(SOAAP_PRIVILEGED)))

#define SOAAP_SANDBOXED "SOAAP_SANDBOXED"
#define __soaap_sandboxed(sboxes...) __attribute__((annotate(SOAAP_SANDBOXED "_" #sboxes)))

#define SOAAP_FP "SOAAP_FP"
#define __soaap_fp(fns...) __attribute__((annotate(SOAAP_FP "_" #fns)))

#define SOAAP_DANGEROUS "SOAAP_DANGEROUS"
#define __soaap_dangerous __attribute__((annotate(SOAAP_DANGEROUS)))

#define SOAAP_NO_SYSCALLS_ALLOWED "SOAAP_NO_SYSCALLS_ALLOWED"

/**
 * Limit the system calls that can be performed from this point on in the
 * current execution process (sandbox or regular process).
 *
 * This is a mechanism-description annotation that should eventually be
 * subsumed into SOAAP itself as it learns about more mechanisms' semantics.
 */
#define SOAAP_SYSCALLS "SOAAP_SYSCALLS"
#define __soaap_limit_syscalls_afterx(syscalls...) \
  __builtin_annotation(0, SOAAP_SYSCALLS "_" #syscalls)
#define __soaap_limit_syscalls(syscalls...) \
  __soaap_limit_syscalls_afterx(syscalls)

/**
 * Limit the system calls that can be called with respect to a file descriptor.
 *
 * This mask of allowable system calls should be combined with the global
 * syscall mask (if any) when determining whether or not a system call will
 * be permitted by the sandboxing mechanism.
 *
 * This is a mechanism-description annotation that should eventually be
 * subsumed into SOAAP itself as it learns about more mechanisms' semantics.
 */
#define SOAAP_FD_SYSCALLS "SOAAP_FD_SYSCALLS"
#define __soaap_limit_fd_syscalls_afterx(fd, syscalls...) \
  __builtin_annotation(fd, SOAAP_FD_SYSCALLS "_" #syscalls)
#define __soaap_limit_fd_syscalls(fd, syscalls...) \
  __soaap_limit_fd_syscalls_afterx(fd, syscalls)

/**
 * Limit the system calls that can be called with respect to a file descriptor
 * that is stored within some collection and accessed using a key.
 *
 * This mask of allowable system calls should be combined with the global
 * syscall mask (if any) when determining whether or not a system call will
 * be permitted by the sandboxing mechanism.
 *
 * This is a mechanism-description annotation that should eventually be
 * subsumed into SOAAP itself as it learns about more mechanisms' semantics.
 */
#define SOAAP_FD_KEY_SYSCALLS "SOAAP_FD_KEY_SYSCALLS"
#define __soaap_limit_fd_key_syscalls(fdkey, syscalls...) \
  __builtin_annotation(fdkey, SOAAP_FD_KEY_SYSCALLS "_" #syscalls)

/**
 * The function returns the file descriptor corresponding to a supplied key.
 */
#define SOAAP_FD_GETTER "SOAAP_FD_GETTER"
#define __soaap_fd_getter __attribute__((annotate(SOAAP_FD_GETTER))) __attribute__((noinline))

/**
 * The function maps key values to file descriptors.
 */
#define SOAAP_FD_SETTER "SOAAP_FD_SETTER"
#define __soaap_fd_setter __attribute__((annotate(SOAAP_FD_SETTER))) __attribute__((noinline))

__attribute__((noinline)) static void __soaap_rpc_send_helper(const char* recipient, char* message_type,...) { }
__attribute__((noinline)) static void __soaap_rpc_send_recv_helper(const char* recipient, char* message_type,...) { }
#define __soaap_rpc_send(RECIPIENT, MESSAGE_TYPE) \
  __soaap_rpc_send_helper(RECIPIENT, #MESSAGE_TYPE);
#define __soaap_rpc_send_with_params(RECIPIENT, MESSAGE_TYPE, PARAMS...) \
  __soaap_rpc_send_helper(RECIPIENT, #MESSAGE_TYPE, PARAMS);
//#define __soaap_rpc_send_recv_sync(RECIPIENT, MESSAGE_TYPE, PARAMS...) \
//  __soaap_rpc_send_recv_helper(RECIPIENT, #MESSAGE_TYPE, PARAMS);

__attribute__((noinline)) static void __soaap_rpc_recv_helper(const char* sender, char* message_type, void* handler) { }
__attribute__((noinline)) static void __soaap_rpc_recv_sync_helper(const char* sender, char* message_type) { }
#define __soaap_rpc_recv(SENDER, MESSAGE_TYPE, HANDLER) \
  __soaap_rpc_recv_helper(SENDER, #MESSAGE_TYPE, HANDLER);
#define __soaap_rpc_recv_sync(SENDER, MESSAGE_TYPE) \
  __soaap_rpc_recv_sync_helper(SENDER, #MESSAGE_TYPE);

#endif /* SOAAP_H */
