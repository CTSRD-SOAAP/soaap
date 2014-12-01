/*
 * RUN: clang %cflags -gsoaap -emit-llvm -S %s -o %t.ll
 * RUN: soaap --soaap-list-fp-targets %t.ll | c++filt > %t.out
 * RUN: FileCheck %s -input-file %t.out
 */
// defining type as a suboject of virtual base 
class A {
  public:
    int a;
    virtual void v() { }
};

class B : public A {
  public:
    int b;
    virtual void w() { }
};

class C : public virtual B {
  public:
    int c;
    virtual void y() { }
};

class D : public C {
  public:
    virtual void z() { }
};

int main(int argc, char** argv) {
  D* d = new D;
  // CHECK: Call at {{.*}}:[[@LINE+3]]
  // CHECK-NEXT: Targets:
  // CHECK-NEXT: A::v()
  d->v();
  return 0;
}
