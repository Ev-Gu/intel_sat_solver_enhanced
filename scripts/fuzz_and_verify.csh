#! /bin/csh -f

if ($#argv < 1) then
	echo "Fuzz and verify IntelSAT (that is, its executable, such as, intel_sat_solver_static) forever, given the optional parameters on the input"
	echo "Parameters: intel_sat_executable [format] [Any additional parameters (optional)]"
	echo "  format: 'cnf' or 'wcnf' (default: 'cnf'). Use 'wcnf' for MaxSAT fuzzing."
	echo "  If format is 'both', alternates between CNF and WCNF formats."
	echo "Examples:"
	echo "  scripts/fuzz_and_verify.csh intel_sat_solver_static"
	echo "  scripts/fuzz_and_verify.csh intel_sat_solver_static wcnf"
	echo "  scripts/fuzz_and_verify.csh intel_sat_solver_static both"
	exit
endif


set topor = $1
if (! -e $topor) then
	echo "intel_sat_executable doesn't exist at $topor. Exiting"
	echo "ERROR"
	exit 130
endif

# Parse format parameter
set format = "cnf"
if ($#argv >= 2) then
    set format_arg = "$argv[2]"
    # Check if second argument is a format specifier (not a solver parameter)
    if ( "$format_arg" == "cnf" || "$format_arg" == "wcnf" || "$format_arg" == "both" ) then
        set format = "$format_arg"
        set additional_params = "$argv[3-]"
    else
        set additional_params = "$argv[2-]"
    endif
else
    set additional_params = ""
endif

set script_dir = `dirname $0`
set delta_debug_topor_till_fixed_point = ${script_dir}/delta_debug_intel_sat_till_fixed_point.csh
set cnfuzz_incr = "$script_dir/../third_party/cnfuzzdd2013/cnfuzz_incr"
set wcnfuzz_incr = "$script_dir/../third_party/cnfuzzdd2013/wcnfuzz_incr"

# Check if wcnfuzz_incr exists if WCNF format is requested
if ( "$format" == "wcnf" || "$format" == "both" ) then
    if ( ! -e "$wcnfuzz_incr" ) then
        echo "ERROR: wcnfuzz_incr not found at $wcnfuzz_incr"
        echo "Please build it using: cd third_party/cnfuzzdd2013 && g++ -O3 -o wcnfuzz_incr wcnfuzz_incr.c"
        exit 1
    endif
endif

set isok = 1
set i = 1
set use_wcnf = 0

set tmpdir = "/tmp/$USER/fuzz_and_verify_`hostname`_$$"
mkdir -p $tmpdir

while ($isok)
    # Determine format for this iteration
    if ( "$format" == "both" ) then
        # Alternate between CNF and WCNF
        if ( $use_wcnf == 0 ) then
            set use_wcnf = 1
        else
            set use_wcnf = 0
        endif
    else if ( "$format" == "wcnf" ) then
        set use_wcnf = 1
    else
        set use_wcnf = 0
    endif
    
	echo "i : $i"
	
	if ( $use_wcnf == 1 ) then
        set currf = "$tmpdir/incrfuzz_$$.wcnf"
        echo "$wcnfuzz_incr > $currf"
        $wcnfuzz_incr > $currf
        set maxsat_flag = "-M 1"
        echo "Format: WCNF (MaxSAT)"
    else
        set currf = "$tmpdir/incrfuzz_$$.cnf"
        echo "$cnfuzz_incr > $currf"
        $cnfuzz_incr > $currf
        set maxsat_flag = ""
        echo "Format: CNF (SAT)"
    endif
	
	set ddout = "$tmpdir/ddout_$$"
	
	if ( "$maxsat_flag" != "" ) then
        echo "$delta_debug_topor_till_fixed_point $topor $currf $maxsat_flag $additional_params |& tee $ddout"
        $delta_debug_topor_till_fixed_point $topor $currf $maxsat_flag $additional_params |& tee $ddout
    else
        echo "$delta_debug_topor_till_fixed_point $topor $currf $additional_params |& tee $ddout"
        $delta_debug_topor_till_fixed_point $topor $currf $additional_params |& tee $ddout
    endif
	set isok = `tail -n 1 $ddout | grep -c Ok`
	@ i = $i + 1
end
