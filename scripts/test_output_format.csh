#! /bin/csh -f

if ($#argv < 2) then
    echo "This script tests the output format correctness of IntelSAT."
    echo "It validates that the solver output conforms to DIMACS format standards."
    echo "Returns 0 on success and other values on failure."
    echo ""
    echo "Parameters: <intel_sat_executable> <file> [Any additional parameters]"
    echo "Example: scripts/test_output_format.csh intel_sat_solver_test_static regression_instances/regr_1.cnf"
    exit 140
endif

set tmpdir = "/tmp/$USER/test_output_format_`hostname`_$$"
mkdir -p $tmpdir
set out_file = $tmpdir/output_format_test.out

echo "Testing output format for: $argv[2]"
echo "Running solver..."

# Run the solver with timeout and conflict threshold limit
# Add conflict threshold to prevent solver from running too long on difficult instances
set solver_cmd = "$argv[1]"
set solver_args = "$argv[2-]"

# Check if conflict threshold is already set
set has_conflict_threshold = (`sh -c 'echo "$1" | grep -c "/topor_tool/conflict_threshold\\|oc "' sh "$solver_args"`)
set has_conflict_threshold = "$has_conflict_threshold"

# Add conflict threshold if not already set (limit to 10000 conflicts for format testing)
if ( $has_conflict_threshold == 0 ) then
    set solver_args = "$solver_args /topor_tool/conflict_threshold 10000"
endif

echo "$solver_cmd $solver_args >& $out_file"

# Try to use timeout command if available
set timeout_cmd = (`sh -c 'which timeout 2>/dev/null || which gtimeout 2>/dev/null || echo ""'`)
set timeout_cmd = "$timeout_cmd"

if ( "$timeout_cmd" != "" ) then
    # Use timeout if available (30 seconds should be enough for format testing)
    $timeout_cmd 30 $solver_cmd $solver_args >& $out_file
    set solver_status = $status
    # Timeout returns 124 (Linux) or 143 (some systems) if it times out
    if ( $solver_status == 124 || $solver_status == 143 ) then
        echo "WARNING: Solver timed out after 30 seconds"
    endif
else
    # No timeout available - run in background and kill after timeout
    # This is a fallback for systems without timeout command
    ($solver_cmd $solver_args >& $out_file) &
    set solver_pid = $!
    
    # Wait up to 30 seconds
    set waited = 0
    while ( $waited < 30 )
        # Check if process is still running
        set pid_exists = (`sh -c 'kill -0 "$1" 2>/dev/null && echo "1" || echo "0"' sh "$solver_pid"`)
        set pid_exists = "$pid_exists"
        if ( $pid_exists == 0 ) then
            # Process finished
            wait $solver_pid
            set solver_status = $status
            break
        endif
        sleep 1
        @ waited = $waited + 1
    end
    
    # If still running, kill it
    if ( $waited >= 30 ) then
        kill $solver_pid 2>/dev/null
        wait $solver_pid 2>/dev/null
        set solver_status = 124
        echo "WARNING: Solver timed out after 30 seconds (killed)"
    endif
endif

# Check if solver ran successfully (non-zero exit is OK if it's UNSAT or timeout)
# But we need to capture output first

set has_error = 0
set error_msgs = ()

# Check if file exists and has content
if ( ! -f "$out_file" ) then
    echo "ERROR: Solver output file not found!"
    exit 1
endif

set file_size = (`sh -c 'wc -c < "$1" | tr -d " "' sh "$out_file"`)
set file_size = "$file_size"
if ( $file_size == 0 ) then
    echo "ERROR: Solver output file is empty!"
    exit 1
endif

echo "Output format validation:"

# 1. Check for result line (must be exactly "s SATISFIABLE" or "s UNSATISFIABLE")
set has_sat_line = (`sh -c 'grep -c "^s SATISFIABLE" "$1"' sh "$out_file"`)
set has_sat_line = "$has_sat_line"
set has_unsat_line = (`sh -c 'grep -c "^s UNSATISFIABLE" "$1"' sh "$out_file"`)
set has_unsat_line = "$has_unsat_line"

if ( $has_sat_line == 0 && $has_unsat_line == 0 ) then
    set has_error = 1
    set error_msgs = ($error_msgs "ERROR: Missing result line. Expected 's SATISFIABLE' or 's UNSATISFIABLE'")
else
    # Check that there's exactly one result line
    set result_count = (`sh -c 'grep -c "^s " "$1"' sh "$out_file"`)
    set result_count = "$result_count"
    if ( $result_count != 1 ) then
        set has_error = 1
        set error_msgs = ($error_msgs "ERROR: Expected exactly one result line starting with 's ', found $result_count")
    endif
    
    # Check that the result line is properly formatted (no extra text)
    set malformed_count = (`sh -c 'grep "^s SATISFIABLE" "$1" | grep -v "^s SATISFIABLE$" | wc -l' sh "$out_file"`)
    set malformed_count = "$malformed_count"
    set unsat_malformed = (`sh -c 'grep "^s UNSATISFIABLE" "$1" | grep -v "^s UNSATISFIABLE$" | wc -l' sh "$out_file"`)
    set unsat_malformed = "$unsat_malformed"
    @ malformed_count = $malformed_count + $unsat_malformed
    
    if ( $malformed_count > 0 ) then
        set has_error = 1
        set error_msgs = ($error_msgs "ERROR: Result line contains extra text. Must be exactly 's SATISFIABLE' or 's UNSATISFIABLE'")
    endif
endif

# 2. Check for model line format if SATISFIABLE
if ( $has_sat_line > 0 ) then
    # Find all model lines
    setenv OUT_FILE "$out_file"
    set model_lines = (`sh -c 'grep "^v" "$OUT_FILE"'`)
    unsetenv OUT_FILE
    
    if ( "$model_lines" == "" ) then
        # No model line is acceptable for some solvers/configurations, but warn
        echo "WARNING: SATISFIABLE but no model line found"
    else
        # Check each model line
        set model_line_count = 0
        setenv OUT_FILE "$out_file"
        foreach line (`sh -c 'grep "^v" "$OUT_FILE"'`)
            unsetenv OUT_FILE
            @ model_line_count = $model_line_count + 1
            
            # Model line should start with "v " (space after v)
            set line_start = (`sh -c 'echo "$1" | sed "s/^\\(v[ ]\\).*/\\1/"' sh "$line"`)
            set line_start = "$line_start"
            if ( "$line_start" != "v " ) then
                set has_error = 1
                set error_msgs = ($error_msgs "ERROR: Model line must start with 'v ' (v followed by space): $line")
            else
                # Extract the literals part (everything after "v ")
                set literals = (`sh -c 'echo "$1" | sed "s/^v //"' sh "$line"`)
                set literals = "$literals"
                
                # Check that literals are valid (integers and optionally 0 at the end)
                # Remove trailing 0 if present and check remaining are valid integers
                set clean_literals = (`sh -c 'echo "$1" | sed "s/[ ]*0[ ]*$//"' sh "$literals"`)
                set clean_literals = "$clean_literals"
                foreach lit ($clean_literals)
                    # Check if it's a valid integer (including negative)
                    set is_valid = (`sh -c 'echo "$1" | grep -c "^-*[0-9][0-9]*$"' sh "$lit"`)
                    set is_valid = "$is_valid"
                    if ( $is_valid == 0 ) then
                        set has_error = 1
                        set error_msgs = ($error_msgs "ERROR: Invalid literal in model line (must be an integer): $line")
                        break
                    endif
                end
            endif
        end
    endif
endif

# 3. Check for MaxSAT-specific output format
set is_wcnf = (`sh -c 'echo "$1" | grep -c "\\.wcnf$"' sh "$argv[2]"`)
set is_wcnf = "$is_wcnf"
set has_maxsat_flag = (`sh -c 'echo "$1" | grep -c "\\-M 1"' sh "$argv[1-]"`)
set has_maxsat_flag = "$has_maxsat_flag"

if ( $is_wcnf != 0 || $has_maxsat_flag != 0 ) then
    echo "MaxSAT instance detected - checking MaxSAT output format..."
    
    if ( $has_sat_line > 0 ) then
        # Check for objective value line ("o <number>")
        setenv OUT_FILE "$out_file"
        set obj_lines = (`sh -c 'grep "^o " "$OUT_FILE"'`)
        unsetenv OUT_FILE
        
        if ( "$obj_lines" == "" ) then
            set has_error = 1
            set error_msgs = ($error_msgs "ERROR: MaxSAT SATISFIABLE output missing objective value line ('o <number>')")
        else
            # Check each objective line format
            foreach obj_line ($obj_lines)
                # Should be "o " followed by a number
                set obj_match = (`sh -c 'echo "$1" | grep -c "^o [0-9][0-9]*$"' sh "$obj_line"`)
                set obj_match = "$obj_match"
                if ( $obj_match == 0 ) then
                    # Check if it's "o " followed by a number and possibly extra text
                    set has_number = (`sh -c 'echo "$1" | sed "s/^o //" | grep -c "^[0-9][0-9]*"' sh "$obj_line"`)
                    set has_number = "$has_number"
                    if ( $has_number == 0 ) then
                        set has_error = 1
                        set error_msgs = ($error_msgs "ERROR: Invalid objective line format (must be 'o <number>'): $obj_line")
                    else
                        # Has number but might have extra text - check for just whitespace after number
                        set obj_value = (`sh -c 'echo "$1" | sed "s/^o //" | awk "{print \\$1}"' sh "$obj_line"`)
                        set obj_value = "$obj_value"
                        set full_line = (`sh -c 'echo "$1" | sed "s/^o //"' sh "$obj_line"`)
                        set full_line = "$full_line"
                        if ( "$obj_value" != "$full_line" ) then
                            set has_error = 1
                            set error_msgs = ($error_msgs "ERROR: Objective line contains extra text after number: $obj_line")
                        endif
                    endif
                endif
            end
            
            # Check that there's at most one objective line
            set obj_count = (`sh -c 'grep -c "^o " "$1"' sh "$out_file"`)
            set obj_count = "$obj_count"
            if ( $obj_count > 1 ) then
                set has_error = 1
                set error_msgs = ($error_msgs "ERROR: Found $obj_count objective lines, expected at most 1")
            endif
        endif
    endif
endif

# 4. Check for invalid output lines (lines that shouldn't appear in DIMACS format)
# Lines should only be: comments (c ...), result (s ...), model (v ...), objective (o ...), or empty
setenv OUT_FILE "$out_file"
set invalid_lines = (`sh -c 'grep -v "^c" "$OUT_FILE" | grep -v "^s " | grep -v "^v" | grep -v "^o " | grep -v "^[ \\t]*$" | grep -v "^$"'`)
unsetenv OUT_FILE
if ( "$invalid_lines" != "" ) then
    # Check if any invalid lines are actually error messages (these are acceptable)
    set error_lines = (`sh -c 'echo "$1" | grep -i "error\\|fatal\\|assertion\\|segmentation"' sh "$invalid_lines"`)
    set error_lines = "$error_lines"
    set non_error_invalid = (`sh -c 'echo "$1" | grep -vi "error\\|fatal\\|assertion\\|segmentation"' sh "$invalid_lines"`)
    set non_error_invalid = "$non_error_invalid"
    
    if ( "$non_error_invalid" != "" ) then
        set has_error = 1
        set error_msgs = ($error_msgs "ERROR: Found invalid output lines (not comments, result, model, objective, or empty):")
        foreach line ($non_error_invalid)
            set error_msgs = ($error_msgs "  $line")
        end
    endif
endif

# 5. Check line order (result should come before model/objective)
if ( $has_sat_line > 0 || $has_unsat_line > 0 ) then
    set result_line_num = (`sh -c 'grep -n "^s " "$1" | head -n 1 | cut -d: -f1' sh "$out_file"`)
    set result_line_num = "$result_line_num"
    
    if ( $has_sat_line > 0 ) then
        # Check model lines come after result
        set first_model_line = (`sh -c 'grep -n "^v" "$1" | head -n 1 | cut -d: -f1' sh "$out_file"`)
        set first_model_line = "$first_model_line"
        if ( "$first_model_line" != "" && $first_model_line < $result_line_num ) then
            set has_error = 1
            set error_msgs = ($error_msgs "ERROR: Model line appears before result line (line $first_model_line vs $result_line_num)")
        endif
        
        # Check objective line comes after result (for MaxSAT)
        if ( $is_wcnf != 0 || $has_maxsat_flag != 0 ) then
            set first_obj_line = (`sh -c 'grep -n "^o " "$1" | head -n 1 | cut -d: -f1' sh "$out_file"`)
            set first_obj_line = "$first_obj_line"
            if ( "$first_obj_line" != "" && $first_obj_line < $result_line_num ) then
                set has_error = 1
                set error_msgs = ($error_msgs "ERROR: Objective line appears before result line (line $first_obj_line vs $result_line_num)")
            endif
        endif
    endif
endif

# 6. Check for duplicate result lines (already checked above, but be explicit)
# Already done in check 1

# 7. Validate that if UNSATISFIABLE, there's no model
if ( $has_unsat_line > 0 ) then
    set has_model = (`sh -c 'grep -c "^v" "$1"' sh "$out_file"`)
    set has_model = "$has_model"
    if ( $has_model > 0 ) then
        set has_error = 1
        set error_msgs = ($error_msgs "ERROR: UNSATISFIABLE result should not have model lines")
    endif
endif

# Report results
echo ""
if ( $has_error != 0 ) then
    echo "========================================="
    echo "OUTPUT FORMAT VALIDATION FAILED"
    echo "========================================="
    # Print error messages - handle array properly to avoid splitting
    foreach msg ($error_msgs)
        echo "$msg"
    end
    echo ""
    echo "Solver output (last 20 lines):"
    tail -n 20 "$out_file"
    echo ""
    echo "Full solver output saved in: $out_file"
    rm -rf $tmpdir
    exit 1
else
    echo "========================================="
    echo "OUTPUT FORMAT VALIDATION PASSED"
    echo "========================================="
    echo "✓ Result line format: OK"
    if ( $has_sat_line > 0 ) then
        echo "✓ Model line format: OK"
    endif
    if ( ($is_wcnf != 0 || $has_maxsat_flag != 0) && $has_sat_line > 0 ) then
        echo "✓ Objective line format: OK"
    endif
    echo "✓ Line order: OK"
    echo "✓ No invalid lines: OK"
    rm -rf $tmpdir
    exit 0
endif

