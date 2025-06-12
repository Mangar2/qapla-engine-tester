cmake_minimum_required(VERSION 3.16)
project(qapla-engine-tester LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXE_LINKER_FLAGS "-static")

file(GLOB SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(qapla-engine-tester ${SOURCES})
target_compile_definitions(qapla-engine-tester PRIVATE _REENTRANT)
target_link_libraries(qapla-engine-tester PRIVATE pthread)