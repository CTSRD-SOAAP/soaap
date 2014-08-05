/*
 * RUN: clang++ %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
#include "soaap.h"
#include <fcntl.h>
#include <unistd.h>
#include <map>

using namespace std;

enum {
  kPrimaryIPCChannel = 12,
  kStatsTableSharedMemFd,
  kIPCDescriptorMax
};

void foo();

class Map {
  public:
    __soaap_fd_getter
    int Get(int key) { return store[key]; }
    __soaap_fd_setter
    void Set(int key, int val) { store[key] = val; }
    map<int,int> store;
};

int main(int argc, char** argv) {
  __soaap_create_persistent_sandbox("sandbox");
  foo();
  return 0;
}

__soaap_sandbox_persistent("sandbox")
void foo() {
  __soaap_limit_syscalls(read, write);
  __soaap_limit_fd_key_syscalls(kPrimaryIPCChannel, read);
  Map* m;
  m->Set(kPrimaryIPCChannel, 14);
  int fd = m->Get(kPrimaryIPCChannel);
  // CHECK-DAG: *** Sandbox "sandbox" performs system call "write" but is not allowed to for the given fd arg.
  // CHECK-DAG: +++ Line 47 of file {{.*}}
  write(fd, NULL, 0);
  // CHECK-NOT: *** Sandbox "sandbox" performs system call "read" but is not allowed to for the given fd arg.
  // CHECK-NOT: +++ Line 50 of file {{.*}}
  read(fd, NULL, 0);
}
