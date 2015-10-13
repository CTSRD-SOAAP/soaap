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

#ifndef SOAAP_ADT_QUEUESET_H
#define SOAAP_ADT_QUEUESET_H

#include <set>
#include <list>

namespace soaap {

  template<typename T>
  class QueueSet {
    public:
      bool enqueue(T elem);
      T dequeue();
      bool empty();
      int size();
      void clear();

    protected:
      std::set<T> set;
      std::list<T> list;
  };

  template<typename T>
  bool QueueSet<T>::enqueue(T elem) {
    if (set.insert(elem).second) {
      list.push_back(elem);
      return true;
    }
    return false;
  }

  template<typename T>
  T QueueSet<T>::dequeue() {
    T elem = list.front();
    list.pop_front();
    set.erase(elem);
    return elem;
  }

  template<typename T>
  bool QueueSet<T>::empty() {
    return list.empty();
  }

  template<typename T>
  int QueueSet<T>::size() {
    return list.size();
  }

  template<typename T>
  void QueueSet<T>::clear() {
    list.clear();
    set.clear();
  }

}

#endif
