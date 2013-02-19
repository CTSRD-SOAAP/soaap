#include "soaap.h"

void dostuff();
void privfunc(int p1);

__soaap_callgates(privfunc);

int main() {
  dostuff();
  return 0;
}

__soaap_sandbox_persistent_named("box")
void dostuff() {
  int key __soaap_sandbox_private("box");
  privfunc(key);
}

void privfunc(int p1) {
  
}
