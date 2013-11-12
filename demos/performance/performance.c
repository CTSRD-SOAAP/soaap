#include "soaap.h"
#include "soaap_perf.h"

void accept_connection();

int main(int argc, char** argv) {
  __soaap_create_persistent_sandbox("session");
  int i;
  for (i = 0; i < 1000; i++) {
    accept_connection();
  }
}

__soaap_sandbox_persistent("session")
__soaap_overhead(20)
void accept_connection() {
  static int id = 1;
  int i;
  for (i = 0; i < 100; i++) {
    printf("[%d] Hello\n", id);
  }
  id++;
}

