/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 *
 * XFAIL: darwin,linux,windows
 */
#include "soaap.h"

#include <sys/capability.h>    /* TODO: change to sys/capsicum.h */

#include <fcntl.h>
#include <unistd.h>

/*
 * An extremely simple model of Capsicum's cap_enter():
 * disallow all system calls except for a whitelist.
 *
 * We can't statically express all of the Capsicum behaviour
 * (e.g., namespace subsetting), but we can express some things.
 *
 * TODO: tag the cap_enter() declaration with these syscalls so that we
 *       don't have to manually annotate every call site.
 */
#define SOAAP_CAPSICUM_CAPABILITY_MODE \
	__soaap_sandboxed_region_start("cap_sandbox"); \
	__soaap_limit_syscalls(cap_rights_limit, read, write, openat, exit);

int	cap_enter(void);


int main(int argc, char** argv)
{
	cap_rights_t rights;
	char buffer[10];
	int passwd;

	passwd = open("/etc/passwd", O_RDONLY);

	cap_enter();
	SOAAP_CAPSICUM_CAPABILITY_MODE

	/*
	 * cap_rights_limit is permitted by SOAAP_CAPSICUM_CAPABILITY_MODE
	 * and we have not imposed any restrictions on 'passwd' yet.
	 *
	 * CHECK-NOT: performs system call "cap_rights_limit" but
	 */
	cap_rights_limit(passwd, cap_rights_init(&rights, CAP_READ));
	__soaap_limit_fd_syscalls(passwd, read);

	/*
	 * This is ok: we have CAP_READ.
	 *
	 * CHECK-NOT: performs system call "read" but
	 */
	read(passwd, buffer, 1);

	/*
	 * This is not ok: we only have CAP_READ on passwd now.
	 *
	 * CHECK: performs system call "write" but
	 */
	write(passwd, "foo", 4);

	return 0;
}
