#! /bin/csh -f

if ($#argv < 2) then
    echo "This script tests the correctness of parsing (not solving) for IntelSAT."
    echo "It runs the solver with a conflict threshold of 0 to prevent solving, and checks for parsing errors."
    echo "Returns 0 on success and other values on failure."
    echo ""
    echo "Parameters: <intel_sat_executable> <file> [Any additional parameters]"
    echo "Example: scripts/test_parsing_only.csh intel_sat_solver_test_static regression_instances/regr_1.cnf"
    exit 140
endif

set tmpdir = "/tmp/$USER/test_parsing_`hostname`_$$"
mkdir -p $tmpdir
set out_file = $tmpdir/parse_test.out

echo "Testing parsing of: $argv[2]"

# Create a temporary file with 'oc 0' prepended as an early command to set conflict threshold to 0
# This will prevent any solve calls from actually solving (they'll exit immediately due to conflict threshold 0)
# However, for files with incremental queries ('s' commands), we can't easily prevent those solves
# So we'll append 'oc 0' before any potential automatic solve at the end
set temp_input = $tmpdir/temp_input
set input_ext = `echo $argv[2] | sed 's/.*\.//'`
set temp_input_file = "$temp_input.$input_ext"

cat $argv[2] > $temp_input_file

# Check if the file already ends with a newline, and if not, add one before appending 'oc 0'
set last_char = `tail -c 1 $temp_input_file | od -An -tu1 2>/dev/null`
if ($last_char != "10" && $last_char != "") then
    echo "" >> $temp_input_file
endif
# Add 'oc 0' at the end to ensure any automatic solve call at the end uses conflict threshold 0
echo "oc 0" >> $temp_input_file

# Note: For incremental instances with 's' commands before the 'oc 0', those solves will still execute
# But this is acceptable - we're primarily testing parsing, and parse errors will be caught regardless

# Run the solver - it should parse the file and then immediately exit due to conflict threshold 0
# The 'oc 0' command sets the conflict threshold to 0, which should cause immediate exit after parsing
echo "Command: $argv[1] $temp_input_file $argv[3-]"
echo "$argv[1] $temp_input_file $argv[3-] >& $out_file"
$argv[1] $temp_input_file $argv[3-] >& $out_file
set parse_status = $status

echo "========================================="
echo "Parsing test output:"
cat $out_file
echo "========================================="

# Check if the file was parsed successfully
# Successful parsing should result in:
# 1. The solver starting (we see "c Intel(R) SAT Solver started")
set solver_started = `grep -c "c Intel(R) SAT Solver started" $out_file`
if ($solver_started == 0) then
    echo "ERROR: Solver did not start - possible issue with executable or input file"
    rm -rf $tmpdir
    exit 1
endif

# Check for parsing errors (errors that occur during file reading/clause parsing)
# These are errors that happen before solving starts
set has_parse_error = `grep -c "c topor_tool ERROR" $out_file`
if ($has_parse_error != 0) then
    echo "ERROR: Parsing errors detected!"
    grep "c topor_tool ERROR" $out_file
    rm -rf $tmpdir
    exit 1
endif

# Check for file access errors
set has_file_error = `grep -c "doesn't exist\|couldn't open\|couldn't reopen" $out_file`
if ($has_file_error != 0) then
    echo "ERROR: File access error detected!"
    grep "doesn't exist\|couldn't open\|couldn't reopen" $out_file
    rm -rf $tmpdir
    exit 1
endif

# For MaxSAT files, check if MaxSAT mode was detected correctly
set is_wcnf = `echo $argv[2] | grep -c "\.wcnf$"`
set has_maxsat_flag = `echo $argv[3-] | grep -c "\-M 1"`
if ($is_wcnf != 0 || $has_maxsat_flag != 0) then
    set maxsat_mode_detected = `grep -c "c running in MaxSAT mode" $out_file`
    if ($maxsat_mode_detected == 0) then
        echo "WARNING: Expected MaxSAT mode but it was not detected"
        echo "  This may indicate a parsing issue with MaxSAT mode detection"
    else
        echo "MaxSAT mode correctly detected"
    endif
else
    set sat_mode_detected = `grep -c "c running in SAT mode" $out_file`
    if ($sat_mode_detected == 0) then
        set maxsat_mode_detected = `grep -c "c running in MaxSAT mode" $out_file`
        if ($maxsat_mode_detected != 0) then
            echo "WARNING: Expected SAT mode but MaxSAT mode was detected instead"
        endif
    endif
endif

# Check for specific parsing-related errors that might not be caught above
# These include: literal out of range, clause parsing errors, format errors
set has_literal_error = `grep -c "literal.*too big\|too small\|couldn't translate.*vector of literals" $out_file`
if ($has_literal_error != 0) then
    echo "ERROR: Literal parsing errors detected!"
    grep "literal.*too big\|too small\|couldn't translate.*vector of literals" $out_file
    rm -rf $tmpdir
    exit 1
endif

set has_format_error = `grep -c "couldn't parse.*p-line\|should not contain a p line\|second line starting with p" $out_file`
if ($has_format_error != 0) then
    echo "ERROR: Format parsing errors detected!"
    grep "couldn't parse.*p-line\|should not contain a p line\|second line starting with p" $out_file
    rm -rf $tmpdir
    exit 1
endif

# Check for MaxSAT-specific parsing errors
if ($is_wcnf != 0 || $has_maxsat_flag != 0) then
    set has_weight_error = `grep -c "Clause weight range violated\|Cumulative weight limit exceeded\|failed to assign relaxation literal" $out_file`
    if ($has_weight_error != 0) then
        echo "ERROR: MaxSAT weight parsing errors detected!"
        grep "Clause weight range violated\|Cumulative weight limit exceeded\|failed to assign relaxation literal" $out_file
        rm -rf $tmpdir
        exit 1
    endif
endif

# If we got here, parsing appears to have completed successfully
# Note: We expect to see timeout/conflict threshold exit, but that's fine - we're testing parsing, not solving
echo "Parsing test PASSED: No parsing errors detected"
echo "  File was successfully parsed (solver started, no parsing errors found)"
rm -rf $tmpdir
exit 0

