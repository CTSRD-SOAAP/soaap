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
      static void flush();
      static void finish();

      static void open_container(const char* name);
      static void close_container(const char* name);
      
      static void open_list(const char* name);
      static void close_list(const char* name);
      static void open_instance(const char* name);
      static void close_instance(const char* name);
      
      static void emit(const char* fmt, ...);

    private:
      static list<xo_handle_t*> handles;
      
  };
}

#endif
