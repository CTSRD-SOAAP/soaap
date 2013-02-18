#include "soaap.h"
#include <string.h>

int sensitive __soaap_sandbox_private("network") = 25;

void dostuff();
void domorestuff();

int main() {
  int a = sensitive;
  dostuff();
  domorestuff();
  return 0;
}

__soaap_sandbox_persistent_named("network")
void dostuff() {
  int y = sensitive;
  printf("secret y is: %d\n", y);
}

__soaap_sandbox_persistent_named("box2")
void domorestuff() {
  int z = sensitive;
  printf("secret is: %d\n", z);
}
