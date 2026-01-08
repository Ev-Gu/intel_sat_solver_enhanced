#! /bin/csh -f

if ($#argv < 1) then
    echo "Tests output format correctness on all regression instances."
    echo "Runs test_output_format.csh on each .cnf and .wcnf file in regression_instances/."
    echo ""
    echo "Parameters: intel_sat_executable [Any additional parameters (optional)]"
    echo "Example: scripts/test_output_format_on_regression.csh intel_sat_solver_test_static"
    exit 140
endif

set script_dir = `dirname $0`
set test_output_format = "$script_dir/test_output_format.csh"
set regr = "$script_dir/../regression_instances"
set topor = $1

if (! -e $topor) then
    echo "intel_sat_executable doesn't exist at $topor. Exiting"
    echo "ERROR"
    exit 130
endif

echo "Testing output format on regression instances"
echo "IntelSAT executable: $topor"
echo "Regression directory: $regr"
echo ""

set inputparams = "$argv[2-]"
if ( "$inputparams" != "" ) then
    echo "Additional parameters: $inputparams"
    echo ""
endif

set all_passed = 1
set test_count = 0
set pass_count = 0

# Test all .cnf files
foreach f (`ls $regr/*.cnf 2>/dev/null`)
    @ test_count = $test_count + 1
    set basename_file = `basename $f`
    echo "========================================="
    echo "Testing output format: $basename_file"
    echo "========================================="
    
    if ( "$inputparams" != "" ) then
        $test_output_format $topor "$f" $inputparams
    else
        $test_output_format $topor "$f"
    endif
    
    if ( $status == 0 ) then
        @ pass_count = $pass_count + 1
        echo "PASSED: $basename_file"
    else
        set all_passed = 0
        echo "FAILED: $basename_file"
    endif
    echo ""
end

# Test all .wcnf files
foreach f (`ls $regr/*.wcnf 2>/dev/null`)
    @ test_count = $test_count + 1
    set basename_file = `basename $f`
    echo "========================================="
    echo "Testing output format: $basename_file"
    echo "========================================="
    
    if ( "$inputparams" != "" ) then
        $test_output_format $topor "$f" $inputparams
    else
        $test_output_format $topor "$f"
    endif
    
    if ( $status == 0 ) then
        @ pass_count = $pass_count + 1
        echo "PASSED: $basename_file"
    else
        set all_passed = 0
        echo "FAILED: $basename_file"
    endif
    echo ""
end

# Summary
echo "========================================="
echo "Output Format Test Summary"
echo "========================================="
echo "Total tests: $test_count"
echo "Passed: $pass_count"
echo "Failed: `expr $test_count - $pass_count`"

if ( $all_passed == 1 ) then
    echo ""
    echo "All output format tests PASSED"
    exit 0
else
    echo ""
    echo "Some output format tests FAILED"
    exit 1
endif

