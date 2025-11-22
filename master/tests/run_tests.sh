#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

FAILURES=0
TOTAL=0

print_header() {
    echo -e "${BLUE}╔════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║     Master Module Test Suite          ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════╝${NC}"
}

run_unity_tests() {
    echo -e "\n${YELLOW}📦 Unity Tests (Legacy)${NC}"
    if make -C unity test; then
        echo -e "${GREEN}✅ Unity tests passed${NC}"
    else
        echo -e "${RED}❌ Unity tests failed${NC}"
        FAILURES=$((FAILURES+1))
    fi
    TOTAL=$((TOTAL+1))
}

run_criterion_tests() {
    echo -e "\n${YELLOW}🔬 Criterion Tests${NC}"
    if make -C criterion test; then
        echo -e "${GREEN}✅ Criterion tests passed${NC}"
    else
        echo -e "${RED}❌ Criterion tests failed${NC}"
        FAILURES=$((FAILURES+1))
    fi
    TOTAL=$((TOTAL+1))
}

run_valgrind() {
    if [ "$1" == "--valgrind" ]; then
        echo -e "\n${YELLOW}🔍 Running Valgrind...${NC}"
        if make valgrind 2>&1 | tee valgrind.log; then
            echo -e "${GREEN}✅ No memory leaks detected${NC}"
        else
            echo -e "${RED}❌ Memory leaks found! Check valgrind.log${NC}"
            FAILURES=$((FAILURES+1))
        fi
        TOTAL=$((TOTAL+1))
    fi
}

run_helgrind() {
    if [ "$1" == "--helgrind" ]; then
        echo -e "\n${YELLOW}🔧 Running Helgrind (race condition detection)...${NC}"
        if make helgrind; then
            echo -e "${GREEN}✅ No race conditions detected${NC}"
        else
            echo -e "${RED}⚠️  Potential race conditions found! Check helgrind_*.log${NC}"
            FAILURES=$((FAILURES+1))
        fi
        TOTAL=$((TOTAL+1))
    fi
}

print_summary() {
    echo -e "\n${BLUE}╔════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║            Test Summary                ║${NC}"
    echo -e "${BLUE}╠════════════════════════════════════════╣${NC}"
    echo -e "${BLUE}║${NC} Total test suites: $TOTAL"
    echo -e "${BLUE}║${NC} Passed: $((TOTAL - FAILURES))"
    echo -e "${BLUE}║${NC} Failed: $FAILURES"
    echo -e "${BLUE}╚════════════════════════════════════════╝${NC}"
    
    if [ $FAILURES -eq 0 ]; then
    echo -e "\n${GREEN}🎉 All tests passed successfully!${NC}\n"
        exit 0
    else
        echo -e "\n${RED}⚠️  Some tests failed. Please review the output above.${NC}\n"
        exit 1
    fi
}

# Main execution
print_header

# Parse arguments
VALGRIND=false
HELGRIND=false
SKIP_UNITY=false
SKIP_CRITERION=false

for arg in "$@"; do
    case $arg in
        --valgrind)
            VALGRIND=true
            ;;
        --helgrind)
            HELGRIND=true
            ;;
        --skip-unity)
            SKIP_UNITY=true
            ;;
        --skip-criterion)
            SKIP_CRITERION=true
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --valgrind        Run Valgrind memory checks"
            echo "  --helgrind        Run Helgrind race condition detection"
            echo "  --skip-unity      Skip Unity tests"
            echo "  --skip-criterion  Skip Criterion tests"
            echo "  --help            Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Clean previous builds
echo -e "${YELLOW}🧹 Cleaning previous builds...${NC}"
make clean > /dev/null 2>&1

# Compile all tests
echo -e "${YELLOW}🔨 Compiling tests...${NC}"
if ! make all; then
    echo -e "${RED}❌ Compilation failed!${NC}"
    exit 1
fi
echo -e "${GREEN}✅ Compilation successful${NC}"

# Run tests
[ "$SKIP_UNITY" != true ] && run_unity_tests
[ "$SKIP_CRITERION" != true ] && run_criterion_tests
[ "$VALGRIND" == true ] && run_valgrind "--valgrind"
[ "$HELGRIND" == true ] && run_helgrind "--helgrind"

print_summary