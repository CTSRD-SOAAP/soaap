/*
 * RUN: clang %cflags -gsoaap -emit-llvm -S %s -o %t.ll
 * RUN: FileCheck %s -input-file %t.ll
 */

class C {
  public:
    virtual void foo() { }
};

int main() {
  C c;
  c.foo();
  // CHECK-NOT: !soaap_defining_vtable_var
  // CHECK-NOT: !soaap_defining_vtable_name
  return 0;
}
