/*
 * Copyright (c) 2017 Gabriela Sklencarova
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

#ifndef _SOAAP_GEN_H_
#define _SOAAP_GEN_H_

#include <stdbool.h>

/* Request types. */
#define TERMINATE_SANDBOX 0x0
#define FUNCTION_CALL 0x1
#define METHOD_INVOCATION 0x2
#define RETURN 0x3
#define STRUCT_GETTER 0x4
#define STRUCT_SETTER 0x5
#define NUM_REQUEST_TYPES 0x6

/* Nvlist field names. */
#define MESSAGE_TYPE "msg_type"
#define FUNCTION_CODE "func_code"
#define SBOX_ARG "_sbox_arg"
#define SBOX_GLOBAL "_sbox_glob"
#define SBOX_ERROR "err"
#define RETURN_VALUE "ret_val"
#define CONS_CODE "cons_code"
#define HANDLE_CODE "handle_code"
#define PATH_LEN "path_len"
#define STRUCT_PATH "struct_path"
#define SETTER_VALUE "setter_value"

/* Function argument annotations. */
#define ACCESS_IN "IN"
#define ACCESS_INOUT "INOUT"
#define ACCESS_OUT "OUT"

#define TYPE_PTR "PTR"
#define TYPE_Z "Z"
#define TYPE_BUFF "BUFF"

#define OPT "OPT"

#define IN_PTR "IN_PTR"
#define IN_PTR_OPT "IN_PTR_OPT"
#define IN_Z "IN_Z"
#define IN_BUFF "IN_BUFF"
#define IN_BUFF_OPT "IN_BUFF_OPT"

#define INOUT_PTR "INOUT_PTR"
#define INOUT_PTR_OPT "INOUT_PTR_OPT"

#define OUT_PTR_OPT "OUT_PTR_OPT"
#define OUT_BUFF "OUT_BUFF"
#define OUT_BUFF_OPT "OUT_BUFF_OPT"

#define RETURN_HANDLE "RETURN_HANDLE"

#define CONSTRUCTOR "CONSTRUCTOR"
#define TRANSITIVE "TRANSITIVE"
#define HANDLE "HANDLE"
#define SANDBOX_INTERNAL "SANDBOX_INTERNAL"

#ifndef IN_SOAAP_GENERATOR

#define __soaap_in __attribute__((annotate(IN_PTR)))
#define __soaap_in_opt __attribute__((annotate(IN_PTR_OPT)))
#define __soaap_in_z __attribute__((annotate(IN_Z)))
#define __soaap_in_reads(N) __attribute__((annotate(IN_BUFF "_" N)))
#define __soaap_in_reads_opt(N) __attribute__((annotate(IN_BUFF_OPT "_" N)))

#define __soaap_inout __attribute__((annotate(INOUT_PTR)))
#define __soaap_inout_opt __attribute__((annotate(INOUT_PTR_OPT)))

#define __soaap_out __attribute__((annotate(OUT_PTR)))
#define __soaap_out_opt __attribute__((annotate(OUT_PTR_OPT)))
#define __soaap_out_writes(N) __attribute__((annotate(OUT_BUFF "_" N)))
#define __soaap_out_writes_opt(N) __attribute__((annotate(OUT_BUFF_OPT "_" N)))

#define __soaap_return_handle __attribute__((annotate(RETURN_HANDLE)))

#define __soaap_constructor(T) __attribute__((annotate(CONSTRUCTOR "_" T)))
#define __soaap_internal(N) __attribute__((annotate(SANDBOX_INTERNAL "_" N)))

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <sys/nv.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

#define DEBUG
// #define PROF

/* DPRINTF */
#ifdef DEBUG
#define DPRINTF(format, ...)        \
    fprintf(stderr, "%s [%d] " format "\n",   \
        __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define DPRINTF(...)
#endif

#define PRINTF(format, ...)       \
    fprintf(stderr, "%s [%d] " format "\n",   \
        __FUNCTION__, __LINE__, ##__VA_ARGS__)

// #define UDSOCKETS

#define MAGIC 0xcafebabe
#define OP_SENDBACK 0x0001
#define OP_SENDRECEIVE 0x0010

#ifndef PAGE_SIZE
#define PAGE_SIZE _SC_PAGE_SIZE
#endif

#define SOAAP_BUF_LEN PAGE_SIZE

char soaap_buf[SOAAP_BUF_LEN];
char soaap_tmpbuf[SOAAP_BUF_LEN];

// bool in_sandbox = false;

__attribute__((used)) static nvlist_t*
soaap_gen_invoke_method(nvlist_t* nvl) {
  nvlist_t* ret = nvlist_create(0);
  return ret;
}

__attribute__((used)) static int
soaap_gen_create_sbox(char* pathname)
{
  DPRINTF("Creating a persistent sandbox.");

  pid_t sbox_pid;
  int pfds[2];

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, pfds) == -1) {
    perror("socketpair");
    exit(1);
  }

  sbox_pid = fork();
  if (sbox_pid == -1) {
    perror("fork");
    return 0;
  }

  char str_fds[2];
  str_fds[0] = pfds[0];
  str_fds[1] = pfds[1];

  /*
   * Child proces.
   */
  if (!sbox_pid) {
    DPRINTF("In child process, calling exec with argument %s.", str_fds);
    /*
     * After this call, the main for the child should take over.
     */
    execl(pathname, pathname, str_fds, NULL);
  }

  DPRINTF("In parent process, returning socket %d.", pfds[1]);
  return pfds[1];
}

/**
 * Send a terminate request to the sandbox with file descriptor fd.
 */
__attribute__((used)) static void
soaap_gen_terminate_sbox(int fd) {
  DPRINTF("[TERMINATE SBOX] Sending a close signal to a sandbox with fd %d.", fd);
  DPRINTF("[TERMINATE SBOX] Sending request over RPC.");
  nvlist_t* req = nvlist_create(0);
  nvlist_add_number(req, MESSAGE_TYPE, TERMINATE_SANDBOX);
  if (nvlist_send(fd, req) < 0) {
    perror("[TERMINATE SBOX] nvlist_send");
    exit(1);
  }
  nvlist_destroy(req);
  DPRINTF("[TERMINATE SBOX] Request sent. Not waiting for a response.");
}

__attribute__((used)) static nvlist_t*
soaap_gen_enter_sbox(int fd, nvlist_t* req) {
  nvlist_t* res;
  bool recv_err;

  DPRINTF("Sending request over RPC to fd %d.", fd);
  nvlist_fdump(req, stdout);
  res = nvlist_xfer(fd, req, 0);
  if (res == NULL)
    err(1, "nvlist_xfer");

  DPRINTF("Received a response.");
  nvlist_fdump(res, stdout);
  recv_err = nvlist_get_bool(res, SBOX_ERROR);
  if (recv_err)
    err(1, "error");

  return res;
}

__attribute__((used)) static void
soaap_gen_add_function_call_data(nvlist_t* req, uint64_t func) {
  nvlist_add_number(req, MESSAGE_TYPE, FUNCTION_CALL);
  nvlist_add_number(req, FUNCTION_CODE, func);
}

__attribute__((used)) static void
soaap_gen_add_method_return_data(nvlist_t* req) {
  nvlist_add_number(req, MESSAGE_TYPE, RETURN);
}

__attribute__((used)) static nvlist_t*
soaap_gen_call_sbox_function(int fd, nvlist_t* req, uint64_t func) {
  nvlist_add_number(req, MESSAGE_TYPE, FUNCTION_CALL);
  nvlist_add_number(req, FUNCTION_CODE, func);
  return soaap_gen_enter_sbox(fd, req);
}

__attribute__((used)) static nvlist_t*
soaap_gen_method_return(int fd, nvlist_t* req) {
  nvlist_add_number(req, MESSAGE_TYPE, RETURN);
  return soaap_gen_enter_sbox(fd, req);
}

#endif  /* IN_SOAAP_GENERATOR */

#endif  /* _SOAAP_GEN_H_ */
