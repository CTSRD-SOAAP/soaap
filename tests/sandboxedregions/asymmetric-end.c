/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -soaap-analyses=vulnerability,syscalls,privcalls,sandboxed -soaap-list-sandboxed-funcs -soaap-output-traces=all -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */

#include <soaap.h>
#include <fcntl.h>

extern int condition;

int endSandbox() {
	open("/foo/bar", O_RDONLY);
	__soaap_sandboxed_region_end("box");
	return 0;
}

int main() {
	__soaap_sandboxed_region_start("box");
	void* authctxt;
	/* perform the key exchange */
	/* authenticate user and start session */
	if (condition) {
		open("/foo/bar", O_RDONLY);
		__soaap_sandboxed_region_end("box");
	} else {
		endSandbox();
	}
}

// CHECK: * Listing sandboxed functions
// CHECK:   Sandbox: box (ephemeral)
// CHECK:     endSandbox ([[TESTDIR:.*]]/sandboxedregions/asymmetric-end.c)

//CHECK:  *** Sandbox "box" performs system call "open" but it is not allowed to,
//CHECK:  *** based on the current sandboxing restrictions.
//CHECK:  +++ Line 15 of file [[TESTDIR]]/sandboxedregions/asymmetric-end.c
//CHECK:  *** Sandbox "box" performs system call "open" but it is not allowed to,
//CHECK:  *** based on the current sandboxing restrictions.
//CHECK:  +++ Line 26 of file [[TESTDIR]]/sandboxedregions/asymmetric-end.c
