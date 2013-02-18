#include "soaap.h"
#include <string.h>

int x;

void dostuff();

int main() {
  dostuff();
  printf("value of secret sandbox-private local variable: %d\n", x);
  return 0;
}

__soaap_sandbox_persistent_named("network")
void dostuff() {
  int y __soaap_sandbox_private("network");
  y = 25;
  printf("leaking sandbox-private y to global x\n");
  x = y;
}

