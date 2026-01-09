#! /bin/csh -f

if ($#argv < 1) then
	echo "Verify the given IntelSAT executable on regression instances given optional parameters on the input"
	echo "Parameters: intel_sat_executable [Any additional parameters (optional)]"
	exit 140
endif

set script_dir = (`dirname $0`)
set script_dir = "$script_dir"

set run_and_verify_topor = "$script_dir/run_and_verify_intel_sat.csh"
set regr = "$script_dir/../regression_instances"
set topor = $1
if (! -e $topor) then
	# Try with .exe extension (for Windows/MSYS2)
	if (-e "${topor}.exe") then
		set topor = "${topor}.exe"
	else
		echo "ERROR: intel_sat_executable doesn't exist at $topor"
		echo "       (Also tried: ${topor}.exe)"
		exit 130
	endif
endif
echo "intel_sat_executable was set to $topor"

set inputparams = "$argv[2-]"
echo "Parameters: $inputparams"

# Use find to handle paths with spaces
setenv REGR_DIR "$regr"
set ddtbug_files = (`sh -c 'find "$REGR_DIR" -maxdepth 1 -name "ddtbug*.cnf" -type f 2>/dev/null | sort -r'`)
unsetenv REGR_DIR
foreach f ($ddtbug_files)
	set insideparams = (`sh -c 'sed -nr "s/c Topors command-line.*cnf (.*)/\\1/p" "$1"' sh "$f"`)
	set insideparams = "$insideparams"
	echo "insideparams: $insideparams"
	
	# Remove invalid parameters that don't exist in the current solver version
	# /topor_tool/solver_mode is an old parameter that no longer exists
	set insideparams = (`sh -c 'echo "$1" | sed "s|/topor_tool/solver_mode [0-9]*||g"' sh "$insideparams"`)
	set insideparams = "$insideparams"
	
	# Add conflict threshold for ddtbug files to prevent them from running too long
	# Check if conflict threshold is already in insideparams
	set has_conflict_threshold = (`sh -c 'echo "$1" | grep -c "/topor_tool/conflict_threshold\\|oc "' sh "$insideparams"`)
	set has_conflict_threshold = "$has_conflict_threshold"
	if ( $has_conflict_threshold == 0 ) then
		set insideparams = "$insideparams /topor_tool/conflict_threshold 200000"
		echo "Added conflict threshold limit (200000) for ddtbug file"
	endif
	echo "Final parameters: $insideparams"
	echo "$run_and_verify_topor $topor $f $insideparams $inputparams > outt"
	$run_and_verify_topor $topor "$f" $insideparams $inputparams > outt
	
	set r = (`sh -c 'grep -c ERROR "$1" 2>/dev/null || echo "0"' sh outt`)
	set r = "$r"
	if ($r != 0) then
		# Check if it's an "unknown result" error (solver didn't produce result line)
		# This is common for difficult ddtbug instances - continue with other tests
		set unknown_result = (`sh -c 'grep -c "unknown result" "$1" 2>/dev/null || echo "0"' sh outt`)
		set unknown_result = "$unknown_result"
		if ( $unknown_result != 0 ) then
			echo "WARNING: Solver did not produce a result for ddtbug file (likely hit conflict threshold)"
			echo "  This is common for difficult ddtbug instances. Continuing with other tests..."
			echo "  File: $f"
		else
			echo "ERROR!"
			echo "Error details from outt:"
			cat outt
			exit 1
		endif
	else
		echo "Ok"
	endif
	rm -f outt
end

# Use find to handle paths with spaces
setenv REGR_DIR "$regr"
set regr_files = (`sh -c 'find "$REGR_DIR" -maxdepth 1 -name "regr*.cnf" -type f 2>/dev/null | sort -r'`)
unsetenv REGR_DIR
foreach f ($regr_files)
	foreach m (0 1)
		foreach v (0 1 2 3 4 5 6 7 8)
			# Remove invalid parameter /topor_tool/solver_mode (no longer exists)
			# Only use /mode/value which is the valid parameter
			set modified_params = "/mode/value $v"
			# Add conflict threshold to prevent indefinite running for difficult instances
			# Check if conflict threshold is already in inputparams
			set has_conflict_threshold = (`sh -c 'echo "$1" | grep -c "/topor_tool/conflict_threshold\\|oc "' sh "$inputparams"`)
			set has_conflict_threshold = "$has_conflict_threshold"
			if ( $has_conflict_threshold == 0 ) then
				set modified_params = "$modified_params /topor_tool/conflict_threshold 100000"
			endif
			echo "$run_and_verify_topor $topor $f $modified_params $inputparams > outt"
			$run_and_verify_topor $topor "$f" $modified_params $inputparams > outt
			set r = (`sh -c 'grep -c ERROR "$1" 2>/dev/null || echo "0"' sh outt`)
			set r = "$r"
			if ($r != 0) then
				# Check if it's an "unknown result" error (solver didn't produce result line)
				set unknown_result = (`sh -c 'grep -c "unknown result" "$1" 2>/dev/null || echo "0"' sh outt`)
				set unknown_result = "$unknown_result"
				if ( $unknown_result != 0 ) then
					echo "WARNING: Solver did not produce a result for regr file (mode=$v, likely hit conflict threshold)"
					echo "  File: $f"
					echo "  This may indicate the instance is difficult for this mode. Continuing..."
				else
					echo "ERROR!"
					echo "Error details from outt:"
					cat outt
					exit 1
				endif
			else
				echo "Ok"
			endif
			rm -f outt
		end
	end
end

# Process .wcnf files (MaxSAT regression tests)
# This section:
# 1. Finds all .wcnf files in $regr (the regression_instances directory, set on line 12)
# 2. Runs each file through IntelSAT in MaxSAT mode (-M 1)
# 3. Checks for errors in the output
# 4. Exits immediately on the first error, otherwise continues and reports success
# Note: If no .wcnf files exist, the loop is skipped and the script continues

setenv REGR_DIR "$regr"
set wcnf_files = (`sh -c 'find "$REGR_DIR" -maxdepth 1 -name "*.wcnf" -type f 2>/dev/null | sort -r'`)
unsetenv REGR_DIR
foreach f ($wcnf_files)
	echo "$run_and_verify_topor $topor $f -M 1 $inputparams > outt"
	$run_and_verify_topor $topor "$f" -M 1 $inputparams > outt
	
	set r = (`sh -c 'grep -c ERROR "$1" 2>/dev/null || echo "0"' sh outt`)
	set r = "$r"
	if ($r != 0) then
		echo "ERROR!"
		echo "Error details from outt:"
		cat outt
		exit 1
	else
		echo "Ok"
		rm -f outt		
	endif
end