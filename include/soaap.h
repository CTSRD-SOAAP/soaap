/*
 * File: soaap.h
 *
 *      This is a sample header file that is global to the entire project.
 *      It is located here so that everyone will find it.
 */

#include "valgrind/taintgrind.h"
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

#define __sandbox __sandbox_persistent
#define __sandbox_persistent __attribute__((annotate(SANDBOX_PERSISTENT))) __attribute__((noinline))
#define __sandbox_ephemeral __attribute__((annotate(SANDBOX_EPHEMERAL))) __attribute__((noinline))
#define __var_read __attribute__((annotate(VAR_READ)))
#define __var_write __attribute__((annotate(VAR_WRITE)))
#define __fd_read __attribute__((annotate(FD_READ)))
#define __fd_write __attribute__((annotate(FD_WRITE)))
#define __callgates(fns...) \
	void __declare_callgates() { \
		__declare_callgates_helper(0, fns); \
	}

void __declare_callgates_helper(int unused, ...);

void soaap_create_sandbox();
void soaap_enter_persistent_sandbox();
void soaap_exit_persistent_sandbox();
void soaap_enter_ephemeral_sandbox();
void soaap_exit_ephemeral_sandbox();

void soaap_shared_var(char* var_name, int perms);
void soaap_shared_fd(int fd, int perms);
void soaap_shared_file(FILE* file, int perms);

void soaap_enter_callgate();
void soaap_exit_callgate();
void soaap_printf(char* str);

void soaap_create_sandbox() {
	TNT_CREATE_SANDBOX();
}

void soaap_enter_persistent_sandbox() {
	TNT_ENTER_PERSISTENT_SANDBOX();
}

void soaap_exit_persistent_sandbox() {
	TNT_EXIT_PERSISTENT_SANDBOX();
}

void soaap_enter_ephemeral_sandbox() {
	TNT_ENTER_EPHEMERAL_SANDBOX();
}

void soaap_exit_ephemeral_sandbox() {
	TNT_EXIT_EPHEMERAL_SANDBOX();
}

void soaap_shared_var(char* var_name, int perms) {
	TNT_SHARED_VAR(var_name,perms);
}


void soaap_shared_fd(int fd, int perms) {
	TNT_SHARED_FD(fd,perms);
}

void soaap_shared_file(FILE* file, int perms) {
	soaap_shared_fd(fileno(file), perms);
}

void soaap_enter_callgate() {
	TNT_ENTER_CALLGATE();
}

void soaap_exit_callgate() {
	TNT_EXIT_CALLGATE();
}

void __declare_callgates_helper(int unused, ...) {
	printf("hello\n");
}

void soaap_printf(char* str) {
	printf("%s", str);
}

extern int printf(const char*, ...);

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

void call_unwrapped_function_w_v(OrigFn* fn, unsigned long* retval) {
	CALL_FN_W_v(*retval, *fn);
}

void call_unwrapped_function_w_w(OrigFn* fn, unsigned long* retval, unsigned long arg1) {
	CALL_FN_W_W(*retval, *fn, arg1);
}

void call_unwrapped_function_w_ww(OrigFn* fn, unsigned long* retval, unsigned long arg1, unsigned long arg2) {
	CALL_FN_W_WW(*retval, *fn, arg1, arg2);
}

void call_unwrapped_function_w_www(OrigFn* fn, unsigned long* retval, unsigned long arg1, unsigned long arg2, unsigned long arg3) {
	CALL_FN_W_WWW(*retval, *fn, arg1, arg2, arg3);
}

void call_unwrapped_function_w_wwww(OrigFn* fn, unsigned long* retval, unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4) {
	CALL_FN_W_WWWW(*retval, *fn, arg1, arg2, arg3, arg4);
}

void call_unwrapped_function_w_5w(OrigFn* fn, unsigned long* retval, unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5) {
	CALL_FN_W_5W(*retval, *fn, arg1, arg2, arg3, arg4, arg5);
}

void call_unwrapped_function_w_6w(OrigFn* fn, unsigned long* retval, unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5, unsigned long arg6) {
	CALL_FN_W_6W(*retval, *fn, arg1, arg2, arg3, arg4, arg5, arg6);
}

void call_unwrapped_function_w_7w(OrigFn* fn, unsigned long* retval, unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5, unsigned long arg6, unsigned long arg7) {
	CALL_FN_W_7W(*retval, *fn, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}

void valgrind_get_orig_fn(OrigFn* fn) {
	VALGRIND_GET_ORIG_FN(*fn);
}
