# Neutron Release

This directory contains the Neutron runtime and all necessary files to run applications.

## Contents:
- $(basename neutron) - Main Neutron executable
- libneutron_runtime.(dylib|so) - Runtime library
- modules/ - Optional modules (if any)

## Usage:
Run the binary directly: ./neutron [options]

## Dependencies:
This release may require system libraries like libcurl and libjsoncpp to be installed.
