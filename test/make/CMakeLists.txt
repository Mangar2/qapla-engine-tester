cmake_minimum_required(VERSION 3.20)
project(qapla-engine-tester-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

# Source directory for qapla-engine
set(QAPLA_ENGINE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

# Test source files
set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/movescanner/movescanner_test.cpp
)

# Create the test executable
add_executable(qapla-engine-tester-tests ${TEST_SOURCES})

# Include the qapla-engine source directory for header access
target_include_directories(qapla-engine-tester-tests PRIVATE ${QAPLA_ENGINE_SRC_DIR})

# Platform-specific threading libraries
if(APPLE)
    target_link_libraries(qapla-engine-tester-tests PRIVATE pthread)
elseif(WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(qapla-engine-tester-tests PRIVATE Threads::Threads)
else()
    target_link_libraries(qapla-engine-tester-tests PRIVATE pthread)
endif()
