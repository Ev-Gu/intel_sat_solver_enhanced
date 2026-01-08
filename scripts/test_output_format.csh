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

# Run the solver
echo "$argv[1-] >& $out_file"
$argv[1-] >& $out_file
set solver_status = $status

# Check if solver ran successfully (non-zero exit is OK if it's UNSAT or timeout)
# But we need to capture output first

set has_error = 0
set error_msgs = ()

# Check if file exists and has content
if ( ! -f "$out_file" ) then
    echo "ERROR: Solver output file not found!"
    exit 1
endif

set file_size = `wc -c < "$out_file" | tr -d ' '`
if ( $file_size == 0 ) then
    echo "ERROR: Solver output file is empty!"
    exit 1
endif

echo "Output format validation:"

# 1. Check for result line (must be exactly "s SATISFIABLE" or "s UNSATISFIABLE")
set has_sat_line = `grep -c "^s SATISFIABLE" "$out_file"`
set has_unsat_line = `grep -c "^s UNSATISFIABLE" "$out_file"`

if ( $has_sat_line == 0 && $has_unsat_line == 0 ) then
    set has_error = 1
    set error_msgs = ($error_msgs "ERROR: Missing result line. Expected 's SATISFIABLE' or 's UNSATISFIABLE'")
else
    # Check that there's exactly one result line
    set result_count = `grep -c "^s " "$out_file"`
    if ( $result_count != 1 ) then
        set has_error = 1
        set error_msgs = ($error_msgs "ERROR: Expected exactly one result line starting with 's ', found $result_count")
    endif
    
    # Check that the result line is properly formatted (no extra text)
    set malformed_count = `grep "^s SATISFIABLE" "$out_file" | grep -v "^s SATISFIABLE$" | wc -l`
    @ malformed_count = $malformed_count + `grep "^s UNSATISFIABLE" "$out_file" | grep -v "^s UNSATISFIABLE$" | wc -l`
    
    if ( $malformed_count > 0 ) then
        set has_error = 1
        set error_msgs = ($error_msgs "ERROR: Result line contains extra text. Must be exactly 's SATISFIABLE' or 's UNSATISFIABLE'")
    endif
endif

# 2. Check for model line format if SATISFIABLE
if ( $has_sat_line > 0 ) then
    # Find all model lines
    set model_lines = `grep "^v" "$out_file"`
    
    if ( "$model_lines" == "" ) then
        # No model line is acceptable for some solvers/configurations, but warn
        echo "WARNING: SATISFIABLE but no model line found"
    else
        # Check each model line
        set model_line_count = 0
        foreach line (`grep "^v" "$out_file"`)
            @ model_line_count = $model_line_count + 1
            
            # Model line should start with "v " (space after v)
            set line_start = `echo "$line" | sed 's/^\(v[ ]\).*/\1/'`
            if ( "$line_start" != "v " ) then
                set has_error = 1
                set error_msgs = ($error_msgs "ERROR: Model line must start with 'v ' (v followed by space): $line")
            else
                # Extract the literals part (everything after "v ")
                set literals = `echo "$line" | sed 's/^v //'`
                
                # Check that literals are valid (integers and optionally 0 at the end)
                # Remove trailing 0 if present and check remaining are valid integers
                set clean_literals = `echo "$literals" | sed 's/[ ]*0[ ]*$//'`
                foreach lit ($clean_literals)
                    # Check if it's a valid integer (including negative)
                    set is_valid = `echo "$lit" | grep -c "^-*[0-9][0-9]*$"`
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
set is_wcnf = `echo "$argv[2]" | grep -c "\.wcnf$"`
set has_maxsat_flag = `echo "$argv[1-]" | grep -c "\-M 1"`

if ( $is_wcnf != 0 || $has_maxsat_flag != 0 ) then
    echo "MaxSAT instance detected - checking MaxSAT output format..."
    
    if ( $has_sat_line > 0 ) then
        # Check for objective value line ("o <number>")
        set obj_lines = `grep "^o " "$out_file"`
        
        if ( "$obj_lines" == "" ) then
            set has_error = 1
            set error_msgs = ($error_msgs "ERROR: MaxSAT SATISFIABLE output missing objective value line ('o <number>')")
        else
            # Check each objective line format
            foreach obj_line ($obj_lines)
                # Should be "o " followed by a number
                set obj_match = `echo "$obj_line" | grep -c "^o [0-9][0-9]*$"`
                if ( $obj_match == 0 ) then
                    # Check if it's "o " followed by a number and possibly extra text
                    set has_number = `echo "$obj_line" | sed 's/^o //' | grep -c "^[0-9][0-9]*"`
                    if ( $has_number == 0 ) then
                        set has_error = 1
                        set error_msgs = ($error_msgs "ERROR: Invalid objective line format (must be 'o <number>'): $obj_line")
                    else
                        # Has number but might have extra text - check for just whitespace after number
                        set obj_value = `echo "$obj_line" | sed 's/^o //' | awk '{print $1}'`
                        set full_line = `echo "$obj_line" | sed 's/^o //'`
                        if ( "$obj_value" != "$full_line" ) then
                            set has_error = 1
                            set error_msgs = ($error_msgs "ERROR: Objective line contains extra text after number: $obj_line")
                        endif
                    endif
                endif
            end
            
            # Check that there's at most one objective line
            set obj_count = `grep -c "^o " "$out_file"`
            if ( $obj_count > 1 ) then
                set has_error = 1
                set error_msgs = ($error_msgs "ERROR: Found $obj_count objective lines, expected at most 1")
            endif
        endif
    endif
endif

# 4. Check for invalid output lines (lines that shouldn't appear in DIMACS format)
# Lines should only be: comments (c ...), result (s ...), model (v ...), objective (o ...), or empty
set invalid_lines = `grep -v "^c" "$out_file" | grep -v "^s " | grep -v "^v" | grep -v "^o " | grep -v "^[ \t]*$" | grep -v "^$"`
if ( "$invalid_lines" != "" ) then
    # Check if any invalid lines are actually error messages (these are acceptable)
    set error_lines = `echo "$invalid_lines" | grep -i "error\|fatal\|assertion\|segmentation"`
    set non_error_invalid = `echo "$invalid_lines" | grep -vi "error\|fatal\|assertion\|segmentation"`
    
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
    set result_line_num = `grep -n "^s " "$out_file" | head -n 1 | cut -d: -f1`
    
    if ( $has_sat_line > 0 ) then
        # Check model lines come after result
        set first_model_line = `grep -n "^v" "$out_file" | head -n 1 | cut -d: -f1`
        if ( "$first_model_line" != "" && $first_model_line < $result_line_num ) then
            set has_error = 1
            set error_msgs = ($error_msgs "ERROR: Model line appears before result line (line $first_model_line vs $result_line_num)")
        endif
        
        # Check objective line comes after result (for MaxSAT)
        if ( $is_wcnf != 0 || $has_maxsat_flag != 0 ) then
            set first_obj_line = `grep -n "^o " "$out_file" | head -n 1 | cut -d: -f1`
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
    set has_model = `grep -c "^v" "$out_file"`
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
    foreach msg ($error_msgs)
        echo "$msg"
    end
    echo ""
    echo "Solver output:"
    cat "$out_file"
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

