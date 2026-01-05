# Unit Tests

This directory contains unit tests for the Qapla Engine Tester using the Catch2 framework.

## Building and Running Unit Tests

### Configure and Build

```bash
# Configure with unit test preset
cmake --preset=unit

# Build unit tests
cmake --build --preset=unit

# Run all unit tests
cd build/unit
ctest

# Or run directly
./qapla-unit-tests

# Run with verbose output
./qapla-unit-tests -v

# Run specific test cases
./qapla-unit-tests "[example]"
./qapla-unit-tests "[math]"
```

### Catch2 Documentation

For more information on writing tests with Catch2, see:
- [Catch2 Documentation](https://github.com/catchorg/Catch2/tree/devel/docs)
- [Catch2 Tutorial](https://github.com/catchorg/Catch2/blob/devel/docs/tutorial.md)

## Test Organization

- **example-test.cpp**: Example tests demonstrating basic Catch2 usage
- Add your test files here with descriptive names ending in `-test.cpp`

## Writing Tests

Example test structure:

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Test description", "[tag]") {
    SECTION("Section name") {
        REQUIRE(condition);
        CHECK(condition);
    }
}
```

### Common Assertions

- `REQUIRE(expr)` - Fails test if false
- `CHECK(expr)` - Records failure but continues
- `REQUIRE_FALSE(expr)` - Requires expression to be false
- `REQUIRE_THROWS(expr)` - Requires exception to be thrown
- `REQUIRE_NOTHROW(expr)` - Requires no exception

### Tags

Use tags to organize and filter tests:
```cpp
TEST_CASE("Description", "[unit][component]") { ... }
```

Run specific tags:
```bash
./qapla-unit-tests "[unit]"
```
