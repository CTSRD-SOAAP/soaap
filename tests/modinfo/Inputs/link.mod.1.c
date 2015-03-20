extern void b();

int a() {
  b();
  return 1;
}

int main(int argc, char** argv) {
  a();
}
