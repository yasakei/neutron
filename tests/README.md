# Neutron Language Tests

This directory contains test scripts for verifying the functionality of the Neutron programming language.

## Global Test

The `global_test.nt` script tests all major features of the Neutron language that are known to work:

1. Variables and data types
2. Arithmetic operations
3. String operations
4. Conditional statements
5. Loops
6. Built-in functions

To run the global test:

```bash
./neutron tests/global_test.nt
```

## Purpose

This test script serves as:
1. A verification that all core language features are working correctly
2. A reference for the syntax and capabilities of the Neutron language
3. A baseline for testing changes to the interpreter

## Notes

- Some features like classes and functions may not be fully implemented or working in the current version
- The modulo operator (%) is not supported in Neutron
- The `typeof` function is not available as a built-in function