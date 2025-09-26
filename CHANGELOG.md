# Changelog

All notable changes to the Neutron programming language will be documented in this file.

## [Unreleased] - YYYY-MM-DD

### Added
- Improved stack management in control flow operations
- Enhanced module loading system with recursive function support
- Dual access pattern for built-in modules (`str(42)` and `convert.str(42)`)
- Development tests verification (all dev_tests now pass)

### Fixed
- Fixed `OP_JUMP_IF_FALSE` bytecode to properly pop condition values from stack
- Resolved function call return value corruption inside loops and conditionals
- Corrected module loading to support recursive function calls within same module
- Fixed variable access in user-defined modules

### Changed
- Module loading now preserves essential functions during execution context setup
- Native functions registered for both global and module access patterns
- Enhanced error reporting for module loading issues

### Security

## Previous Versions

### Core Architecture
- Scanner and tokenization system
- Parser with AST generation
- Bytecode compiler
- Virtual machine execution
- Native function integration
- Module system with built-in and user modules
- Object-oriented programming with classes
- Memory management and garbage collection