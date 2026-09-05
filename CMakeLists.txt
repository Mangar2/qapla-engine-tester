cmake_minimum_required(VERSION 3.20)
project(qapla-engine-tester LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  # macOS: libc++ is the system C++ runtime and always present.
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
elseif(NOT WIN32)
  # Linux: build against libstdc++, the runtime every distribution ships. libc++
  # has to be installed on purpose, so a released binary linked against it does
  # not start, and whoever compiles this has to fetch it first -- g++ cannot even
  # be used, because it does not know -stdlib. libstdc++ is still linked into the
  # binary, because its version is the one that travels badly: an older
  # distribution answers a newer one with "libstdc++.so.6: version GLIBCXX_... not
  # found". libgcc_s stays dynamic on purpose -- linking it in drags GCC 13's
  # unwinder along, which calls _dl_find_object and so demands glibc 2.35, locking
  # out RHEL 9. The library itself is everywhere.
  add_link_options(-static-libstdc++)
endif()

# Option to build unit tests
option(BUILD_UNIT_TESTS "Build unit tests" OFF)

# Main executable
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(FILTER SOURCES EXCLUDE REGEX "src/test-system/unit/.*\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "src/json/test/.*\\.cpp$")
add_executable(qapla-engine-tester ${SOURCES})
target_compile_definitions(qapla-engine-tester PRIVATE _REENTRANT)
target_include_directories(qapla-engine-tester PRIVATE . src)

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
    list(FILTER LIB_SOURCES EXCLUDE REGEX "src/json/test/.*\\.cpp$")

    # Create library from main sources (excluding main)
    add_library(qapla-lib STATIC ${LIB_SOURCES})
    target_compile_definitions(qapla-lib PRIVATE _REENTRANT)
    target_include_directories(qapla-lib PUBLIC . src)

    if(APPLE)
        target_link_libraries(qapla-lib PRIVATE pthread)
    elseif(WIN32)
        target_link_libraries(qapla-lib PRIVATE Threads::Threads)
    else()
        target_link_libraries(qapla-lib PRIVATE pthread)
    endif()

    # Collect unit test sources
    file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS "src/test-system/unit/*.cpp")
    file(GLOB_RECURSE JSON_UNIT_TEST_SOURCES CONFIGURE_DEPENDS "src/json/test/*.cpp")
    list(APPEND UNIT_TEST_SOURCES ${JSON_UNIT_TEST_SOURCES})

    # Create unit test executable
    add_executable(qapla-unit-tests ${UNIT_TEST_SOURCES})
    target_link_libraries(qapla-unit-tests PRIVATE qapla-lib Catch2::Catch2WithMain)
    target_compile_definitions(qapla-unit-tests PRIVATE _REENTRANT)

    # Enable CTest
    enable_testing()
    add_test(NAME qapla-unit-tests COMMAND qapla-unit-tests)
endif()
