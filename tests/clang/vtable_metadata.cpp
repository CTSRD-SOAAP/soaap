/*
 * RUN: clang %cflags -gsoaap -emit-llvm -S %s -o %t.ll
 * RUN: FileCheck %s -input-file %t.ll
 */
using namespace std;

class C {
  public:
    virtual void foo() { }
};

int main() {
  C* c = new C;
  c->foo();
  // CHECK: call void %3(%class.C* %1), !dbg !33, !soaap_defining_vtable_var !34, !soaap_static_vtable_var !34
  delete c;
  return 0;
}
