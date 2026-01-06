# GitHub Copilot Instructions

## General Guidelines

### Commit to Git
- Keep the commit information brief and relevant to the changes made.
- use git commit -a -m to checkin.

### Problem-Solving Approach
When encountering uncertainty or problems with implementation:
- **Stop and ask** instead of proceeding with potentially incorrect solutions
- Present concrete options or suggestions for how to proceed
- Wait for user confirmation before implementing an approach

**Example scenarios where you should ask first:**
- When test expectations are unclear and could be derived from actual output (anti-pattern: writing tests based on observed results)
- When multiple implementation approaches exist with different trade-offs
- When the current approach reveals architectural issues that might need user input
- When you're unsure about the correct semantics or expected behavior

### Testing Best Practices
- **Never derive expected test results from actual test output**
- Design tests with independent, calculable expectations
- If test expectations require runtime data (like random deltas), provide mechanisms to inspect that data before validation
- Test intentions should be clear and verifiable without running the code first

## Project-Specific Notes

### SPSA Implementation
- Parameter updates follow: `Δθ_i = r * c_i * gradient_signal * delta_i`
- Deltas are random ±1 values, generated with fixed seed for reproducible tests
- When testing with fixed seeds, either:
  - Calculate expected results from known delta sequences, OR
  - Provide test helper methods to inspect actual deltas and calculate expectations dynamically
