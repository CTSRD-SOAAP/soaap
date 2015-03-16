/*
 * RUN: clang %cflags -emit-llvm -S %s -o %t.ll
 * RUN: soaap --soaap-infer-fp-targets --soaap-list-fp-targets -o %t.soaap.ll %t.ll > %t.out
 * RUN: FileCheck %s -input-file %t.out
 *
 * CHECK: Running Soaap Pass
 */
typedef long ssize_t;
typedef unsigned long size_t;

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);

void call_read(ssize_t (*f)(int, void*, size_t), int fd, void *buf, size_t count) {
  // CHECK: Function "call_read"
  // CHECK-NEXT:   Call at {{.*}}:21
  // CHECK-NEXT:     Targets:
  // CHECK-NEXT:       [<privileged>]:
  // CHECK-NEXT:         read (inferred)
  // CHECK-NOT:          write (inferred)
  f(fd, buf, count);
}

void call_write(ssize_t (*f)(int, const void*, size_t), int fd, const void *buf, size_t count) {
  // CHECK: Function "call_write"
  // CHECK-NEXT:   Call at {{.*}}:31
  // CHECK-NEXT:     Targets:
  // CHECK-NEXT:       [<privileged>]:
  // CHECK-NEXT:          write (inferred)
  // CHECK-NOT:           read (inferred)
  f(fd, buf, count);
}

void call_either(ssize_t (*f)(), int fd, void *buf, size_t count) {
  // CHECK: Function "call_either"
  // CHECK-NEXT:   Call at {{.*}}:41
  // CHECK-NEXT:     Targets:
  // CHECK-NEXT:       [<privileged>]:
  // CHECK-NEXT:          read (inferred)
  // CHECK-NEXT:          write (inferred)
  f(fd, buf, count);
}

int main(int argc, char** argv) {
  char buf[10];
  call_read(read, 0, buf, sizeof(buf));
  call_write(write, 1, buf, sizeof(buf));
  call_either(read, 0, buf, sizeof(buf));
  call_either(write, 1, buf, sizeof(buf));
}
