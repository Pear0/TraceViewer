cmake_minimum_required(VERSION 3.17)
project(TraceViewer2)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

string(APPEND CMAKE_CXX_FLAGS_DEBUG " -fno-omit-frame-pointer")
string(APPEND CMAKE_LINKER_FLAGS_DEBUG " -fno-omit-frame-pointer")

string(APPEND CMAKE_CXX_FLAGS_DEBUG " -fsanitize=address")
string(APPEND CMAKE_LINKER_FLAGS_DEBUG " -fsanitize=address")

set(LIBDWARF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/libdwarf")

# find_package(LibDwarf REQUIRED)
find_package(LibElf REQUIRED)
find_package(CapnProto REQUIRED)

set(OUTPUT_LIB "${CMAKE_BINARY_DIR}/libdwarf.a")

capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS schema/tracestreaming.capnp)


# Tell cmake where Qt is located
set(Qt5_DIR "/usr/local/opt/qt/lib/cmake/Qt5")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Tell cmake to find the modules Qt5Core and Qt5widgets
find_package(Qt5 COMPONENTS Core Gui Widgets Quick REQUIRED)

include_directories(lib/libdwarf/build/libdwarf)
include_directories(lib/libdwarf/libdwarf)
include_directories(${LIBELF_INCLUDE_DIRS})
include_directories(${LIBCAPNP_INCLUDE_DIRS})

add_executable(TraceViewer2
        src/main.cpp
        src/ListeningThread.cpp
        src/ListeningThread.h
        src/TraceData.cpp
        src/TraceData.h
        src/DebugTable.cpp
        src/DebugTable.h
        src/DwarfInfo.cpp
        src/DwarfInfo.h
        src/Result.h
        src/TraceModel.cpp
        src/TraceModel.h
        src/TraceViewWindow.cpp
        src/TraceViewWindow.h
        ${CAPNP_SRCS})

target_include_directories(TraceViewer2 PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_directories(TraceViewer2 PRIVATE "/usr/local/lib")  # for libelf.a
target_link_directories(TraceViewer2 PRIVATE "lib/libdwarf/build/libdwarf/.libs")  # for libdwarf.a

# Link the library to the executable
target_link_libraries(TraceViewer2 PRIVATE Qt5::Core Qt5::Widgets Qt5::Quick dwarf z ${LIBELF_LIBRARIES})
target_link_libraries(TraceViewer2 PRIVATE CapnProto::capnp-rpc)
