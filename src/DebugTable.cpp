//
// Created by Will Gulian on 11/26/20.
//

#include <iostream>

extern "C" {
  // #include "dwarf.h"
  #include "libdwarf.h"
}

#include "DebugTable.h"
#include "DwarfInfo.h"


void DebugTable::foo() {

  const char *file_name = "/Users/will/Work/Pear0/cs3210-rustos/kern/build/kernel.elf";

  auto v = DwarfInfo::load_file(file_name);

  if (v.is_ok()) {
    std::cout << "loaded!" << std::endl;

    v.as_value().bruh();

    std::cout << "done!" << std::endl;


  } else {
    std::cout << "error: " << v.as_error() << std::endl;
  }


}
