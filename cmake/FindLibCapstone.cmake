# - Try to find libcapstone
# Referenced by the find_package() command from the top-level
# CMakeLists.txt.
# Once done this will define
#
#  LIBCAPSTONE_FOUND - system has libcapstone
#  LIBCAPSTONE_INCLUDE_DIRS - the libcapstone include directory
#  LIBCAPSTONE_LIBRARIES - Link these to use libcapstone
#  LIBCAPSTONE_DEFINITIONS - Compiler switches required for using libcapstone
#
# This module reads hints about search locations from variables:
#
#  LIBCAPSTONE_ROOT - Preferred installation prefix
#
#  Copyright (c) 2008 Bernhard Walle <bernhard.walle@gmx.de>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

if (LIBCAPSTONE_LIBRARIES AND LIBCAPSTONE_INCLUDE_DIRS)
  set (LibCapstone_FIND_QUIETLY TRUE)
endif (LIBCAPSTONE_LIBRARIES AND LIBCAPSTONE_INCLUDE_DIRS)

find_path (LIBCAPSTONE_INCLUDE_DIRS
  NAMES
    capstone/capstone.h capstone/arm64.h
  HINTS
    ${LIBCAPSTONE_ROOT}
  PATH_SUFFIXES
    include
	capstone/include
)
if (NOT LIBCAPSTONE_INCLUDE_DIRS)
	return ()
endif()

find_library (LIBCAPSTONE_LIBRARIES
  NAMES
    capstone libcapstone
  HINTS
    ${LIBCAPSTONE_ROOT}
  PATH_SUFFIXES 
    lib
	capstone/lib
)
if (NOT LIBCAPSTONE_LIBRARIES)
	 return ()
endif()

include (FindPackageHandleStandardArgs)

# handle the QUIETLY and REQUIRED arguments and set LIBCAPSTONE_FOUND to TRUE if all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibCapstone DEFAULT_MSG
  LIBCAPSTONE_LIBRARIES
  LIBCAPSTONE_INCLUDE_DIRS)

set(CMAKE_REQUIRED_LIBRARIES capstone)
include(CheckCSourceCompiles)
check_c_source_compiles("#include <capstone/capstone.h>
int main()
{
	csh handle;
	cs_insn *insn;
	size_t count;
	if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK)
		return -1;
	count = cs_disasm(handle, 0, 0, 0x1000, 0, &insn);
	cs_close(&handle);
	return 0;
}
" CAPSTONE_GETSHDRSTRNDX)
unset(CMAKE_REQUIRED_LIBRARIES)

mark_as_advanced(LIBCAPSTONE_INCLUDE_DIRS LIBCAPSTONE_LIBRARIES CAPSTONE_GETSHDRSTRNDX)
