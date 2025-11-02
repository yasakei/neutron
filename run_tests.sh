#!/bin/bash
# Neutron Test Runner v2.0 - Organized Test Suite

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

NEUTRON_BIN="./neutron"
if [ ! -f "$NEUTRON_BIN" ]; then
    echo -e "${RED}Error: neutron binary not found${NC}"
    exit 1
fi

TEST_DIRS=("tests/fixes" "tests/core" "tests/operators" "tests/control-flow" "tests/functions" "tests/classes" "tests/modules")

TOTAL_PASSED=0
TOTAL_FAILED=0
FAILED_TESTS=()

echo -e "${BOLD}${CYAN}╔════════════════════════════════╗${NC}"
echo -e "${BOLD}${CYAN}║  Neutron Test Suite v2.0       ║${NC}"
echo -e "${BOLD}${CYAN}╚════════════════════════════════╝${NC}"
echo ""

for test_dir in "${TEST_DIRS[@]}"; do
    [ ! -d "$test_dir" ] && continue
    
    TEST_FILES=("$test_dir"/*.nt)
    [ ! -f "${TEST_FILES[0]}" ] && continue
    
    DIR_NAME=$(basename "$test_dir")
    echo -e "${BOLD}${BLUE}Testing: ${DIR_NAME}${NC}"
    
    DIR_PASSED=0
    DIR_FAILED=0
    
    for test_file in "${TEST_FILES[@]}"; do
        [ ! -f "$test_file" ] && continue
        
        test_name=$(basename "$test_file" .nt)
        
        if $NEUTRON_BIN "$test_file" > /tmp/neutron_test.txt 2>&1; then
            echo -e "  ${GREEN}✓${NC} ${test_name}"
            ((DIR_PASSED++))
            ((TOTAL_PASSED++))
        else
            echo -e "  ${RED}✗${NC} ${test_name}"
            cat /tmp/neutron_test.txt | sed 's/^/    /'
            FAILED_TESTS+=("$test_dir/$test_name")
            ((DIR_FAILED++))
            ((TOTAL_FAILED++))
        fi
    done
    
    echo -e "  Summary: ${GREEN}$DIR_PASSED passed${NC}, ${RED}$DIR_FAILED failed${NC}"
    echo ""
done

rm -f /tmp/neutron_test.txt

echo -e "${BOLD}════ FINAL SUMMARY ═══${NC}"
echo -e "Total: $((TOTAL_PASSED + TOTAL_FAILED))"
echo -e "${GREEN}Passed: $TOTAL_PASSED${NC}"
echo -e "${RED}Failed: $TOTAL_FAILED${NC}"

if [ $TOTAL_FAILED -gt 0 ]; then
    echo -e "\n${RED}Failed tests:${NC}"
    for test in "${FAILED_TESTS[@]}"; do
        echo -e "  ${RED}✗${NC} $test"
    done
    exit 1
fi

echo -e "\n${GREEN}🎉 All tests passed! 🎉${NC}"
exit 0
