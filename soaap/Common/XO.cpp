#include "Common/XO.h"

#include "Common/Debug.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdarg>

using namespace llvm;
using namespace soaap;

list<xo_handle_t*> XO::handles;

// use llvm's output stream for stdout to get consistent buffering behaviour
int llvm_write(void* opaque, const char* str) {
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
