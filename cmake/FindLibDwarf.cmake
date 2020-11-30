# - Try to find libelf
# Referenced by the find_package() command from the top-level
# CMakeLists.txt.
# Once done this will define
#
#  LIBDWARF_FOUND - system has libelf
#  LIBDWARF_INCLUDE_DIRS - the libelf include directory
#  LIBDWARF_LIBRARIES - Link these to use libelf
#  LIBDWARF_DEFINITIONS - Compiler switches required for using libelf
#
# This module reads hints about search locations from variables:
#
#  LIBDWARF_ROOT - Preferred installation prefix
#
#  Copyright (c) 2008 Bernhard Walle <bernhard.walle@gmx.de>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

#if (LIBELF_LIBRARIES AND LIBELF_INCLUDE_DIRS)
#  set (LibElf_FIND_QUIETLY TRUE)
#endif (LIBELF_LIBRARIES AND LIBELF_INCLUDE_DIRS)

find_path (LIBDWARF_INCLUDE_DIRS
  NAMES
    libdwarf/libdwarf.h libdwarf.h
  HINTS
    ${LIBDWARF_ROOT}
  PATH_SUFFIXES
    include
	libdwarf/include
)

find_library (LIBDWARF_LIBRARIES
  NAMES
    dwarf libdwarf
  HINTS
    ${LIBDWARF_ROOT}
  PATH_SUFFIXES 
    lib
	libdwarf/lib
)

include (FindPackageHandleStandardArgs)

# handle the QUIETLY and REQUIRED arguments and set LIBELF_FOUND to TRUE if all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibDwarf DEFAULT_MSG
  LIBDWARF_LIBRARIES
  LIBDWARF_INCLUDE_DIRS)

set(CMAKE_REQUIRED_LIBRARIES dwarf)
include(CheckCSourceCompiles)
check_c_source_compiles("#include <libdwarf.h>
int main() {
  return 0;
}" DWARF_COMPILEX)
unset(CMAKE_REQUIRED_LIBRARIES)

mark_as_advanced(LIBDWARF_INCLUDE_DIRS LIBDWARF_LIBRARIES DWARF_COMPILEX)
