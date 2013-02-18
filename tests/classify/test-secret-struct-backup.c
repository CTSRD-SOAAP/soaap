#include "soaap.h"
#include <string.h>

struct cred {
  char user[10];
  char pass[10] __soaap_classify("secret");
}

void doauth(struct cred* c);

int main() {
  struct cred c;
  doauth(&c);
  return 0;
}

__soaap_sandbox_persistent
__soaap_clearance("secret")
void doauth(struct cred* c) {
  strcpy(c->user, "me");
}
