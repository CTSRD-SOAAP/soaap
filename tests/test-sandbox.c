#include <stdio.h>
#include <unistd.h>
#include "soaap.h"

int x __var_read;
void f();

int main() {
  f(3,4);
  return 0;
}

__sandbox_persistent
void f(int __fd_read ifd, int ofd) {
  printf("hello from sandbox\n");
  char buf[10];
  x = 3;
  read(ifd, buf, 1);
  write(ofd, buf, 1);
}
