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

#include "Common/XO.h"

#include "Common/Debug.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdarg>

using namespace llvm;
using namespace soaap;

list<xo_handle_t*> XO::handles;

// use llvm's output stream for stdout to get consistent buffering behaviour
ssize_t llvm_write(void* opaque, const char* str) {
  outs() << str;
  return 0;
}

void XO::create(int style, int flags) {
  SDEBUG("soaap.xo", 3, dbgs() << "Creating non-file handle with style "
                               << style << " and flags " << flags << "\n");
  xo_handle_t* handle = xo_create(style, flags);
  handles.push_back(handle);
  // use llvm's output stream for stdout
  xo_set_writer(handle, NULL, llvm_write, NULL, NULL);
}

void XO::create_to_file(FILE* fp, int style, int flags) {
  SDEBUG("soaap.xo", 3, dbgs() << "Creating file handle with style "
                               << style << " and flags " << flags << "\n");
  handles.push_back(xo_create_to_file(fp, style, flags));
}

void XO::finish() {
  for (xo_handle_t* handle : handles) {
    xo_finish_h(handle);
  }
}

void XO::open_container(const char* name) {
  for (xo_handle_t* handle : handles) {
    xo_open_container_h(handle, name);
  }
}

void XO::close_container(const char* name) {
  for (xo_handle_t* handle : handles) {
    xo_close_container_h(handle, name);
  }
}

void XO::open_list(const char* name) {
  for (xo_handle_t* handle : handles) {
    xo_open_list_h(handle, name);
  }
}

void XO::close_list(const char* name) {
  for (xo_handle_t* handle : handles) {
    xo_close_list_h(handle, name);
  }
}

void XO::open_instance(const char* name) {
  for (xo_handle_t* handle : handles) {
    xo_open_instance_h(handle, name);
  }
}

void XO::close_instance(const char* name) {
  for (xo_handle_t* handle : handles) {
    xo_close_instance_h(handle, name);
  }
}

void XO::emit(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  for (xo_handle_t* handle : handles) {
    SDEBUG("soaap.xo", 3, dbgs() << "Emitting format string to handle\n");
    xo_emit_hv(handle, fmt, args);
  }
  va_end(args);
}
