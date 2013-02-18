#include <stdio.h>
#include "soaap.h"

int read(int,char*,int);

void g(int i) {
  //printf("i: %d\n", i);
  char buf[10];
  read(i,buf,10);
}

__sandbox_persistent
void f(int __fd_read ifd) {
  g(ifd);
}

int main(int argc, char** argv) {
  int i = 1;
  //int j = i;
  f(i);
}
