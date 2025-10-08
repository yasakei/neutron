# Box Package Manager - Complete Guide

## Table of Contents
1. [Introduction](#introduction)
2. [Installation](#installation)
3. [Commands Reference](#commands-reference)
4. [Module Development](#module-development)
5. [Cross-Platform Support](#cross-platform-support)
6. [NUR Registry](#nur-registry)

## Introduction

Box is Neutron's official package manager for installing and managing native modules.

Features:
- ✅ Install modules from NUR registry
- ✅ Version management
- ✅ Cross-platform builds (Linux, macOS, Windows)
- ✅ Local module installation (./.box/modules/)
- ✅ C API for native module development

## Installation

Box is built automatically with Neutron:

\`\`\`bash
# Build Neutron + Box
cmake --build build

# Box binary available at ./box
\`\`\`

## Commands Reference

See [COMMANDS.md](./COMMANDS.md) for detailed command reference.

## Module Development

See [MODULE_DEVELOPMENT.md](./MODULE_DEVELOPMENT.md) for creating native modules.

## Cross-Platform Support

Box supports building modules on:
- **Linux** - .so (gcc/clang++)
- **macOS** - .dylib (clang++)
- **Windows** - .dll (MSVC)

## NUR Registry

Official module registry: https://github.com/neutron-modules/nur
