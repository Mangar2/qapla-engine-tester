# Copilot Instructions

Always use english language for any comment in checkin and code.

## Checking in Code for Qapla Chess GUI
If I ask you to checkin:
- use git commit -a -m to checkin. You do not need to stage files or updated submodule references.

## Project Overview
**Qapla Engine Tester** is the cli version of Qapla Chess Gui as well as the base library for it. Important: changes in the library may affect the gui and this is not visible when building the cli version. Ask before changing any public interfaces.

## C++ Code Style
- C++20 - use ranges, format, nodiscard
- Min identifier length: 3 chars
- Only "why" comments (no comments describing what the code lines does); Still JSDoc for method declarations
- Max complexity: 20, max nesting: 3
- Prevent implicit conversions, use same types
- **Explicit bool conversions**: Never rely on implicit int→bool. Use `!= 0` (preferred) or `static_cast<bool>()`
- Always use curly braces for control statements
- Use `auto` when type is already visible in the line
- Do not return data via reference or pointer parameters. Use return values instead. Remember that the compiler will optimize return value copies via RVO.
- Only comment inside methods to explain why something is done - never what is done

## Unit-Test Design Principles
- **Test only public interface**: Unit-tests verify behavior through public methods only - never access private members
- **Read implementation first**: Before writing tests, read the `.cpp` file to identify which logic is owned vs. delegated - only test owned logic
- **Minimal test coverage**: Design the smallest set of tests that covers all code paths and edge cases
- **Test ordering**: Basic functionality tests first, edge cases and special scenarios last

## Ask if things do not fit
Sometimes function return parameters don´t fit to the format or form we need. Sometimes we may change the the function sometimes we dont´t. So ask what should be done before implementing complex transformation.