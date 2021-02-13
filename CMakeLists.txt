cmake_minimum_required(VERSION 3.17)
project(TraceViewer2)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

string(APPEND CMAKE_CXX_FLAGS_DEBUG " -fno-omit-frame-pointer")
string(APPEND CMAKE_LINKER_FLAGS_DEBUG " -fno-omit-frame-pointer")

string(APPEND CMAKE_CXX_FLAGS_DEBUG " -fsanitize=address")
string(APPEND CMAKE_LINKER_FLAGS_DEBUG " -fsanitize=address")

find_package(LibDwarf REQUIRED)
find_package(LibElf REQUIRED)
find_package(LibCapstone REQUIRED)
find_package(CapnProto REQUIRED)

capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS schema/tracestreaming.capnp)

# Tell cmake where Qt is located
set(Qt5_DIR "/usr/local/opt/qt/lib/cmake/Qt5")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Tell cmake to find the modules Qt5Core and Qt5widgets
find_package(Qt5 COMPONENTS Core Gui Widgets Quick REQUIRED)

include_directories(${LIBDWARF_INCLUDE_DIRS})
include_directories(${LIBELF_INCLUDE_DIRS})
include_directories(${LIBCAPNP_INCLUDE_DIRS})
include_directories(${LIBCAPSTONE_INCLUDE_DIRS})

add_executable(TraceViewer2 ${CAPNP_SRCS}
        src/main.cpp
        src/DebugTable.cpp
        src/DebugTable.h
        src/DwarfInfo.cpp
        src/DwarfInfo.h
        src/FileLoader.cpp
        src/FileLoader.h
        src/ListeningThread.cpp
        src/ListeningThread.h
        src/QtCustomUser.h
        src/QtOutputStream.cpp
        src/QtOutputStream.h
        src/Result.h
        src/TraceData.cpp
        src/TraceData.h
        src/Disassembler/Disassembler.cpp
        src/Disassembler/Disassembler.h
        src/Model/DisassemblyModel.cpp
        src/Model/DisassemblyModel.h
        src/Model/TraceHierarchyModel.cpp
        src/Model/TraceHierarchyModel.h
        src/Model/TraceHierarchyFilterProxy.cpp
        src/Model/TraceHierarchyFilterProxy.h
        src/View/CustomTraceDialog.cpp
        src/View/CustomTraceDialog.h
        src/View/TraceTimeline.cpp
        src/View/TraceTimeline.h
        src/View/TraceViewWindow.cpp
        src/View/TraceViewWindow.h
        src/Model/DisassemblyInlinesModel.cpp
        src/Model/DisassemblyInlinesModel.h)

set_target_properties(TraceViewer2 PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Trace Viewer"
        MACOSX_BUNDLE_GUI_IDENTIFIER com.pear0.traceviewer
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist"
        )

target_include_directories(TraceViewer2 PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Link the library to the executable
target_link_libraries(TraceViewer2 PRIVATE Qt5::Core Qt5::Widgets Qt5::Quick)
target_link_libraries(TraceViewer2 PRIVATE z ${LIBELF_LIBRARIES} ${LIBDWARF_LIBRARIES} ${LIBCAPSTONE_LIBRARIES})
target_link_libraries(TraceViewer2 PRIVATE CapnProto::capnp-rpc)
