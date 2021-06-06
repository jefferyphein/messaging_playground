#include <fstream>
#include "hello.pb.h"

int main() {
  sayhello::Person person;
  person.set_name("test");
  person.set_email("test@example.com");

  std::ofstream ofs("cpp_test.out", std::ios::out | std::ios::binary);
  person.SerializePartialToOstream(&ofs);

}
