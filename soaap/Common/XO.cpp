#include "Common/XO.h"

#include <cstdarg>

using namespace soaap;

list<xo_handle_t*> XO::handles;

void XO::create(int style, int flags) {
  handles.push_back(xo_create(style, flags));
}

void XO::create_to_file(FILE* fp, int style, int flags) {
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
    xo_emit_hv(handle, fmt, args);
  }
  va_end(args);
}
