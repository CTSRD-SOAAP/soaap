/*
 * RUN: clang %cflags -gsoaap -emit-llvm -S %s -o %t.ll
 * RUN: FileCheck %s -input-file %t.ll
 */

class C {
  public:
    virtual void foo() { }
};

int main() {
  C* c = new C;
  c->foo();
  // CHECK: call void %{{[0-9]+}}(%class.C* %{{[0-9]+}}), !dbg !{{[0-9]+}}, !soaap_defining_vtable_name !{{[0-9]+}}, !soaap_static_vtable_name !{{[0-9]+}}
  delete c;
  return 0;
}
