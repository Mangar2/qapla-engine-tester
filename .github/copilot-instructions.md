# Copilot Instructions

Always use english language for any comment in checkin and code.

## Extending copilot instructions
Remember that this text is for you only. Be as brief as ever possible, never waste token.

## Checking in Code for Qapla Chess GUI
If I ask you to checkin:
- use git commit -a -m to checkin. You do not need to stage files or updated submodule references.

## Project Overview
**Qapla Engine Tester** is the cli version of Qapla Chess Gui as well as the base library for it. Important: changes in the library may affect the gui and this is not visible when building the cli version. Ask before changing any public interfaces.

## Renaming symbols in c++
Never rename symbols in c++ that are heavily used across files. Ask the user to do it because it is efficently possible with the IDE.

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
- Use `std::format` for string formatting
- Use "auto" whenever possible, make it complete like "const auto*" instead of just auto
- Avoid implicit type conversions (e.g. pointer to bool)
- Avoid copying code. Instead create methods to be used by multiple callers.
- Use single tab indentation for continuation lines, NOT alignment to opening parenthesis

## Unit-Test Design Principles
- **Test only public interface**: Unit-tests verify behavior through public methods only - never access private members
- **Read implementation first**: Before writing tests, read the `.cpp` file to identify which logic is owned vs. delegated - only test owned logic
- **Minimal test coverage**: Design the smallest set of tests that covers all code paths and edge cases
- **Test ordering**: Basic functionality tests first, edge cases and special scenarios last

## Ask if things do not fit
Sometimes function return parameters don´t fit to the format or form we need. Sometimes we may change the the function sometimes we dont´t. So ask what should be done before implementing complex transformation.

## CLI return codes
- 0 `NoError`: Everything ran correctly; test completed as expected
- 1 `GeneralError`: Unexpected program error (e.g. crash, unhandled exception)
- 2 `InvalidParameters`: Invalid or missing CLI parameter
- 10 `EngineError`: Engine crashed, could not start, or returned illegal moves
- 11 `EngineMissbehaviour`: Engine hung, ignored protocol, or failed to follow commands
- 12 `EngineNote`: Test completed, but non-critical engine issues occurred (only in `--test`)
- 13 `MissedTarget`: EPD target success threshold was not reached (`--epd`)
- 14 `H1Accepted`: SPRT result: H₁ (stronger engine) accepted (`--sprt`)
- 15 `H0Accepted`: SPRT result: H₀ (no significant difference) accepted (`--sprt`)
- 16 `UndefinedResult`: SPRT result could not be decided within maxGames (`--sprt`)

## Creating tests
- Never fix a test without having checked before that the problem is not in the main code. Inform me how you verified this.
