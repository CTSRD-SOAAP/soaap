/*
 * RUN: clang++ %cflags -gsoaap -emit-llvm -c %s -o %t.ll
 * RUN: soaap --soaap-list-fp-targets %t.ll | c++filt > %t.out
 * RUN: FileCheck %s -input-file %t.out
 */
// defining type as a suboject of virtual base 
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

int main(int argc, char** argv) {
  B* b = new B;
  // CHECK: Call at {{.*}}:[[@LINE+3]]
  // CHECK-NEXT: Targets:
  // CHECK-NEXT: A::v()
  b->v();
  return 0;
}
