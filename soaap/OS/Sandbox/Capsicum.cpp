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

#include "OS/Sandbox/Capsicum.h"

using namespace soaap;

Capsicum::Capsicum() {
  //FreeBSD system calls taken from FreeBSD/sys/kern/capabilities.conf
  ////
  // Allow ACL and MAC label operations by file descriptor, subject to
  // capability rights.  Allow MAC label operations on the current process but
  // we will need to scope __mac_get_pid(2).
  //
  addPermittedSysCall("__acl_aclcheck_fd", true);
  addPermittedSysCall("__acl_delete_fd", true);
  addPermittedSysCall("__acl_get_fd", true);
  addPermittedSysCall("__acl_set_fd", true);
  addPermittedSysCall("__mac_get_fd", true);
  addPermittedSysCall("__mac_get_proc", true);
  addPermittedSysCall("__mac_set_fd", true);
  addPermittedSysCall("__mac_set_proc", true);

  ////
  //// Allow sysctl(2) as we scope internal to the call; this is a global
  //// namespace, but there are several critical sysctls required for almost
  //// anything to run, such as hw.pagesize.  For now that policy lives in the
  //// kernel for performance and simplicity, but perhaps it could move to a
  //// proxying daemon in userspace.
  ////
  addPermittedSysCall("__sysctl");

  ////
  //// Allow umtx operations as these are scoped by address space.
  ////
  //// XXRW: Need to check this very carefully.
  ////
  addPermittedSysCall("_umtx_lock");
  addPermittedSysCall("_umtx_op");
  addPermittedSysCall("_umtx_unlock");

  ////
  //// Allow process termination using abort2(2).
  ////
  addPermittedSysCall("abort2");

  ////
  //// Allow accept(2) since it doesn't manipulate namespaces directly, rather
  //// relies on existing bindings on a socket, subject to capability rights.
  ////
  addPermittedSysCall("accept", true);
  addPermittedSysCall("accept4", true);
  capabilityMap["accept"] = CAP_ACCEPT;
  capabilityMap["accept4"] = CAP_ACCEPT;

  ////
  //// Allow AIO operations by file descriptor, subject to capability rights.
  ////
  addPermittedSysCall("aio_cancel", true);
  addPermittedSysCall("aio_error", true);
  addPermittedSysCall("aio_fsync", true);
  addPermittedSysCall("aio_read", true);
  addPermittedSysCall("aio_return", true);
  addPermittedSysCall("aio_suspend", true);
  addPermittedSysCall("aio_waitcomplete", true);
  addPermittedSysCall("aio_write", true);
  capabilityMap["aio_fsync"] = CAP_FSYNC;
  capabilityMap["aio_read"] = CAP_PREAD;

  ////
  //// Allow bindat(2).
  ////
  addPermittedSysCall("bindat");

  ////
  //// Allow capability mode and capability system calls.
  ////
  addPermittedSysCall("cap_enter");
  addPermittedSysCall("cap_fcntls_get");
  addPermittedSysCall("cap_fcntls_limit");
  addPermittedSysCall("cap_getmode");
  addPermittedSysCall("cap_ioctls_get");
  addPermittedSysCall("cap_ioctls_limit");
  addPermittedSysCall("__cap_rights_get");
  addPermittedSysCall("cap_rights_limit");

  ////
  //// Allow read-only clock operations.
  ////
  addPermittedSysCall("clock_getres");
  addPermittedSysCall("clock_gettime");

  ////
  //// Always allow file descriptor close(2).
  ////
  addPermittedSysCall("close");
  addPermittedSysCall("closefrom");

  ////
  //// Allow connectat(2).
  ////
  addPermittedSysCall("connectat");

  ////
  //// Always allow dup(2) and dup2(2) manipulation of the file descriptor table.
  ////
  addPermittedSysCall("dup");
  addPermittedSysCall("dup2");

  ////
  //// Allow extended attribute operations by file descriptor, subject to
  //// capability rights.
  ////
  addPermittedSysCall("extattr_delete_fd", true);
  addPermittedSysCall("extattr_get_fd", true);
  addPermittedSysCall("extattr_list_fd", true);
  addPermittedSysCall("extattr_set_fd", true);
  capabilityMap["extattr_delete_fd"] = CAP_EXTATTR_DELETE;
  capabilityMap["extattr_get_fd"] = CAP_EXTATTR_GET;
  capabilityMap["extattr_list_fd"] = CAP_EXTATTR_LIST;
  capabilityMap["extattr_set_fd"] = CAP_EXTATTR_SET;

  ////
  //// Allow changing file flags, mode, and owner by file descriptor, subject to
  //// capability rights.
  ////
  addPermittedSysCall("fchflags", true);
  addPermittedSysCall("fchmod", true);
  addPermittedSysCall("fchown", true);
  capabilityMap["fchflags"] = CAP_FCHFLAGS;
  capabilityMap["fchmod"] = CAP_FCHMOD;
  capabilityMap["fchown"] = CAP_FCHOWN;

  ////
  //// For now, allow fcntl(2), subject to capability rights, but this probably
  //// needs additional scoping.
  ////
  addPermittedSysCall("fcntl", true);
  capabilityMap["fcntl"] = CAP_FCNTL;

  ////
  //// Allow fexecve(2), subject to capability rights.  We perform some scoping,
  //// such as disallowing privilege escalation.
  ////
  addPermittedSysCall("fexecve", true);
  capabilityMap["fexecve"] = CAP_FEXECVE;

  ////
  //// Allow flock(2), subject to capability rights.
  ////
  addPermittedSysCall("flock", true);
  capabilityMap["flock"] = CAP_FLOCK;

  ////
  //// Allow fork(2), even though it returns pids -- some applications seem to
  //// prefer this interface.
  ////
  addPermittedSysCall("fork");

  ////
  //// Allow fpathconf(2), subject to capability rights.
  ////
  addPermittedSysCall("fpathconf", true);
  capabilityMap["fpathconf"] = CAP_FPATHCONF;

  ////
  //// Allow various file descriptor-based I/O operations, subject to capability
  //// rights.
  ////
  addPermittedSysCall("freebsd6_ftruncat", true);
  addPermittedSysCall("freebsd6_lseek", true);
  addPermittedSysCall("freebsd6_mmap", true);
  addPermittedSysCall("freebsd6_pread", true);
  addPermittedSysCall("freebsd6_pwrite", true);

  ////
  //// Allow querying file and file system state with fstat(2) and fstatfs(2),
  //// subject to capability rights.
  ////
  addPermittedSysCall("fstat", true);
  addPermittedSysCall("fstatfs", true);
  capabilityMap["fstat"] = CAP_FSTAT;
  capabilityMap["fstatfs"] = CAP_FSTATFS;

  ////
  //// Allow further file descriptor-based I/O operations, subject to capability
  //// rights.
  ////
  addPermittedSysCall("fsync", true);
  addPermittedSysCall("ftruncate", true);
  capabilityMap["fsync"] = CAP_FSYNC;

  ////
  //// Allow futimes(2), subject to capability rights.
  ////
  addPermittedSysCall("futimes", true);
  capabilityMap["futimes"] = CAP_FUTIMES;

  ////
  //// Allow querying process audit state, subject to normal access control.
  ////
  addPermittedSysCall("getaudit");
  addPermittedSysCall("getaudit_addr");
  addPermittedSysCall("getauid");

  ////
  //// Allow thread context management with getcontext(2).
  ////
  addPermittedSysCall("getcontext");

  ////
  //// Allow directory I/O on a file descriptor, subject to capability rights.
  //// Originally we had separate capabilities for directory-specific read
  //// operations, but on BSD we allow reading the raw directory data, so we just
  //// rely on CAP_READ now.
  ////
  addPermittedSysCall("getdents", true);
  addPermittedSysCall("getdirentries", true);

  ////
  //// Allow querying certain trivial global state.
  ////
  addPermittedSysCall("getdomainname");

  ////
  //// Allow querying current process credential state.
  ////
  addPermittedSysCall("getegid");
  addPermittedSysCall("geteuid");

  ////
  //// Allow querying certain trivial global state.
  ////
  addPermittedSysCall("gethostid");
  addPermittedSysCall("gethostname");

  ////
  //// Allow querying per-process timer.
  ////
  addPermittedSysCall("getitimer");

  ////
  //// Allow querying current process credential state.
  ////
  addPermittedSysCall("getgid");
  addPermittedSysCall("getgroups");
  addPermittedSysCall("getlogin");

  ////
  //// Allow querying certain trivial global state.
  ////
  addPermittedSysCall("getpagesize");
  addPermittedSysCall("getpeername");

  ////
  //// Allow querying certain per-process scheduling, resource limit, and
  //// credential state.
  ////
  //// XXXRW: getpgid(2) needs scoping.  It's not clear if it's worth scoping
  //// getppid(2).  getpriority(2) needs scoping.  getrusage(2) needs scoping.
  //// getsid(2) needs scoping.
  ////
  addPermittedSysCall("getpgid");
  addPermittedSysCall("getpgrp");
  addPermittedSysCall("getpid");
  addPermittedSysCall("getppid");
  addPermittedSysCall("getpriority");
  addPermittedSysCall("getresgid");
  addPermittedSysCall("getresuid");
  addPermittedSysCall("getrlimit");
  addPermittedSysCall("getrusage");
  addPermittedSysCall("getsid");

  ////
  //// Allow querying socket state, subject to capability rights.
  ////
  //// XXXRW: getsockopt(2) may need more attention.
  ////
  addPermittedSysCall("getsockname", true);
  addPermittedSysCall("getsockopt", true);
  capabilityMap["getsockname"] = CAP_GETSOCKNAME;
  capabilityMap["getsockopt"] = CAP_GETSOCKOPT;

  ////
  //// Allow querying the global clock.
  ////
  addPermittedSysCall("gettimeofday");

  ////
  //// Allow querying current process credential state.
  ////
  addPermittedSysCall("getuid");

  ////
  //// Allow ioctl(2), which hopefully will be limited by applications only to
  //// required commands with cap_ioctls_limit(2) syscall.
  ////
  addPermittedSysCall("ioctl");
  capabilityMap["ioctl"] = CAP_IOCTL;

  ////
  //// Allow querying current process credential state.
  ////
  addPermittedSysCall("issetugid");

  ////
  //// Allow kevent(2), as we will authorize based on capability rights on the
  //// target descriptor.
  ////
  addPermittedSysCall("kevent");

  ////
  //// Allow kill(2), as we allow the process to send signals only to himself.
  ////
  addPermittedSysCall("kill");

  ////
  //// Allow message queue operations on file descriptors, subject to capability
  //// rights.
  ////
  addPermittedSysCall("kmq_notify", true);
  addPermittedSysCall("kmq_setattr", true);
  addPermittedSysCall("kmq_timedreceive", true);
  addPermittedSysCall("kmq_timedsend", true);

  ////
  //// Allow kqueue(2), we will control use.
  ////
  addPermittedSysCall("kqueue");

  ////
  //// Allow managing per-process timers.
  ////
  addPermittedSysCall("ktimer_create");
  addPermittedSysCall("ktimer_delete");
  addPermittedSysCall("ktimer_getoverrun");
  addPermittedSysCall("ktimer_gettime");
  addPermittedSysCall("ktimer_settime");

  ////
  //// Allow AIO operations by file descriptor, subject to capability rights.
  ////
  addPermittedSysCall("lio_listio", true);

  ////
  //// Allow listen(2), subject to capability rights.
  ////
  //// XXXRW: One might argue this manipulates a global namespace.
  ////
  addPermittedSysCall("listen", true);
  capabilityMap["listen"] = CAP_LISTEN;

  ////
  //// Allow I/O-related file descriptors, subject to capability rights.
  ////
  addPermittedSysCall("lseek", true);
  capabilityMap["lseek"] = CAP_SEEK;

  ////
  //// Allow simple VM operations on the current process.
  ////
  addPermittedSysCall("madvise");
  addPermittedSysCall("mincore");
  addPermittedSysCall("minherit");
  addPermittedSysCall("mlock");
  addPermittedSysCall("mlockall");

  ////
  //// Allow memory mapping a file descriptor, and updating protections, subject
  //// to capability rights.
  ////
  addPermittedSysCall("mmap", true);
  addPermittedSysCall("mprotect", true);
  capabilityMap["mmap"] = CAP_MMAP_RWX; // TODO specialise more

  ////
  //// Allow simple VM operations on the current process.
  ////
  addPermittedSysCall("msync");
  addPermittedSysCall("munlock");
  addPermittedSysCall("munlockall");
  addPermittedSysCall("munmap");

  ////
  //// Allow the current process to sleep.
  ////
  addPermittedSysCall("nanosleep");

  ////
  //// Allow querying the global clock.
  ////
  addPermittedSysCall("ntp_gettime");

  ////
  //// Allow AIO operations by file descriptor, subject to capability rights.
  ////
  addPermittedSysCall("oaio_read", true);
  addPermittedSysCall("oaio_write", true);

  ////
  //// Allow simple VM operations on the current process.
  ////
  addPermittedSysCall("obreak");

  ////
  //// Allow AIO operations by file descriptor, subject to capability rights.
  ////
  addPermittedSysCall("olio_listio", true);

  ////
  //// Operations relative to directory capabilities.
  ////
  addPermittedSysCall("chflagsat", true);
  addPermittedSysCall("faccessat", true);
  addPermittedSysCall("fchmodat", true);
  addPermittedSysCall("fchownat", true);
  addPermittedSysCall("fstatat", true);
  addPermittedSysCall("futimesat", true);
  addPermittedSysCall("linkat", true);  // TODO specialize
  addPermittedSysCall("mkdirat", true);
  addPermittedSysCall("mkfifoat", true);
  addPermittedSysCall("mknodat", true);
  addPermittedSysCall("openat", true); // TODO specialize
  addPermittedSysCall("readlinkat", true);
  addPermittedSysCall("renameat", true); // TODO specialize
  addPermittedSysCall("symlinkat", true);
  addPermittedSysCall("unlinkat", true);
  capabilityMap["chflagsat"] = CAP_CHFLAGSAT;
  capabilityMap["fchmodat"] = CAP_FCHMODAT;
  capabilityMap["fchownat"] = CAP_FCHOWNAT;
  capabilityMap["fstatat"] = CAP_FSTATAT;
  capabilityMap["futimesat"] = CAP_FUTIMESAT;
  capabilityMap["mkdirat"] = CAP_MKDIRAT;
  capabilityMap["mkfifoat"] = CAP_MKFIFOAT;
  capabilityMap["mknodat"] = CAP_MKNODAT;
  capabilityMap["openat"] = CAP_CREATE;
  capabilityMap["symlinkat"] = CAP_SYMLINKAT;
  capabilityMap["unlinkat"] = CAP_UNLINKAT;

  ////
  //// Allow poll(2), which will be scoped by capability rights.
  ////
  //// XXXRW: Perhaps we don't need the OpenBSD version?
  //// XXXRW: We don't yet do that scoping.
  ////
  addPermittedSysCall("openbsd_poll", true);

  ////
  //// Process descriptor-related system calls are allowed.
  ////
  addPermittedSysCall("pdfork");
  addPermittedSysCall("pdgetpid");
  addPermittedSysCall("pdkill");

  ////
  //// Allow pipe(2).
  ////
  addPermittedSysCall("pipe");
  addPermittedSysCall("pipe2");

  ////
  //// Allow poll(2), which will be scoped by capability rights.
  //// XXXRW: We don't yet do that scoping.
  ////
  addPermittedSysCall("poll", true);
  capabilityMap["poll"] = CAP_EVENT;

  ////
  //// Allow I/O-related file descriptors, subject to capability rights.
  ////
  addPermittedSysCall("pread", true);
  addPermittedSysCall("preadv", true);
  capabilityMap["pread"] = CAP_PREAD;
  capabilityMap["preadv"] = CAP_PREAD;

  ////
  //// Allow access to profiling state on the current process.
  ////
  addPermittedSysCall("profil");

  ////
  //// Allow I/O-related file descriptors, subject to capability rights.
  ////
  addPermittedSysCall("pwrite", true);
  addPermittedSysCall("pwritev", true);
  addPermittedSysCall("read", true);
  addPermittedSysCall("readv", true);
  addPermittedSysCall("recv", true);
  addPermittedSysCall("recvfrom", true);
  addPermittedSysCall("recvmsg", true);
  capabilityMap["pwrite"] = CAP_PWRITE;
  capabilityMap["pwritev"] = CAP_PWRITE;
  capabilityMap["read"] = CAP_READ;
  capabilityMap["readv"] = CAP_READ;
  capabilityMap["recv"] = CAP_RECV;

  ////
  //// Allow real-time scheduling primitives to be used.
  ////
  //// XXXRW: These require scoping.
  ////
  addPermittedSysCall("rtprio");
  addPermittedSysCall("rtprio_thread");

  ////
  //// Allow simple VM operations on the current process.
  ////
  addPermittedSysCall("sbrk");

  ////
  //// Allow querying trivial global scheduler state.
  ////
  addPermittedSysCall("sched_get_priority_max");
  addPermittedSysCall("sched_get_priority_min");

  ////
  //// Allow various thread/process scheduler operations.
  ////
  //// XXXRW: Some of these require further scoping.
  ////
  addPermittedSysCall("sched_getparam");
  addPermittedSysCall("sched_getscheduler");
  addPermittedSysCall("sched_rr_get_interval");
  addPermittedSysCall("sched_setparam");
  addPermittedSysCall("sched_setscheduler");
  addPermittedSysCall("sched_yield");

  ////
  //// Allow I/O-related file descriptors, subject to capability rights.
  ////
  addPermittedSysCall("sctp_generic_recvmsg", true);
  addPermittedSysCall("sctp_generic_sendmsg", true);
  addPermittedSysCall("sctp_generic_sendmsg_iov", true);
  addPermittedSysCall("sctp_peeloff", true);
  capabilityMap["sctp_peeloff"] = CAP_PEELOFF;

  ////
  //// Allow pselect(2) and select(2), which will be scoped by capability rights.
  ////
  //// XXXRW: But is it?
  ////
  addPermittedSysCall("pselect", true);
  addPermittedSysCall("select", true);
  capabilityMap["select"] = CAP_EVENT;

  ////
  //// Allow I/O-related file descriptors, subject to capability rights.  Use of
  //// explicit addresses here is restricted by the system calls themselves.
  ////
  addPermittedSysCall("send", true);
  addPermittedSysCall("sendfile", true);
  addPermittedSysCall("sendmsg", true);
  addPermittedSysCall("sendto", true);
  capabilityMap["send"] = CAP_SEND;
  capabilityMap["sendfile"] = CAP_SEND;
  capabilityMap["sendmsg"] = CAP_SEND;
  capabilityMap["sendto"] = CAP_SEND;

  ////
  //// Allow setting per-process audit state, which is controlled separately by
  //// privileges.
  ////
  addPermittedSysCall("setaudit");
  addPermittedSysCall("setaudit_addr");
  addPermittedSysCall("setauid");

  ////
  //// Allow setting thread context.
  ////
  addPermittedSysCall("setcontext");

  ////
  //// Allow setting current process credential state, which is controlled
  //// separately by privilege.
  ////
  addPermittedSysCall("setegid");
  addPermittedSysCall("seteuid");
  addPermittedSysCall("setgid");

  ////
  //// Allow use of the process interval timer.
  ////
  addPermittedSysCall("setitimer");

  ////
  //// Allow setpriority(2).
  ////
  //// XXXRW: Requires scoping.
  ////
  addPermittedSysCall("setpriority");

  ////
  //// Allow setting current process credential state, which is controlled
  //// separately by privilege.
  ////
  addPermittedSysCall("setregid");
  addPermittedSysCall("setresgid");
  addPermittedSysCall("setresuid");
  addPermittedSysCall("setreuid");

  ////
  //// Allow setting process resource limits with setrlimit(2).
  ////
  addPermittedSysCall("setrlimit");

  ////
  //// Allow creating a new session with setsid(2).
  ////
  addPermittedSysCall("setsid");

  ////
  //// Allow setting socket options with setsockopt(2), subject to capability
  //// rights.
  ////
  //// XXXRW: Might require scoping.
  ////
  addPermittedSysCall("setsockopt", true);
  capabilityMap["setsockopt"] = CAP_SETSOCKOPT;

  ////
  //// Allow setting current process credential state, which is controlled
  //// separately by privilege.
  ////
  addPermittedSysCall("setuid");

  ////
  //// shm_open(2) is scoped so as to allow only access to new anonymous objects.
  ////
  addPermittedSysCall("shm_open");

  ////
  //// Allow I/O-related file descriptors, subject to capability rights.
  ////
  addPermittedSysCall("shutdown", true);
  capabilityMap["shutdown"] = CAP_SHUTDOWN;

  ////
  //// Allow signal control on current process.
  ////
  addPermittedSysCall("sigaction");
  addPermittedSysCall("sigaltstack");
  addPermittedSysCall("sigblock");
  addPermittedSysCall("sigpending");
  addPermittedSysCall("sigprocmask");
  addPermittedSysCall("sigqueue");
  addPermittedSysCall("sigreturn");
  addPermittedSysCall("sigsetmask");
  addPermittedSysCall("sigstack");
  addPermittedSysCall("sigsuspend");
  addPermittedSysCall("sigtimedwait");
  addPermittedSysCall("sigvec");
  addPermittedSysCall("sigwaitinfo");
  addPermittedSysCall("sigwait");

  ////
  //// Allow creating new socket pairs with socket(2) and socketpair(2).
  ////
  addPermittedSysCall("socket");
  addPermittedSysCall("socketpair");

  ////
  //// Allow simple VM operations on the current process.
  ////
  //// XXXRW: Kernel doesn't implement this, so drop?
  ////
  addPermittedSysCall("sstk");

  ////
  //// Do allow sync(2) for now, but possibly shouldn't.
  ////
  addPermittedSysCall("sync");

  ////
  //// Always allow process termination with sys_exit(2).
  ////
  addPermittedSysCall("exit");
  addPermittedSysCall("sys_exit");

  ////
  //// sysarch(2) does rather diverse things, but is required on at least i386
  //// in order to configure per-thread data.  As such, it's scoped on each
  //// architecture.
  ////
  addPermittedSysCall("sysarch");

  ////
  //// Allow thread operations operating only on current process.
  ////
  addPermittedSysCall("thr_create");
  addPermittedSysCall("thr_exit");
  addPermittedSysCall("thr_kill");

  ////
  //// Allow thread operations operating only on current process.
  ////
  addPermittedSysCall("thr_new");
  addPermittedSysCall("thr_self");
  addPermittedSysCall("thr_set_name");
  addPermittedSysCall("thr_suspend");
  addPermittedSysCall("thr_wake");

  ////
  //// Allow manipulation of the current process umask with umask(2).
  ////
  addPermittedSysCall("umask");

  ////
  //// Allow submitting of process trace entries with utrace(2).
  ////
  addPermittedSysCall("utrace");

  ////
  //// Allow generating UUIDs with uuidgen(2).
  ////
  addPermittedSysCall("uuidgen");

  ////
  //// Allow I/O-related file descriptors, subject to capability rights.
  ////
  addPermittedSysCall("write", true);
  addPermittedSysCall("writev", true);
  capabilityMap["write"] = CAP_WRITE;
  capabilityMap["writev"] = CAP_WRITE;

  ////
  //// Allow processes to yield(2).
  ////
  addPermittedSysCall("yield");
}

std::vector<uint64_t> Capsicum::rightsForFunctions(FunctionSet& FS) {
  std::vector<uint64_t> rights;
  for (Function* F : FS) {
    rights.push_back(capabilityMap[F->getName().str()]);
  }
  return rights;
}

std::vector<uint64_t> Capsicum::rightsForStrings(StringSet& SS) {
  std::vector<uint64_t> rights;
  for (std::string s : SS) {
    rights.push_back(capabilityMap[s]);
  }
  return rights;
}
