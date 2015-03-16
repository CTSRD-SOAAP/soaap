/*
 * RUN: clang++ %cflags -gsoaap -emit-llvm -c %s -o %t.ll
 * RUN: soaap --soaap-list-fp-targets %t.ll | c++filt > %t.out
 * RUN: FileCheck %s -input-file %t.out
 */
// diamond inheritance 
class A {
  public:
    int a;
    virtual void v() { }
};

class B : public virtual A {
  public:
    int b;
    virtual void w() { }
};

class C : public virtual A {
  public:
    int c;
    virtual void y() { }
};

class D : public B, public C {
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
