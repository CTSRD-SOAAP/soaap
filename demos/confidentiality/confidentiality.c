#include "soaap.h"

void accept_connection();
extern void compute_session_key(char*, char*, i);
void foo();

__soaap_classify("secret") char* server_key;

struct {
  __soaap_private("something_else") char* key;
} sensitive;

int main(int argc, char** argv) {
  while (argc-- > 0) {
    accept_connection();
  }
  foo();
}

__soaap_sandbox_persistent("something_else")
void foo() { 
  printf("hello\n");
}

__soaap_sandbox_persistent("session")
void accept_connection() {
  __soaap_private("session") char session_key[1024];
  int i = sensitive.key;
  compute_session_key(session_key, server_key, i);
}

