#include "soaap.h"
#include <unistd.h>

void remote_auth_user(int sock);
int check_login(char* user, char* pwd);

//__soaap_callgates(authenticate, check_login);

int main(int argc, char** argv) {
  __soaap_create_persistent_sandbox("authenticate");
  remote_auth_user(argc);
}

__soaap_privileged
int check_login(char* user, char* pwd) {
  // Access /etc/passwd and check user/pwd
  return 0;
}

__soaap_sandbox_persistent("authenticate")
void remote_auth_user(__soaap_fd_read int sock) {
  char user[50], pwd[50];
  read(sock, user, 50);
  read(sock, pwd, 50);
  if (!check_login(user, pwd)) {
    // retry...
  }
}
