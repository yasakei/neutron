#!/bin/bash

# Neutron Compiler Demo Script

echo "=== Neutron Programming Language Compiler Demo ==="
echo

# Build the compiler
echo "Building the Neutron compiler..."
make clean && make
echo

# Show the comprehensive example
echo "Showing the comprehensive example..."
echo "----------------------------------------"
cat examples/comprehensive.nt
echo
echo "----------------------------------------"
echo

# Run the compiler on the comprehensive example
echo "Running the Neutron compiler on the comprehensive example..."
echo "Tokens generated:"
echo "----------------------------------------"
./neutron examples/comprehensive.nt
echo
echo "----------------------------------------"
echo

echo "Demo complete!"
echo
echo "To try the REPL, run: ./neutronc"
echo "To compile a file, run: ./neutronc <filename.nt>"