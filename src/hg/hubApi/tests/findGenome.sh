#!/bin/bash

# Test harness for findGenome API changes
# Tests search functionality before and after performance optimizations

# set -e

# Configuration
apiBinary="../hubApi"
testDir="findGenome_tests"
beforeDir="${testDir}/before"
afterDir="${testDir}/after"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Create test directories
mkdir -p "${beforeDir}" "${afterDir}"

# Test cases array - each line: "testName|queryParams"
declare -a testCases=(
    # Basic text searches
    "basicSingleWord|q=human&maxItemsOutput=5"
    "basicMultiWord|q=white%20rhino&maxItemsOutput=5"
    "quotedPhrase|q=\"white%20rhino\"&maxItemsOutput=5"

    # Short word tests (affected by ft_min_word_len=3)
    "shortAssemblyHg|q=hg&maxItemsOutput=10"
    "shortAssemblyMm|q=mm&maxItemsOutput=10"
    "shortAssemblyDm|q=dm&maxItemsOutput=10"
    "shortWithWildcard|q=hg*&maxItemsOutput=10"

    # Operator tests
    "plusOperator|q=+human&maxItemsOutput=5"
    "minusOperator|q=human%20-mouse&maxItemsOutput=5"
    "wildcardSearch|q=homo*&maxItemsOutput=5"
    "complexOperators|q=+-mouse%20sapiens*%20rat&maxItemsOutput=10"

    # Filter tests
    "filterReference|q=human&category=reference&maxItemsOutput=5"
    "filterRepresentative|q=human&category=representative&maxItemsOutput=5"
    "filterLatest|q=human&status=latest&maxItemsOutput=5"
    "filterComplete|q=human&level=complete&maxItemsOutput=5"
    "filterChromosome|q=human&level=chromosome&maxItemsOutput=5"

    # Browser existence filters
    "browserMustExist|q=human&browser=mustExist&maxItemsOutput=5"
    "browserNotExist|q=human&browser=notExist&maxItemsOutput=5"
    "browserMayExist|q=human&browser=mayExist&maxItemsOutput=5"

    # Combined filters
    "multiFilters|q=human&category=reference&status=latest&level=complete&maxItemsOutput=5"
    "textPlusFilters|q=white%20rhino&browser=mustExist&status=latest&maxItemsOutput=3"

    # Edge cases
    "noResults|q=zyxwvutsrq&maxItemsOutput=5"
    "specialChars|q=C.%20elegans&maxItemsOutput=5"
    "numbersYear|q=2020&maxItemsOutput=5"

    # Performance test cases
    "largeResultSet|q=chromosome&maxItemsOutput=50"
    "complexSearch|q=+human%20+reference%20-mouse&category=reference&status=latest&maxItemsOutput=20"

    # Specific assembly searches
    "assemblyHg38|q=hg38&maxItemsOutput=3"
    "assemblyMm39|q=mm39&maxItemsOutput=3"
    "assemblyPrefix|q=GCA_*&maxItemsOutput=10"
)

# Function to clear MySQL caches
clearSqlCaches() {
    echo "  Clearing SQL caches..."

    # Connect to MySQL and clear various caches
    hgsql -e "
          FLUSH QUERY CACHE;
          FLUSH TABLES;
          RESET QUERY CACHE;
          FLUSH STATUS;
    " hgcentraltest 2>/dev/null || {
       echo "  Warning: Could not clear SQL caches (may need admin privileges)"
    }
}

# Function to run a single test
runTest() {
    local testName="$1"
    local queryParams="$2"
    local outputDir="$3"
    local timingFile="$4"

    printf "#  Running: '%s'\n" "${testName}" 1>&2

    # Add timing measurement
    local startTime=$(date +%s%N)

    # Run the API call - filter CGI headers with grep "^{"
    if PATH_INFO="/findGenome" ${apiBinary} ${queryParams} | grep "^{" > "${outputDir}/${testName}.json" 2>"${outputDir}/${testName}.err"; then
        local endTime=$(date +%s%N)
        local durationMs=$(( (endTime - startTime) / 1000000 ))
        echo "${testName}:${durationMs}" >> "$timingFile"

        # Validate JSON output
        if ! python3 -m json.tool "${outputDir}/${testName}.json" >/dev/null 2>&1; then
            echo -e "${RED}WARNING: Invalid JSON output for $testName${NC}"
        fi

        return 0
    else
        local endTime=$(date +%s%N)
        local durationMs=$(( (endTime - startTime) / 1000000 ))
        echo "${testName}:${durationMs}:ERROR" >> "$timingFile"
        return 1
    fi
}

# Function to extract key metrics from JSON
extractMetrics() {
    local jsonFile="$1"

    if [ ! -f "$jsonFile" ]; then
        echo "ERROR:missing_file"
        return
    fi

    # Extract key fields for comparison
    python3 -c "
import json
import sys

try:
    with open('$jsonFile', 'r') as f:
        data = json.load(f)

    # Extract comparable metrics
    metrics = {
        'itemCount': data.get('itemCount', 0),
        'totalMatchCount': data.get('totalMatchCount', 0),
        'availableAssemblies': data.get('availableAssemblies', 0),
        'resultCount': len([k for k in data.keys() if k not in ['itemCount', 'totalMatchCount', 'availableAssemblies', 'q', 'browser', 'maxItemsLimit', 'category', 'status', 'level', 'liftable']])
    }

    # Sort assembly results for consistent comparison
    assemblies = []
    for key, value in data.items():
        if isinstance(value, dict) and 'scientificName' in value:
            assemblies.append(key)

    metrics['assemblyIds'] = sorted(assemblies)

    print(json.dumps(metrics, sort_keys=True))

except Exception as e:
    print(f'ERROR:{str(e)}')
" 2>/dev/null
}

# Function to compare two test results
compareResults() {
    local testName="$1"
    local beforeFile="${beforeDir}/${testName}.json"
    local afterFile="${afterDir}/${testName}.json"

    local beforeMetrics=$(extractMetrics "$beforeFile")
    local afterMetrics=$(extractMetrics "$afterFile")

    if [ "$beforeMetrics" = "$afterMetrics" ]; then
        echo -e "${GREEN}✓${NC} $testName"
        return 0
    else
        echo -e "${RED}✗${NC} $testName - Results differ!"
        echo "  Before: $beforeMetrics"
        echo "  After:  $afterMetrics"
        return 1
    fi
}

# Function to run all tests
runTestSuite() {
    local phase="$1"
    local outputDir="$2"
    local timingFile="${outputDir}/timing.txt"

    echo -e "${BLUE}Running $phase tests...${NC}"
    # Clear SQL caches before test run
    clearSqlCaches

    # Clear timing file
    > "$timingFile"

    local passed=0
    local failed=0

    for testCase in "${testCases[@]}"; do
        IFS='|' read -r testName queryParams <<< "$testCase"

        if runTest "$testName" "$queryParams" "$outputDir" "$timingFile"; then
            ((passed++))
        else
            ((failed++))
            echo -e "${RED}FAILED: $testName${NC}"
        fi
    done

    echo -e "${BLUE}$phase Results: ${GREEN}$passed passed${NC}, ${RED}$failed failed${NC}"

    # Show timing summary
    if [ -f "$timingFile" ]; then
        echo -e "${YELLOW}Timing Summary ($phase):${NC}"
        sort -t: -k2 -n "$timingFile" | tail -5 | while IFS=':' read -r name time rest; do
            echo "  $name: ${time}ms"
        done
    fi
}

# Function to compare timing performance
compareTiming() {
    echo -e "${BLUE}Performance Comparison:${NC}"

    if [ -f "${beforeDir}/timing.txt" ] && [ -f "${afterDir}/timing.txt" ]; then
        python3 -c "
import sys

# Read timing data
beforeTimes = {}
afterTimes = {}

with open('${beforeDir}/timing.txt', 'r') as f:
    for line in f:
        parts = line.strip().split(':')
        if len(parts) >= 2 and parts[1].isdigit():
            beforeTimes[parts[0]] = int(parts[1])

with open('${afterDir}/timing.txt', 'r') as f:
    for line in f:
        parts = line.strip().split(':')
        if len(parts) >= 2 and parts[1].isdigit():
            afterTimes[parts[0]] = int(parts[1])

# Calculate improvements
improvements = []
for testName in beforeTimes:
    if testName in afterTimes:
        beforeTime = beforeTimes[testName]
        afterTime = afterTimes[testName]
        if beforeTime > 0:
            improvement = ((beforeTime - afterTime) / beforeTime) * 100
            improvements.append((testName, beforeTime, afterTime, improvement))

# Sort by improvement percentage
improvements.sort(key=lambda x: x[3], reverse=True)

# Show top improvements
print('Top Performance Improvements:')
for testName, beforeTime, afterTime, improvement in improvements[:10]:
    if improvement > 0:
        print(f'  {testName}: {beforeTime}ms → {afterTime}ms ({improvement:+.1f}%)')
    else:
        print(f'  {testName}: {beforeTime}ms → {afterTime}ms ({improvement:+.1f}%)')
"
    fi
}

# Main execution
main() {
    echo -e "${BLUE}FindGenome API Test Harness${NC}"
    echo "Testing ${#testCases[@]} test cases"
    echo

    # Check if binary exists
    if [ ! -x "$apiBinary" ]; then
        echo -e "${RED}Error: $apiBinary not found or not executable${NC}"
        echo "Please compile the hubApi binary first"
        exit 1
    fi

    case "${1:-}" in
        "before")
            runTestSuite "BEFORE" "$beforeDir"
            ;;
        "after")
            runTestSuite "AFTER" "$afterDir"
            ;;
        "compare")
            if [ ! -d "$beforeDir" ] || [ ! -d "$afterDir" ]; then
                echo -e "${RED}Error: Run 'before' and 'after' tests first${NC}"
                exit 1
            fi

            echo -e "${BLUE}Comparing results...${NC}"
            local differences=0

            for testCase in "${testCases[@]}"; do
                IFS='|' read -r testName queryParams <<< "$testCase"
                if ! compareResults "$testName"; then
                    ((differences++))
                fi
            done

            echo
            if [ $differences -eq 0 ]; then
                echo -e "${GREEN}✓ All tests match! Optimization preserved functionality.${NC}"
            else
                echo -e "${RED}✗ Found $differences differences${NC}"
            fi

            compareTiming
            ;;
        "full")
            echo "Running full test suite..."
            runTestSuite "BEFORE" "$beforeDir"
            echo
            echo -e "${YELLOW}Now make your code changes and run: $0 after${NC}"
            ;;
        *)
            echo "Usage: $0 {before|after|compare|full}"
            echo
            echo "  before  - Run tests before code changes"
            echo "  after   - Run tests after code changes"
            echo "  compare - Compare before/after results"
            echo "  full    - Run before tests and show next steps"
            echo
            echo "Example workflow:"
            echo "  1. $0 before"
            echo "  2. Make your code changes"
            echo "  3. $0 after"
            echo "  4. $0 compare"
            ;;
    esac
}

main "$@"
