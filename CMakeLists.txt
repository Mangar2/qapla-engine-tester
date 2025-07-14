cmake_minimum_required(VERSION 3.16)
project(qapla-engine-tester LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(GLOB SOURCES CONFIGURE_DEPENDS "src/*.cpp")
add_executable(qapla-engine-tester ${SOURCES})
target_compile_definitions(qapla-engine-tester PRIVATE _REENTRANT)

if(APPLE)
    # macOS: kein statisches Linken
    target_link_libraries(qapla-engine-tester PRIVATE pthread)

else()
    # Linux & Windows mit g++/clang++ (inkl. MinGW): statisch linken
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")

    find_package(Threads REQUIRED)
    target_link_libraries(qapla-engine-tester PRIVATE Threads::Threads)
endif()
