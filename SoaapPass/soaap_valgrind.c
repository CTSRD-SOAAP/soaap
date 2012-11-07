/*
 * soaap_valgrind.c
 *
 *  Created on: Oct 8, 2012
 *      Author: khilan
 */

#include "soaap.h"

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
