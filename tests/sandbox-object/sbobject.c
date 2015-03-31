/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */

#include <fcntl.h>
#include <stddef.h>
#include <soaap.h>

/*
 * TODO: here are the annotations I think we need
 */
#define __soaap_dangerous __attribute__((annotate("dangerous")))

#define __soaap_sandbox_type __attribute__((annotate("sandbox_type")))

#define __soaap_creates_sandbox(sandbox_t) \
	__attribute__((annotate("creates_sandbox" #sandbox_t)))

#define __soaap_destroys_sandbox(sandbox_t) \
	__attribute__((annotate("destroys_sandbox" #sandbox_t)))


/**
 * A data structure full of DANGER!
 *
 * There is just no way for mortal programmers to process this safely.
 */
struct dangerous_data
{
	void *data;
	size_t length;
	int bits_per_datum;
	int padding;
	int stride;
}
__soaap_dangerous;

//! Read a @ref dangerous_data object from a file.
struct dangerous_data*	read_file(int fd);

//! Process data with weird and dangerous pointer arithmetic.
__soaap_vuln_fn("CVE-2015-999")
int			process_data(struct dangerous_data*);



struct sensitive_data
{
	void *secret_key;
	size_t keylen;
}
__soaap_classify("sensitive");


/**
 * The parent-side representation of a sandbox.
 */
struct sandbox
{
	/** File descriptor we use for sandbox RPCs. */
	int socket;
}
__soaap_sandbox_type;


__soaap_creates_sandbox(struct sandbox)
struct sandbox*	sandbox_new(void);

__soaap_destroys_sandbox(struct sandbox)
void		sandbox_free(struct sandbox*);


#ifdef ELIDE_SANDBOX
#define DO(fn, sandbox, ...) fn(__VA_ARGS__)
#else
void	rpc(struct sandbox*, ...);
#define DO(fn, sandbox, ...) rpc(sandbox, fn, __VA_ARGS__)
#endif



int main(int argc, char *argv[])
{
	struct sensitive_data must_not_leak;

	int file = open(argv[1], O_RDONLY);
	struct dangerous_data *data = read_file(file);

#ifndef ELIDE_SANDBOX
	struct sandbox *worker = sandbox_new();
#endif

	DO(process_data, worker, data);

#ifndef ELIDE_SANDBOX
	sandbox_free(worker);
#endif

	return 0;
}
