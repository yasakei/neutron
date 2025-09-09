# Contributing to Neutron

We're excited that you're interested in contributing to Neutron! This document provides guidelines and information to help you get started.

## Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/yasakei/neutron.git`
3. Create a new branch for your feature: `git checkout -b my-feature`
4. Make your changes
5. Test your changes
6. Commit your changes: `git commit -am "Add my feature"`
7. Push to your fork: `git push origin my-feature`
8. Create a pull request

## Project Structure

- `src/` - C++ source files
- `include/` - Header files
- `examples/` - Example Neutron programs
- `tests/` - Unit tests
- `docs/` - Documentation

## Modular Design

Neutron is designed to be modular, making it easy to add new features. Each major component should be in its own files:

- Scanner: `scanner.h`/`scanner.cpp`
- Parser: `parser.h`/`parser.cpp`
- Code Generator: (to be implemented)
- Virtual Machine: (to be implemented)

## Coding Standards

- Follow C++17 standards
- Use meaningful variable and function names
- Comment your code where necessary
- Write unit tests for new functionality

## Testing

Run the existing tests to ensure nothing is broken:

```bash
cd tests
make test_scanner
```

Add new tests when implementing features.

## Reporting Issues

Please use the GitHub issue tracker to report bugs or suggest features.

## Contact

For questions or discussions, please open an issue or contact the maintainers.