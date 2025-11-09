cmake_minimum_required(VERSION 3.20)
project(qapla-engine-tester LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

file(GLOB SOURCES CONFIGURE_DEPENDS "src/*.cpp")
add_executable(qapla-engine-tester ${SOURCES})
target_compile_definitions(qapla-engine-tester PRIVATE _REENTRANT)

if(APPLE)
    # macOS: pthread
    target_link_libraries(qapla-engine-tester PRIVATE pthread)
elseif(WIN32)
    # Windows: Threads
    find_package(Threads REQUIRED)
    target_link_libraries(qapla-engine-tester PRIVATE Threads::Threads)
else()
    # Linux: pthread
    target_link_libraries(qapla-engine-tester PRIVATE pthread)
endif()
