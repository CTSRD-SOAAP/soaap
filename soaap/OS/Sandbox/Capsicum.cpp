#include "OS/Sandbox/Capsicum.h"

using namespace soaap;

Capsicum::Capsicum() {
  addPermittedSysCall("cap_enter");
  addPermittedSysCall("cap_fcntls_get");
  addPermittedSysCall("cap_fcntls_limit");
  addPermittedSysCall("cap_getmode");
  addPermittedSysCall("cap_ioctls_get");
  addPermittedSysCall("cap_ioctls_limit");
  addPermittedSysCall("__cap_rights_get");
  addPermittedSysCall("cap_rights_limit");
  addPermittedSysCall("dup");
  addPermittedSysCall("dup2");
  addPermittedSysCall("close");
  addPermittedSysCall("exit");
  addPermittedSysCall("read", true);
  addPermittedSysCall("write", true);
  
  //TODO: add the system calls below (taken from FreeBSD/sys/kern/capabilities.conf)
  ////
  // Allow ACL and MAC label operations by file descriptor, subject to
  // capability rights.  Allow MAC label operations on the current process but
  // we will need to scope __mac_get_pid(2).
  //
  //__acl_aclcheck_fd
  //__acl_delete_fd
  //__acl_get_fd
  //__acl_set_fd
  //__mac_get_fd
  //__mac_get_proc
  //__mac_set_fd
  //__mac_set_proc

  ////
  //// Allow sysctl(2) as we scope internal to the call; this is a global
  //// namespace, but there are several critical sysctls required for almost
  //// anything to run, such as hw.pagesize.  For now that policy lives in the
  //// kernel for performance and simplicity, but perhaps it could move to a
  //// proxying daemon in userspace.
  ////
  //__sysctl

  ////
  //// Allow umtx operations as these are scoped by address space.
  ////
  //// XXRW: Need to check this very carefully.
  ////
  //_umtx_lock
  //_umtx_op
  //_umtx_unlock

  ////
  //// Allow process termination using abort2(2).
  ////
  //abort2

  ////
  //// Allow accept(2) since it doesn't manipulate namespaces directly, rather
  //// relies on existing bindings on a socket, subject to capability rights.
  ////
  //accept
  //accept4

  ////
  //// Allow AIO operations by file descriptor, subject to capability rights.
  ////
  //aio_cancel
  //aio_error
  //aio_fsync
  //aio_read
  //aio_return
  //aio_suspend
  //aio_waitcomplete
  //aio_write

  ////
  //// Allow bindat(2).
  ////
  //bindat

  ////
  //// Allow capability mode and capability system calls.
  ////
  //cap_enter
  //cap_fcntls_get
  //cap_fcntls_limit
  //cap_getmode
  //cap_ioctls_get
  //cap_ioctls_limit
  //__cap_rights_get
  //cap_rights_limit

  ////
  //// Allow read-only clock operations.
  ////
  //clock_getres
  //clock_gettime

  ////
  //// Always allow file descriptor close(2).
  ////
  //close
  //closefrom

  ////
  //// Allow connectat(2).
  ////
  //connectat

  ////
  //// Always allow dup(2) and dup2(2) manipulation of the file descriptor table.
  ////
  //dup
  //dup2

  ////
  //// Allow extended attribute operations by file descriptor, subject to
  //// capability rights.
  ////
  //extattr_delete_fd
  //extattr_get_fd
  //extattr_list_fd
  //extattr_set_fd

  ////
  //// Allow changing file flags, mode, and owner by file descriptor, subject to
  //// capability rights.
  ////
  //fchflags
  //fchmod
  //fchown

  ////
  //// For now, allow fcntl(2), subject to capability rights, but this probably
  //// needs additional scoping.
  ////
  //fcntl

  ////
  //// Allow fexecve(2), subject to capability rights.  We perform some scoping,
  //// such as disallowing privilege escalation.
  ////
  //fexecve

  ////
  //// Allow flock(2), subject to capability rights.
  ////
  //flock

  ////
  //// Allow fork(2), even though it returns pids -- some applications seem to
  //// prefer this interface.
  ////
  //fork

  ////
  //// Allow fpathconf(2), subject to capability rights.
  ////
  //fpathconf

  ////
  //// Allow various file descriptor-based I/O operations, subject to capability
  //// rights.
  ////
  //freebsd6_ftruncate
  //freebsd6_lseek
  //freebsd6_mmap
  //freebsd6_pread
  //freebsd6_pwrite

  ////
  //// Allow querying file and file system state with fstat(2) and fstatfs(2),
  //// subject to capability rights.
  ////
  //fstat
  //fstatfs

  ////
  //// Allow further file descriptor-based I/O operations, subject to capability
  //// rights.
  ////
  //fsync
  //ftruncate

  ////
  //// Allow futimes(2), subject to capability rights.
  ////
  //futimes

  ////
  //// Allow querying process audit state, subject to normal access control.
  ////
  //getaudit
  //getaudit_addr
  //getauid

  ////
  //// Allow thread context management with getcontext(2).
  ////
  //getcontext

  ////
  //// Allow directory I/O on a file descriptor, subject to capability rights.
  //// Originally we had separate capabilities for directory-specific read
  //// operations, but on BSD we allow reading the raw directory data, so we just
  //// rely on CAP_READ now.
  ////
  //getdents
  //getdirentries

  ////
  //// Allow querying certain trivial global state.
  ////
  //getdomainname

  ////
  //// Allow querying current process credential state.
  ////
  //getegid
  //geteuid

  ////
  //// Allow querying certain trivial global state.
  ////
  //gethostid
  //gethostname

  ////
  //// Allow querying per-process timer.
  ////
  //getitimer

  ////
  //// Allow querying current process credential state.
  ////
  //getgid
  //getgroups
  //getlogin

  ////
  //// Allow querying certain trivial global state.
  ////
  //getpagesize
  //getpeername

  ////
  //// Allow querying certain per-process scheduling, resource limit, and
  //// credential state.
  ////
  //// XXXRW: getpgid(2) needs scoping.  It's not clear if it's worth scoping
  //// getppid(2).  getpriority(2) needs scoping.  getrusage(2) needs scoping.
  //// getsid(2) needs scoping.
  ////
  //getpgid
  //getpgrp
  //getpid
  //getppid
  //getpriority
  //getresgid
  //getresuid
  //getrlimit
  //getrusage
  //getsid

  ////
  //// Allow querying socket state, subject to capability rights.
  ////
  //// XXXRW: getsockopt(2) may need more attention.
  ////
  //getsockname
  //getsockopt

  ////
  //// Allow querying the global clock.
  ////
  //gettimeofday

  ////
  //// Allow querying current process credential state.
  ////
  //getuid

  ////
  //// Allow ioctl(2), which hopefully will be limited by applications only to
  //// required commands with cap_ioctls_limit(2) syscall.
  ////
  //ioctl

  ////
  //// Allow querying current process credential state.
  ////
  //issetugid

  ////
  //// Allow kevent(2), as we will authorize based on capability rights on the
  //// target descriptor.
  ////
  //kevent

  ////
  //// Allow kill(2), as we allow the process to send signals only to himself.
  ////
  //kill

  ////
  //// Allow message queue operations on file descriptors, subject to capability
  //// rights.
  ////
  //kmq_notify
  //kmq_setattr
  //kmq_timedreceive
  //kmq_timedsend

  ////
  //// Allow kqueue(2), we will control use.
  ////
  //kqueue

  ////
  //// Allow managing per-process timers.
  ////
  //ktimer_create
  //ktimer_delete
  //ktimer_getoverrun
  //ktimer_gettime
  //ktimer_settime

  ////
  //// Allow AIO operations by file descriptor, subject to capability rights.
  ////
  //lio_listio

  ////
  //// Allow listen(2), subject to capability rights.
  ////
  //// XXXRW: One might argue this manipulates a global namespace.
  ////
  //listen

  ////
  //// Allow I/O-related file descriptors, subject to capability rights.
  ////
  //lseek

  ////
  //// Allow simple VM operations on the current process.
  ////
  //madvise
  //mincore
  //minherit
  //mlock
  //mlockall

  ////
  //// Allow memory mapping a file descriptor, and updating protections, subject
  //// to capability rights.
  ////
  //mmap
  //mprotect

  ////
  //// Allow simple VM operations on the current process.
  ////
  //msync
  //munlock
  //munlockall
  //munmap

  ////
  //// Allow the current process to sleep.
  ////
  //nanosleep

  ////
  //// Allow querying the global clock.
  ////
  //ntp_gettime

  ////
  //// Allow AIO operations by file descriptor, subject to capability rights.
  ////
  //oaio_read
  //oaio_write

  ////
  //// Allow simple VM operations on the current process.
  ////
  //obreak

  ////
  //// Allow AIO operations by file descriptor, subject to capability rights.
  ////
  //olio_listio

  ////
  //// Operations relative to directory capabilities.
  ////
  //chflagsat
  //faccessat
  //fchmodat
  //fchownat
  //fstatat
  //futimesat
  //linkat
  //mkdirat
  //mkfifoat
  //mknodat
  //openat
  //readlinkat
  //renameat
  //symlinkat
  //unlinkat

  ////
  //// Allow poll(2), which will be scoped by capability rights.
  ////
  //// XXXRW: Perhaps we don't need the OpenBSD version?
  //// XXXRW: We don't yet do that scoping.
  ////
  //openbsd_poll

  ////
  //// Process descriptor-related system calls are allowed.
  ////
  //pdfork
  //pdgetpid
  //pdkill

  ////
  //// Allow pipe(2).
  ////
  //pipe
  //pipe2

  ////
  //// Allow poll(2), which will be scoped by capability rights.
  //// XXXRW: We don't yet do that scoping.
  ////
  //poll

  ////
  //// Allow I/O-related file descriptors, subject to capability rights.
  ////
  //pread
  //preadv

  ////
  //// Allow access to profiling state on the current process.
  ////
  //profil

  ////
  //// Allow I/O-related file descriptors, subject to capability rights.
  ////
  //pwrite
  //pwritev
  //read
  //readv
  //recv
  //recvfrom
  //recvmsg

  ////
  //// Allow real-time scheduling primitives to be used.
  ////
  //// XXXRW: These require scoping.
  ////
  //rtprio
  //rtprio_thread

  ////
  //// Allow simple VM operations on the current process.
  ////
  //sbrk

  ////
  //// Allow querying trivial global scheduler state.
  ////
  //sched_get_priority_max
  //sched_get_priority_min

  ////
  //// Allow various thread/process scheduler operations.
  ////
  //// XXXRW: Some of these require further scoping.
  ////
  //sched_getparam
  //sched_getscheduler
  //sched_rr_get_interval
  //sched_setparam
  //sched_setscheduler
  //sched_yield

  ////
  //// Allow I/O-related file descriptors, subject to capability rights.
  ////
  //sctp_generic_recvmsg
  //sctp_generic_sendmsg
  //sctp_generic_sendmsg_iov
  //sctp_peeloff

  ////
  //// Allow pselect(2) and select(2), which will be scoped by capability rights.
  ////
  //// XXXRW: But is it?
  ////
  //pselect
  //select

  ////
  //// Allow I/O-related file descriptors, subject to capability rights.  Use of
  //// explicit addresses here is restricted by the system calls themselves.
  ////
  //send
  //sendfile
  //sendmsg
  //sendto

  ////
  //// Allow setting per-process audit state, which is controlled separately by
  //// privileges.
  ////
  //setaudit
  //setaudit_addr
  //setauid

  ////
  //// Allow setting thread context.
  ////
  //setcontext

  ////
  //// Allow setting current process credential state, which is controlled
  //// separately by privilege.
  ////
  //setegid
  //seteuid
  //setgid

  ////
  //// Allow use of the process interval timer.
  ////
  //setitimer

  ////
  //// Allow setpriority(2).
  ////
  //// XXXRW: Requires scoping.
  ////
  //setpriority

  ////
  //// Allow setting current process credential state, which is controlled
  //// separately by privilege.
  ////
  //setregid
  //setresgid
  //setresuid
  //setreuid

  ////
  //// Allow setting process resource limits with setrlimit(2).
  ////
  //setrlimit

  ////
  //// Allow creating a new session with setsid(2).
  ////
  //setsid

  ////
  //// Allow setting socket options with setsockopt(2), subject to capability
  //// rights.
  ////
  //// XXXRW: Might require scoping.
  ////
  //setsockopt

  ////
  //// Allow setting current process credential state, which is controlled
  //// separately by privilege.
  ////
  //setuid

  ////
  //// shm_open(2) is scoped so as to allow only access to new anonymous objects.
  ////
  //shm_open

  ////
  //// Allow I/O-related file descriptors, subject to capability rights.
  ////
  //shutdown

  ////
  //// Allow signal control on current process.
  ////
  //sigaction
  //sigaltstack
  //sigblock
  //sigpending
  //sigprocmask
  //sigqueue
  //sigreturn
  //sigsetmask
  //sigstack
  //sigsuspend
  //sigtimedwait
  //sigvec
  //sigwaitinfo
  //sigwait

  ////
  //// Allow creating new socket pairs with socket(2) and socketpair(2).
  ////
  //socket
  //socketpair

  ////
  //// Allow simple VM operations on the current process.
  ////
  //// XXXRW: Kernel doesn't implement this, so drop?
  ////
  //sstk

  ////
  //// Do allow sync(2) for now, but possibly shouldn't.
  ////
  //sync

  ////
  //// Always allow process termination with sys_exit(2).
  ////
  //sys_exit

  ////
  //// sysarch(2) does rather diverse things, but is required on at least i386
  //// in order to configure per-thread data.  As such, it's scoped on each
  //// architecture.
  ////
  //sysarch

  ////
  //// Allow thread operations operating only on current process.
  ////
  //thr_create
  //thr_exit
  //thr_kill

  ////
  //// Allow thread operations operating only on current process.
  ////
  //thr_new
  //thr_self
  //thr_set_name
  //thr_suspend
  //thr_wake

  ////
  //// Allow manipulation of the current process umask with umask(2).
  ////
  //umask

  ////
  //// Allow submitting of process trace entries with utrace(2).
  ////
  //utrace

  ////
  //// Allow generating UUIDs with uuidgen(2).
  ////
  //uuidgen

  ////
  //// Allow I/O-related file descriptors, subject to capability rights.
  ////
  //write
  //writev

  ////
  //// Allow processes to yield(2).
  ////
  //yield
}
