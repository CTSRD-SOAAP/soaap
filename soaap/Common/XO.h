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

#ifndef SOAAP_COMMON_XO_H
#define SOAAP_COMMON_XO_H

#include <cstdio>
#include <cassert>

extern "C" {
#include <libxo/xo.h>
}

#include <list>

using namespace std;

namespace soaap {
  class XO {
    public:
      static void create(int style, int flags);
      static void create_to_file(FILE* fp, int style, int flags);
      static void finish();

      static void emit(const char* fmt, ...);

    private:
      static void open_container(const char* name);
      static void close_container(const char* name);
      static void open_list(const char* name);
      static void close_list(const char* name);
      static void open_instance(const char* name);
      static void close_instance(const char* name);
    public:
      // RAII classes for XO::[open/close]_[container/list/instance]()
      template<void(*openFunc)(const char*), void(*closeFunc)(const char*)>
      class RAII {
        public:
          explicit RAII(const char* name) : name(name) { openFunc(name); }
          void close() {
            if (name) {
              closeFunc(name);
              name = nullptr;
            }
          }
          ~RAII() { close(); }
          const char* name;
      };
      typedef RAII<open_list, close_list> List;
      typedef RAII<open_container, close_container> Container;

      /** Instances should be created by referring to the outer element and not from a constant string */
      class Instance : public RAII<open_instance, close_instance> {
        public:
          Instance(const List& list) : RAII(list.name) { assert(list.name && "Creating element for already closed list!"); }
      };
    private:
      static list<xo_handle_t*> handles;
  };
}

#endif
