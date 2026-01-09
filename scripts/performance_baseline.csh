#! /bin/csh -f

if ($#argv < 1) then
    echo "Performance Baseline Evaluation Script"
    echo "Establishes baseline runtime and quality metrics for IntelSAT on selected benchmarks"
    echo ""
    echo "Usage: <intel_sat_executable> [output_file]"
    echo "  intel_sat_executable: Path to IntelSAT solver executable"
    echo "  output_file: Optional path to output file (default: baseline_results.txt)"
    echo ""
    echo "Example:"
    echo "  tcsh scripts/performance_baseline.csh ./intel_sat_solver_enhanced_static"
    echo "  tcsh scripts/performance_baseline.csh ./intel_sat_solver_enhanced_static baseline_2026.txt"
    exit 140
endif

set solver = $1
if (! -e $solver) then
    echo "ERROR: Solver executable not found at: $solver"
    exit 130
endif

# Safely get optional output file argument
if ($#argv >= 2) then
    set output_file = "$argv[2]"
else
    set output_file = "baseline_results.txt"
endif

set script_dir = `dirname $0`
set regr_dir = "$script_dir/../regression_instances"

# Create temporary directory for outputs
# Try /tmp first (Unix/MSYS2), then TMP (Windows), then current directory
if (-d /tmp) then
    set tmpdir = "/tmp/$USER/performance_baseline_`hostname`_$$"
else if ($?TMP) then
    set tmpdir = "$TMP/performance_baseline_$$"
else
    set tmpdir = "./performance_baseline_tmp_$$"
endif
mkdir -p $tmpdir
echo "Temporary directory: $tmpdir"

# Select a representative set of benchmarks
# Mix of small, medium files for CNF and all WCNF files
set cnf_benchmarks = (regr_1.cnf regr_5.cnf regr_10.cnf regr_15.cnf regr_20.cnf regr_30.cnf)
set wcnf_benchmarks = (regr_wcnf_1.wcnf regr_wcnf_2.wcnf regr_wcnf_3.wcnf regr_wcnf_4.wcnf)

echo "================================================"
echo "Performance Baseline Evaluation"
echo "================================================"
echo "Solver: $solver"
echo "Output file: $output_file"
echo "Date: `date`"
echo ""
echo "Selected benchmarks:"
echo "  CNF: $cnf_benchmarks"
echo "  WCNF: $wcnf_benchmarks"
echo "================================================"
echo ""

# Initialize output file
echo "Performance Baseline Results" > $output_file
echo "Generated: `date`" >> $output_file
echo "Solver: $solver" >> $output_file
echo "" >> $output_file
echo "Format: Benchmark | Type | Result | Objective | Runtime(s) | Status" >> $output_file
echo "----------------------------------------------------" >> $output_file

set total_tests = 0
set passed_tests = 0
set failed_tests = 0

# Function to run a benchmark and extract metrics
# Process CNF benchmarks
set maxsat_flag = ""
foreach bench ($cnf_benchmarks)
    set benchmark_type = "cnf"
    set bench_file = "$regr_dir/$bench"
        
        if (! -e $bench_file) then
            echo "WARNING: Benchmark not found: $bench_file"
            continue
        endif
        
        @ total_tests = $total_tests + 1
        
        echo ""
        echo "Running: $bench ($benchmark_type)"
        echo "----------------------------------------"
        
        set out_file = "$tmpdir/$bench.out"
        
        # Use time command for timing (more portable)
        # Format: real time (wall time) in seconds
        echo "Command: $solver $bench_file $maxsat_flag"
        
        # Use simple timing with date command
        # Note: date +%s gives integer seconds, so very fast runs (<1s) will show as 0
        set start_time = `date +%s`
        $solver $bench_file $maxsat_flag >& $out_file
        set solver_status = $status
        set end_time = `date +%s`
        @ runtime_sec = $end_time - $start_time
        set runtime = "$runtime_sec"
        
        # If runtime is 0, the solver ran very fast (< 1 second)
        # This is common for small benchmarks
        if ("$runtime" == "" || "$runtime" == "0") then
            set runtime = "<1"
        endif
        
        # Extract result
        set result = "UNKNOWN"
        set is_sat = `grep -c "^s SATISFIABLE" $out_file`
        set is_unsat = `grep -c "^s UNSATISFIABLE" $out_file`
        set is_timeout = `grep -c "^s TIMEOUT" $out_file`
        
        if ($is_sat > 0) then
            set result = "SATISFIABLE"
        else if ($is_unsat > 0) then
            set result = "UNSATISFIABLE"
        else if ($is_timeout > 0) then
            set result = "TIMEOUT"
        else
            set result = "ERROR"
        endif
        
        # Extract objective value for MaxSAT
        set objective = "N/A"
        if ($benchmark_type == "wcnf" && $is_sat > 0) then
            set obj_line = `grep "^o " $out_file | head -n 1`
            if ("$obj_line" != "") then
                set objective = `echo $obj_line | sed 's/^o //'`
            endif
        endif
        
        # Determine status
        set status = "OK"
        if ("$result" == "ERROR" || "$result" == "UNKNOWN") then
            set status = "FAILED"
            @ failed_tests = $failed_tests + 1
            echo "WARNING: Solver did not produce expected output"
            echo "Last 20 lines of output:"
            tail -n 20 $out_file
        else
            @ passed_tests = $passed_tests + 1
        endif
        
        # Print and save results
        printf "  Result: %s\n" $result
        if ($objective != "N/A") then
            printf "  Objective: %s\n" $objective
        endif
        printf "  Runtime: %s seconds\n" $runtime
        printf "  Status: %s\n" $status
        
        # Save to output file
        printf "%-40s | %-4s | %-12s | %-10s | %-10s | %s\n" "$bench" "$benchmark_type" "$result" "$objective" "$runtime" "$status" >> $output_file
    end

# Process WCNF benchmarks
set maxsat_flag = "-M 1"
foreach bench ($wcnf_benchmarks)
    set benchmark_type = "wcnf"
    set bench_file = "$regr_dir/$bench"
    
    if (! -e $bench_file) then
        echo "WARNING: Benchmark not found: $bench_file"
        continue
    endif
    
    @ total_tests = $total_tests + 1
    
    echo ""
    echo "Running: $bench ($benchmark_type)"
    echo "----------------------------------------"
    
    set out_file = "$tmpdir/$bench.out"
    
    # Use time command for timing (more portable)
    # Format: real time (wall time) in seconds
    echo "Command: $solver $bench_file $maxsat_flag"
    
    # Use simple timing with date command
    # Note: date +%s gives integer seconds, so very fast runs (<1s) will show as 0
    set start_time = `date +%s`
    $solver $bench_file $maxsat_flag >& $out_file
    set solver_status = $status
    set end_time = `date +%s`
    @ runtime_sec = $end_time - $start_time
    set runtime = "$runtime_sec"
    
    # If runtime is 0, the solver ran very fast (< 1 second)
    # This is common for small benchmarks
    if ("$runtime" == "" || "$runtime" == "0") then
        set runtime = "<1"
    endif
    
    # Extract result
    set result = "UNKNOWN"
    set is_sat = `grep -c "^s SATISFIABLE" $out_file`
    set is_unsat = `grep -c "^s UNSATISFIABLE" $out_file`
    set is_timeout = `grep -c "^s TIMEOUT" $out_file`
    
    if ($is_sat > 0) then
        set result = "SATISFIABLE"
    else if ($is_unsat > 0) then
        set result = "UNSATISFIABLE"
    else if ($is_timeout > 0) then
        set result = "TIMEOUT"
    else
        set result = "ERROR"
    endif
    
    # Extract objective value for MaxSAT
    set objective = "N/A"
    if ($benchmark_type == "wcnf" && $is_sat > 0) then
        set obj_line = `grep "^o " $out_file | head -n 1`
        if ("$obj_line" != "") then
            set objective = `echo $obj_line | sed 's/^o //'`
        endif
    endif
    
    # Determine status
    set status = "OK"
    if ("$result" == "ERROR" || "$result" == "UNKNOWN") then
        set status = "FAILED"
        @ failed_tests = $failed_tests + 1
        echo "WARNING: Solver did not produce expected output"
        echo "Last 20 lines of output:"
        tail -n 20 $out_file
    else
        @ passed_tests = $passed_tests + 1
    endif
    
    # Print and save results
    printf "  Result: %s\n" $result
    if ($objective != "N/A") then
        printf "  Objective: %s\n" $objective
    endif
    printf "  Runtime: %s seconds\n" $runtime
    printf "  Status: %s\n" $status
    
    # Save to output file
    printf "%-40s | %-4s | %-12s | %-10s | %-10s | %s\n" "$bench" "$benchmark_type" "$result" "$objective" "$runtime" "$status" >> $output_file
end

echo ""
echo "================================================"
echo "Summary"
echo "================================================"
echo "Total tests: $total_tests"
echo "Passed: $passed_tests"
echo "Failed: $failed_tests"
echo ""
echo "Results saved to: $output_file"
echo ""

# Save summary to output file
echo "" >> $output_file
echo "Summary" >> $output_file
echo "----------------------------------------------------" >> $output_file
echo "Total tests: $total_tests" >> $output_file
echo "Passed: $passed_tests" >> $output_file
echo "Failed: $failed_tests" >> $output_file

# Clean up
rm -rf $tmpdir

if ($failed_tests > 0) then
    echo "WARNING: Some tests failed. Check the output file for details."
    exit 1
else
    echo "All tests passed successfully!"
    exit 0
endif

