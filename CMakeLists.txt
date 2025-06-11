cmake_minimum_required(VERSION 3.16)
project(qaplaEngineTester LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXE_LINKER_FLAGS "-static")

file(GLOB SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(qaplaEngineTester ${SOURCES})
target_compile_definitions(qaplaEngineTester PRIVATE _REENTRANT)
target_link_libraries(qaplaEngineTester PRIVATE pthread)