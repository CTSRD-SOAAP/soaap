#ifndef SOAAP_COMMON_XO_H
#define SOAAP_COMMON_XO_H

#include <cstdio>

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
          Instance(const List& list) : RAII(list.name) {}
      };
    private:
      static list<xo_handle_t*> handles;
  };
}

#endif
