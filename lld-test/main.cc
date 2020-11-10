#include <iostream>
#include "foo.h"

extern int foo();

int main(int argc, char **argv) {

exit(foo() + bug::get_thing("baz"));
}

#ifndef ALLOW_INIT
struct A {
  int i;

  A(int i) : i(i) { std::cout << "ctor a" << i << '\n'; }

  ~A() { std::cout << "dtor a" << i << '\n'; }
};

A a0(0);
#endif

