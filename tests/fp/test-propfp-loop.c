#include <stdio.h>
#include "soaap.h"

int read(int,char*,int);

__sandbox_persistent
void f(int __fd_read ifd) {
  int i=0;
  int j=ifd;
  char buf[10];
  while (i < 10) {
    ifd = j+1;
    j = ifd;
    read(j, buf, 10);
    i++;
  }
}

int main(int argc, char** argv) {
  f(1);
}
