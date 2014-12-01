/*
 * RUN: clang %cflags -gsoaap -emit-llvm -S %s -o %t.ll
 * RUN: soaap --soaap-list-fp-targets %t.ll | c++filt > %t.out
 * RUN: FileCheck %s -input-file %t.out
 */
// basic test for non-virtual inheritance
using namespace std;

class A {
  public:
    virtual void foo() { }
};

class B : public A {
};

class C {
  public:
    virtual void bar() { }
};

class D : public C, public B {
};

int main() {
  D* d = new D;
  // CHECK: Call at {{.*}}:[[@LINE+3]]
  // CHECK-NEXT: Targets:
  // CHECK-NEXT: A::foo()
  d->foo();
  return 0;
}
