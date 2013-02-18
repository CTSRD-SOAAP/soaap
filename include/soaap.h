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

#define SANDBOX_PRIVATE "SANDBOX_PRIVATE"

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
	void __declare_callgates() { \
		__declare_callgates_helper(0, fns); \
	}

#define __soaap_classify(L) __attribute__((annotate(CLASSIFY"_"L)))
#define __soaap_clearance(L) __attribute__((annotate(CLEARANCE"_"L)))
#define __soaap_sandbox_private(N) __attribute__((annotate(SANDBOX_PRIVATE"_"N)))

void __declare_callgates_helper(int unused, ...) { }

void soaap_create_sandbox();
void soaap_enter_persistent_sandbox();
void soaap_exit_persistent_sandbox();
void soaap_enter_ephemeral_sandbox();
void soaap_exit_ephemeral_sandbox();

void soaap_shared_var(char* var_name, int perms);
void soaap_shared_fd(int fd, int perms);
void soaap_shared_file(FILE* file, int perms);

void soaap_exit_callgate();
void soaap_printf(char* str);

int printf(const char*, ...);

/*
// functions for valgrind-function-wrapping
// (see http://valgrind.org/docs/manual/manual-core-adv.html#manual-core-adv.wrapping)
void valgrind_get_orig_fn(OrigFn* fn);

void call_unwrapped_function_w_v(OrigFn* fn, unsigned long* retval);
void call_unwrapped_function_w_w(OrigFn* fn, unsigned long* retval, unsigned long arg1);
void call_unwrapped_function_w_ww(OrigFn* fn, unsigned long* retval, unsigned long arg1, unsigned long arg2);
void call_unwrapped_function_w_www(OrigFn* fn, unsigned long* retval, unsigned long arg1, unsigned long arg2, unsigned long arg3);
void call_unwrapped_function_w_wwww(OrigFn* fn, unsigned long* retval, unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4);
void call_unwrapped_function_w_5w(OrigFn* fn, unsigned long* retval, unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5);
void call_unwrapped_function_w_6w(OrigFn* fn, unsigned long* retval, unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5, unsigned long arg6);
void call_unwrapped_function_w_7w(OrigFn* fn, unsigned long* retval, unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5, unsigned long arg6, unsigned long arg7);
*/

#endif /* SOAAP_H */
