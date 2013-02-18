#include "soaap.h"
#include <string.h>
#include <stdlib.h>

void dostuff();

int main() {
  dostuff();
  printf("value of secret sandbox-private password: %s\n", getenv("PUBLIC"));
  return 0;
}

__soaap_sandbox_persistent_named("box")
void dostuff() {
  char* password __soaap_sandbox_private("box");
  password = "mypass";
  printf("leaking sandbox-private password to environment variable PUBLIC\n");
  setenv("PUBLIC", password, 1);
}

