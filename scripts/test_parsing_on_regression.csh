#! /bin/csh -f

if ($#argv < 1) then
	echo "Test parsing correctness of the given IntelSAT executable on regression instances"
	echo "Parameters: intel_sat_executable [Any additional parameters (optional)]"
	exit 140
endif

set script_dir = `dirname $0`

set test_parsing = "$script_dir/test_parsing_only.csh"
set regr = "$script_dir/../regression_instances"
set topor = $1
if (! -e $topor) then
	echo "intel_sat_executable doesn't exist at $topor. Exiting"
	echo "ERROR"
	exit 130
endif
echo "intel_sat_executable was set to $topor"

set inputparams = "$argv[2-]"
echo "Additional parameters: $inputparams"

# Test all .cnf files
foreach f (`ls -rS $regr/*.cnf 2>/dev/null`)
	echo ""
	echo "========================================"
	echo "Testing parsing of: $f"
	echo "========================================"
	$test_parsing $topor $f $inputparams
	set r = $status
	if ($r != 0) then
		echo "ERROR: Parsing test failed for $f"
		exit $r
	else
		echo "OK: Parsing test passed for $f"
	endif
end

# Test all .wcnf files (MaxSAT instances)
foreach f (`ls -rS $regr/*.wcnf 2>/dev/null`)
	echo ""
	echo "========================================"
	echo "Testing parsing of MaxSAT file: $f"
	echo "========================================"
	$test_parsing $topor $f -M 1 $inputparams
	set r = $status
	if ($r != 0) then
		echo "ERROR: Parsing test failed for $f"
		exit $r
	else
		echo "OK: Parsing test passed for $f"
	endif
end

echo ""
echo "========================================"
echo "All parsing tests PASSED"
echo "========================================"

