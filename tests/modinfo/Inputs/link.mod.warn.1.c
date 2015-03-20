#include "soaap.h"

extern void bar();

int x = 0;

__soaap_sandbox_persistent("auth")
void foo() {
  bar();
  x = 3;
}

int main(int argc, char** argv) {
  foo();
  return 0;
}
