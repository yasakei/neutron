# Neutron Changelog

All notable changes to the Neutron programming language will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.0.2-alpha] - 2025-10-06

### Added
- **Match Statement** - Pattern matching for cleaner conditional logic
  - Support for numbers, strings, booleans, and expressions
  - Arrow operator (`=>`) for single-line cases
  - Block statement cases with curly braces
  - Optional default case
  - Nested match statements
  - Test suite: `tests/test_match.nt`
  
- **Lambda Functions** - Anonymous function expressions
  - Inline function definitions with `fun(params) { body }` syntax
  - Lambdas stored in variables and arrays
  - Lambdas as function arguments
  - Immediately invoked lambda expressions
  - Test suite: `tests/test_lambda_comprehensive.nt`
  
- **Try-Catch Infrastructure** - Foundation for exception handling
  - Tokens: `try`, `catch`, `finally`, `throw`
  - AST structures for exception handling
  - Parser support (VM implementation deferred)

- **New Bytecode Operations**
  - `OP_DUP` - Duplicates value on stack top
  - `OP_CLOSURE` - Creates closure from function

### Fixed
- Match statement stack underflow with multiple cases
  - Fixed duplicate POP after `OP_JUMP_IF_FALSE` which already pops

### Documentation
- Updated `docs/ROADMAP.md` with task completion status
- Updated `docs/language_reference.md` with match and lambda documentation
- Added `docs/IMPLEMENTATION_SUMMARY.md` with technical details
- Added `logs/v1.0.2-alpha.md` with detailed changelog

### Testing
- 10 comprehensive match statement tests
- 8 comprehensive lambda function tests
- All tests passing

---

## [1.0.1-alpha] - Previous Release

_Documentation for previous releases to be added_

---

## [1.0.0-alpha] - Initial Release

_Documentation for initial release to be added_

---

For detailed information about each release, see the `logs/` directory.
