cmake_minimum_required(VERSION 3.20)
project(qapla-engine-tester LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

# Option to build unit tests
option(BUILD_UNIT_TESTS "Build unit tests" OFF)

# Main executable
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(FILTER SOURCES EXCLUDE REGEX "src/test-system/unit/.*\\.cpp$")
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

# Unit Tests
if(BUILD_UNIT_TESTS)
    # Download Catch2
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.2
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/extern/Catch2
    )
    FetchContent_MakeAvailable(Catch2)

    # Collect all non-main sources for the library
    file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
    list(FILTER LIB_SOURCES EXCLUDE REGEX "src/qapla-engine-tester\\.cpp$")
    list(FILTER LIB_SOURCES EXCLUDE REGEX "src/test-system/unit/.*\\.cpp$")

    # Create library from main sources (excluding main)
    add_library(qapla-lib STATIC ${LIB_SOURCES})
    target_compile_definitions(qapla-lib PRIVATE _REENTRANT)

    if(APPLE)
        target_link_libraries(qapla-lib PRIVATE pthread)
    elseif(WIN32)
        target_link_libraries(qapla-lib PRIVATE Threads::Threads)
    else()
        target_link_libraries(qapla-lib PRIVATE pthread)
    endif()

    # Collect unit test sources
    file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS "src/test-system/unit/*.cpp")

    # Create unit test executable
    add_executable(qapla-unit-tests ${UNIT_TEST_SOURCES})
    target_link_libraries(qapla-unit-tests PRIVATE qapla-lib Catch2::Catch2WithMain)
    target_compile_definitions(qapla-unit-tests PRIVATE _REENTRANT)

    # Enable CTest
    enable_testing()
    add_test(NAME qapla-unit-tests COMMAND qapla-unit-tests)
endif()
