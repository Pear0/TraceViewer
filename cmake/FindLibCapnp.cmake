# - Try to find libcapnp
# Referenced by the find_package() command from the top-level
# CMakeLists.txt.
# Once done this will define
#
#  LIBCAPNP_FOUND - system has libcapnp
#  LIBCAPNP_INCLUDE_DIRS - the libcapnp include directory
#  LIBCAPNP_LIBRARIES - Link these to use libcapnp
#  LIBCAPNP_DEFINITIONS - Compiler switches required for using libcapnp
#
# This module reads hints about search locations from variables:
#
#  LIBCAPNP_ROOT - Preferred installation prefix
#
#  Copyright (c) 2008 Bernhard Walle <bernhard.walle@gmx.de>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

if (LIBCAPNP_LIBRARIES AND LIBCAPNP_INCLUDE_DIRS)
  set (LibCapnp_FIND_QUIETLY TRUE)
endif (LIBCAPNP_LIBRARIES AND LIBCAPNP_INCLUDE_DIRS)

find_path (LIBCAPNP_INCLUDE_DIRS
  NAMES
    capnp/message.h capnp/schema.h
  HINTS
    ${LIBCAPNP_ROOT}
  PATH_SUFFIXES
    include
	libcapnp/include
)
if (NOT LIBCAPNP_INCLUDE_DIRS)
	set (DWARF_WITH_LIBCAPNP OFF)
	return ()
endif()

find_library (LIBCAPNP_LIBRARIES
  NAMES
    capnp libcapnp
  HINTS
    ${LIBCAPNP_ROOT}
  PATH_SUFFIXES 
    lib
	libcapnp/lib
)
if (NOT LIBCAPNP_LIBRARIES)
	 set (DWARF_WITH_LIBCAPNP OFF)
	 return ()
endif()

include (FindPackageHandleStandardArgs)

# handle the QUIETLY and REQUIRED arguments and set LIBCAPNP_FOUND to TRUE if all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibCapnp DEFAULT_MSG
  LIBCAPNP_LIBRARIES
  LIBCAPNP_INCLUDE_DIRS)

mark_as_advanced(LIBCAPNP_INCLUDE_DIRS LIBCAPNP_LIBRARIES)
