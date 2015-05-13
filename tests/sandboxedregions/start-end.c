/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -soaap-analyses=vulnerability,syscalls,privcalls,sandboxed -soaap-list-sandboxed-funcs -soaap-output-traces=all -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */

#include <soaap.h>
#include <stdlib.h>
#include <fcntl.h>

#define use_privsep 1

extern int compat20;

__soaap_privileged extern int do_fork();

__soaap_sandboxed("preauth")
void* do_authentication2(void) {
	open("/foo/bar", O_RDONLY);
	return NULL;
}
__soaap_sandboxed("preauth")
void* do_authentication(void) {
	open("/foo/bar", O_RDONLY);
	return NULL;
}

__soaap_sandboxed("postauth")
void* do_postauth(void) {
	open("/foo/bar", O_RDONLY);
	return NULL;
}

int main() {
	if (do_fork()) {
		goto authenticated;
	}
	__soaap_sandboxed_region_start("preauth");
	void* authctxt;
	/* perform the key exchange */
	/* authenticate user and start session */
	if (compat20) {
		/* do_ssh2_kex(); */
		authctxt = do_authentication2();
	} else {
		//do_ssh1_kex();
		authctxt = do_authentication();
	}
	/*
	 * If we use privilege separation, the unprivileged child transfers
	 * the current keystate and exits
	 */
	if (use_privsep) {
		// must have end annotation before exit() since everything after this is flagged as unreachable
		// because llvm adds an unreachable instr after exit()
		__soaap_sandboxed_region_end("preauth");
		exit(0);
	}
authenticated:
	__soaap_sandboxed_region_start("postauth");
	do_postauth();
	__soaap_sandboxed_region_end("postauth");
}

// CHECK: * Listing sandboxed functions
// CHECK:   Sandbox: postauth (ephemeral)
// CHECK:     do_postauth ([[TESTDIR:.*]]/sandboxedregions/start-end.c)
// CHECK:   Sandbox: preauth (ephemeral)
// CHECK:     do_authentication2 ([[TESTDIR]]/sandboxedregions/start-end.c)
// CHECK:     do_authentication ([[TESTDIR]]/sandboxedregions/start-end.c)
// CHECK-NOT:  *** Function "do_authentication" has been annotated as only being allowed to execute in the sandboxes: [preauth] but it may execute in a privileged context


//CHECK:  *** Sandbox "postauth" performs system call "open" but it is not allowed to,
//CHECK:  *** based on the current sandboxing restrictions.
//CHECK:  +++ Line 32 of file [[TESTDIR]]/sandboxedregions/start-end.c
//CHECK:  *** Sandbox "preauth" performs system call "open" but it is not allowed to,
//CHECK:  *** based on the current sandboxing restrictions.
//CHECK:  +++ Line 21 of file [[TESTDIR]]/sandboxedregions/start-end.c
//CHECK:  *** Sandbox "preauth" performs system call "open" but it is not allowed to,
//CHECK:  *** based on the current sandboxing restrictions.
//CHECK:  +++ Line 26 of file [[TESTDIR]]/sandboxedregions/start-end.c
