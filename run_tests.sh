#!/bin/bash

# Neutron Test Runner
# Runs all test files in the tests/ directory

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Check if neutron binary exists
if [ ! -f "./neutron" ]; then
    echo -e "${RED}Error: neutron binary not found${NC}"
    echo "Please build the project first: make"
    exit 1
fi

# Find all test files
TEST_FILES=(tests/test_*.nt)

if [ ${#TEST_FILES[@]} -eq 0 ]; then
    echo -e "${YELLOW}No test files found in tests/ directory${NC}"
    exit 0
fi

# Test results
PASSED=0
FAILED=0
FAILED_TESTS=()

echo "================================"
echo "  Neutron Test Suite"
echo "================================"
echo ""

# Run each test
for test_file in "${TEST_FILES[@]}"; do
    if [ ! -f "$test_file" ]; then
        continue
    fi
    
    test_name=$(basename "$test_file")
    echo -e "${YELLOW}Running: $test_name${NC}"
    
    # Run the test and capture output
    if ./neutron "$test_file" > /tmp/neutron_test_output.txt 2>&1; then
        echo -e "${GREEN}✓ PASSED${NC}"
        cat /tmp/neutron_test_output.txt
        ((PASSED++))
    else
        echo -e "${RED}✗ FAILED${NC}"
        cat /tmp/neutron_test_output.txt
        FAILED_TESTS+=("$test_name")
        ((FAILED++))
    fi
    
    echo ""
done

# Cleanup
rm -f /tmp/neutron_test_output.txt

# Print summary
echo "================================"
echo "  Test Summary"
echo "================================"
echo -e "Total tests: $((PASSED + FAILED))"
echo -e "${GREEN}Passed: $PASSED${NC}"
echo -e "${RED}Failed: $FAILED${NC}"

if [ $FAILED -gt 0 ]; then
    echo ""
    echo "Failed tests:"
    for test in "${FAILED_TESTS[@]}"; do
        echo -e "  ${RED}✗ $test${NC}"
    done
fi

echo ""

# Exit with appropriate code
if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi
