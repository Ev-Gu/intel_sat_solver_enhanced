#! /bin/csh -f

if ($#argv < 1) then
	echo "Verify the given IntelSAT executable on regression instances given optional parameters on the input"
	echo "Parameters: intel_sat_executable [Any additional parameters (optional)]"
	exit 140
endif

set script_dir = `dirname $0`

set run_and_verify_topor = "$script_dir/run_and_verify_intel_sat.csh"
set regr = "$script_dir/../regression_instances"
set topor = $1
if (! -e $topor) then
	echo "intel_sat_executable doesn't exist at $topor. Exiting"
	echo "ERROR"
	exit 130
endif
echo "intel_sat_executable was set to $topor"

set inputparams = "$argv[2-]"
echo "Parameters: $inputparams"

foreach f (`ls -rS $regr/ddtbug*.cnf`)
	set insideparams = `sed -nr "s/c Topors command-line.*cnf (.*)/\1/p" $f`
	echo "insideparams: $insideparams"
	echo "$run_and_verify_topor $topor $f $insideparams $inputparams > outt"
	$run_and_verify_topor $topor $f $insideparams $inputparams > outt
	
	set r = `grep -c ERROR outt`
	if ($r != 0) then
		echo "ERROR!"
		exit
	else
		echo "Ok"
		rm outt		
	endif
end

foreach f (`ls -rS $regr/regr*.cnf`)
	foreach m (0 1)
		foreach v (0 1 2 3 4 5 6 7 8)
			set modified_params = "/topor_tool/solver_mode $m /mode/value $v"
			echo "$run_and_verify_topor $topor $f $modified_params $inputparams > outt"
			$run_and_verify_topor $topor $f $modified_params $inputparams > outt
			set r = `grep -c ERROR outt`
			if ($r != 0) then
				echo "ERROR!"
				exit
			else
				echo "Ok"
				rm outt		
			endif
		end
	end
end

# Process .wcnf files (MaxSAT regression tests)
# This section:
# 1. Finds all .wcnf files in $regr (the regression_instances directory, set on line 12)
# 2. Runs each file through IntelSAT in MaxSAT mode (-M 1)
# 3. Checks for errors in the output
# 4. Exits immediately on the first error, otherwise continues and reports success
# Note: If no .wcnf files exist, the loop is skipped (due to 2>/dev/null) and the script continues
#
# WCNF file format (Weighted CNF for MaxSAT):
# - Comments: lines starting with 'c'
# - Hard clauses: lines starting with 'h' followed by literals ending with 0
#   Example: h 1 2 3 4 0
# - Soft clauses: lines starting with a weight (positive integer) followed by literals ending with 0
#   Example: 5 -3 -5 6 7 0  (weight=5, clause: -3 OR -5 OR 6 OR 7)
# Note: WCNF files should NOT contain a 'p wcnf' header line (unlike standard WCNF format)
foreach f (`ls -rS $regr/*.wcnf 2>/dev/null`)
	echo "$run_and_verify_topor $topor $f -M 1 $inputparams > outt"
	$run_and_verify_topor $topor $f -M 1 $inputparams > outt
	
	set r = `grep -c ERROR outt`
	if ($r != 0) then
		echo "ERROR!"
		exit
	else
		echo "Ok"
		rm outt		
	endif
end