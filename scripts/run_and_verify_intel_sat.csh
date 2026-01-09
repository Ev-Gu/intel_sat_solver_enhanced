#! /bin/csh -f

if ($#argv < 2) then
    echo "This script run and verifies IntelSAT (that is, its executable, such as, intel_sat_solver_static). It returns 0 on success and other values on failure. Features:"
    echo "- Incremental instances in IntelSAT format (DIMACS - p cnf + s assump1 ... assumpn 0) are supported, while still taking advantage of the standard tools drat-trim and DiMoCheck for verification (with a new dedicated flow on top of them in the script)."
    echo "- Every generated clause is verified using drat-trim."
    echo "- Every solution is verified using DiMoCheck."
    echo "Parameters: the command-line (i.e., <intel_sat_solver_static> <file> [Any additional parameters])"
    echo "Example invocation: scripts/run_and_verify_intel_sat.csh intel_sat_solver_test_static regression_instances/regr_1.cnf"
    exit 140
endif

echo "The command-line $argv[1-]"

set hostname_val = (`sh -c 'hostname'`)
set hostname_val = "$hostname_val"
set tmpdir = "/tmp/$USER/run_and_verify_topor_${hostname_val}_$$"
mkdir -p "$tmpdir"
echo "The temporary directory: $tmpdir"
# The output file of topor_tool
set out_file = "$tmpdir/outt"
set drat_file = "${out_file}.drat"

echo "$argv[1-] /topor_tool/text_drat_file $drat_file /topor_tool/print_model 1 >& $out_file"
$argv[1-] /topor_tool/text_drat_file "$drat_file" /topor_tool/print_model 1 >& "$out_file"
echo "Invocation completed"
set issat = (`sh -c 'grep -c "s SATISFIABLE" "$1"' sh "$out_file"`)
set issat = "$issat"
set isunsat = (`sh -c 'grep -c "s UNSATISFIABLE" "$1"' sh "$out_file"`)
set isunsat = "$isunsat" 
if ($issat == 0 && $isunsat == 0) then
	echo "ERROR: unknown result"
	echo "Solver output (last 50 lines):"
	tail -n 50 $out_file
	echo "Full solver output saved in: $out_file"
	exit 120
endif

set f = "$argv[2]"
set incremental_queries = (`sh -c 'egrep -c "^s " "$1"' sh "$f"`)
set incremental_queries = "$incremental_queries"

# Check if this is a MaxSAT instance (.wcnf file or -M 1 flag)
set is_wcnf = (`sh -c 'echo "$1" | grep -c "\\.wcnf$"' sh "$f"`)
set is_wcnf = "$is_wcnf"
set has_maxsat_flag = (`sh -c 'echo "$1" | grep -c "\\-M 1"' sh "$argv[1-]"`)
set has_maxsat_flag = "$has_maxsat_flag"
set is_maxsat = 0
if ( $is_wcnf != 0 || $has_maxsat_flag != 0 ) then
	set is_maxsat = 1
	echo "Detected MaxSAT instance"
endif

set print_ucore = (`sh -c 'echo "$1" | grep -c "/topor_tool/print_ucore 1"' sh "$argv[1-]"`)
set print_ucore = "$print_ucore"
echo "print_ucore = $print_ucore"

# script_dir is used only to get the verifiers: precochk and drat
set script_dir = (`sh -c 'dirname "$1"' sh "$0"`)
set script_dir = "$script_dir"
set cs = $script_dir/../third_party/dimocheck
set cu = $script_dir/../third_party/drat-trim
# drat-trim params for both SAT & UNSAT: forward; verbose
set cupparams = "-f -v -U"
# Forward RUP UNSAT
set cupunsat = "$cupparams"
# Forward RUP SAT
set cupsat = "-S $cupparams"

# The output file of the verifier
set verify_out = ${out_file}.verify
# Get the number of variables from the CNF (the highest integer in the clauses and the assumption lists)
set vars = (`sh -c 'egrep "s -?[0-9]|^-?[0-9]" "$1" | grep -Eo "[0-9]+" | sort -rn | head -n 1' sh "$f"`)
set vars = "$vars"

if ($incremental_queries) then
	echo "Type Incremental: $incremental_queries queries"
	set curr_query = 1
	
	while ($curr_query <= $incremental_queries)
		echo "====================================================="
		echo "Query $curr_query out of $incremental_queries"
		# fcurr_query: the would-be non-incremental input CNF of the current query only we're going to construct
		set fcurr_query = ${out_file}.cnf
		# Find the line of the current query in the input CNF
		set curr_query_line_num_in_f = (`sh -c 'grep -n -m"$1" "^s " "$2" | tail -n 1 | cut -f1 -d:' sh "$curr_query" "$f"`)
		set curr_query_line_num_in_f = "$curr_query_line_num_in_f"
		echo "curr_query_line_num_in_f = $curr_query_line_num_in_f"
		# Getting the result of the curent query from the incremental output
		set curr_res = (`sh -c 'grep -m"$1" "^s " "$2" | tail -n 1' sh "$curr_query" "$out_file"`)
		set curr_res = "$curr_res"
		set r = (`sh -c 'echo "$1" | awk "{print \\$2}"' sh "$curr_res"`)
		set r = "$r"
		echo "Result = $r"
		# Put only the clauses from $f till the current query in fcurr_query
		head -n "$curr_query_line_num_in_f" "$f" | egrep "^-?[0-9]" > "$fcurr_query"
		if ($print_ucore == 0 || $r != "UNSATISFIABLE") then
			# Append unit clauses for every assumption (if any) to fcurr_query
			grep -m$curr_query "^s " $f | tail -n 1 | tr ' ' "\n" | egrep -v "^(s|0)" | awk '{print $0, "0"}' >> $fcurr_query
		else
			echo "Appended only the assumptions in the ucore"
			# Append unit clauses for assumptions in the ucore (if any) to fcurr_query
			grep -m$curr_query -A 1 "^s " $out_file | tail -n 1 | tr ' ' "\n" | egrep -v "^(v|0)" | awk '{print $0, "0"}' >> $fcurr_query
			
		endif
		# The number of clauses in fcurr_query
		set clss = (`sh -c 'egrep -c "^-?[0-9]" "$1"' sh "$fcurr_query"`)
		set clss = "$clss" 		
		# Prepend p cnf vars clss to fcurr_query
		sed -i "1i p cnf $vars $clss" $fcurr_query
		echo "fcurr_query = $fcurr_query completed after adding p cnf $vars $clss"
		
		# fcurr_query_res: the would-be result file for the current query only
		set fcurr_query_res = ${out_file}.fcurr_query_res
		# Emitting the result to fcurr_query_res
		echo $curr_res > $fcurr_query_res
		echo "Created $fcurr_query_res file by emitting the current result $curr_res there"
		
		if ($r == "SATISFIABLE") then
			if ($clss == 0) then
				echo "SATISFIABLE empty query $curr_query completed succesfully!"
			else
				# The result is SAT
				
				# fcurr_query_drat: the would be drat for the current query
				set fcurr_query_drat = ${out_file}.fcurr_query_drat
				# get the line number of the "c query completed $curr_query" corresponding to the current query from the drat
				set curr_query_end_line_num_in_orig_drat = (`sh -c 'egrep -w -n "c query completed $1" "$2" | tr ":" " " | awk "{print \\$1}"' sh "$curr_query" "$drat_file"`)
				set curr_query_end_line_num_in_orig_drat = "$curr_query_end_line_num_in_orig_drat"
				if ($curr_query_end_line_num_in_orig_drat == "") then
					echo "ERROR query ${curr_query} (SATISFIABLE): DRAT cut off (was there an assertion failure, a segmentation fault or something other unexpected event?)"
					exit 173
				endif
				echo "curr_query_end_line_num_in_orig_drat = $curr_query_end_line_num_in_orig_drat"
				# Put only the lines till the current query from the drat in fcurr_query_drat, but without the "0" and the comment lines
				head -n "$curr_query_end_line_num_in_orig_drat" "$drat_file" | egrep -v "^(0|c)" > "$fcurr_query_drat"
				# Verifying with drat
				echo "SAT -- running $cu"
				echo "$cu $fcurr_query $fcurr_query_drat $cupsat"
				$cu $fcurr_query $fcurr_query_drat  $cupsat > $verify_out
				set ok = (`sh -c 'grep -c "s DERIVATION" "$1"' sh "$verify_out"`)
				set ok = "$ok"
				if ($ok == 0) then
					echo "ERROR query ${curr_query} SAT derivation verification failed!"
					exit 175
				endif
				
				# Get the "pline" with the solution
				set pline = (`sh -c 'grep -A 1 -m"$1" "^s " "$2" | tail -n 1' sh "$curr_query" "$out_file"`)
				set pline = "$pline"
				# and emit it to fcurr_query_res
				echo $pline >> $fcurr_query_res
				# Verifying with precochk
				echo "SAT -- running $cs"
				echo "$cs $fcurr_query $fcurr_query_res"
				$cs $fcurr_query $fcurr_query_res > $verify_out
				set ok = (`sh -c 'egrep -c "satisfiable and solution correct|s MODEL_SATISFIES_FORMULA" "$1"' sh "$verify_out"`)
				set ok = "$ok"
				if ($ok == 0) then
					echo "ERROR query ${curr_query} SAT verification failed!"
					exit 160
				endif
				
				echo "SATISFIABLE query $curr_query completed succesfully!"
			endif
		endif
		if ($r == "UNSATISFIABLE") then
			# The result is UNSAT
			# fcurr_query_drat: the would be drat for the current query
			set fcurr_query_drat = ${out_file}.fcurr_query_drat
			# get the line number of the "c query completed $curr_query" corresponding to the current query from the drat
			set curr_query_end_line_num_in_orig_drat = `egrep -w -n "c query completed $curr_query" $drat_file | tr ':' ' ' | awk '{print $1}'`
			echo "curr_query_end_line_num_in_orig_drat = $curr_query_end_line_num_in_orig_drat"
			if ($curr_query_end_line_num_in_orig_drat == "") then
				echo "ERROR query ${curr_query} (UNSATISFIABLE): DRAT cut off (was there an assertion failure, a segmentation fault or something other unexpected event?)"
				exit 174
			endif
			# Put only the lines till the current query from the drat in fcurr_query_drat, but without the "0" lines
			head -n "$curr_query_end_line_num_in_orig_drat" "$drat_file" | egrep -v "^(0|c)" > "$fcurr_query_drat"
			# Need the number of lines in the current drat for an adjustment, described below
			set lines_in_drat = (`sh -c 'wc -l "$1" | awk "{print \\$1}"' sh "$fcurr_query_drat"`)
			set lines_in_drat = "$lines_in_drat"
			if ( $lines_in_drat == 0 ) then
				# an empty proof in text format doesn't work for some reason in the drat verification tool, whereas "a" is interpreted as a binary "0"
				echo "a" > "$fcurr_query_drat"
			else
				# Restore the last 0
				echo "0" >> "$fcurr_query_drat"
			endif
			# Verifying with drat
			echo "UNSAT -- running $cu"
			echo "$cu $fcurr_query $fcurr_query_drat $cupunsat"
			$cu $fcurr_query $fcurr_query_drat  $cupunsat > $verify_out
			set ok = (`sh -c 'grep -c "s VERIFIED" "$1"' sh "$verify_out"`)
			set ok = "$ok"
			if ($ok == 0) then
				echo "ERROR query ${curr_query} UNSAT verification failed!"
				exit 170
			endif
			echo "UNSATISFIABLE query $curr_query completed succesfully!"
		endif
		if ($r != "SATISFIABLE" && $r != "UNSATISFIABLE") then
			echo "ERROR query ${curr_query} couldn't parse current result $r"
			exit 150
		endif
		@ curr_query = $curr_query + 1		
	end
else
	# The non-incremental case
	echo "Type Not Incremental"
	
	# Handle MaxSAT verification
	if ($is_maxsat) then
		if ($isunsat) then
			echo "MaxSAT UNSATISFIABLE - verification passed"
		else if ( $issat != 0 ) then
			# Extract objective value and model
			set obj_line = (`sh -c 'grep "^o " "$1" | head -n 1' sh "$out_file"`)
			set obj_line = "$obj_line"
			if ( "$obj_line" == "" ) then
				echo "ERROR: No objective value line found in solver output"
				exit 1
			endif
			set reported_obj = (`sh -c 'echo "$1" | sed "s/^o //" | awk "{print \\$1}"' sh "$obj_line"`)
			set reported_obj = "$reported_obj"
			
			set model_line = (`sh -c 'grep "^v" "$1" | head -n 1' sh "$out_file"`)
			set model_line = "$model_line"
			if ( "$model_line" == "" ) then
				echo "ERROR: No model line found in solver output"
				exit 1
			endif
			set model_values = (`sh -c 'echo "$1" | sed "s/^v //"' sh "$model_line"`)
			set model_values = "$model_values"
			
			# Create temporary files for verification
			set model_file = "$tmpdir/model"
			set hard_clauses_file = "$tmpdir/hard_clauses"
			set soft_clauses_file = "$tmpdir/soft_clauses"
			
			# Write model to file (one value per line)
			echo "$model_values" | awk '{for(i=1;i<=NF;i++) if($i != "") print $i}' > "$model_file"
			
			# Parse WCNF file to extract hard and soft clauses
			set var_count = 0
			awk '
			{
				if ($0 ~ /^c/ || $0 == "" || $0 ~ /^[ \t]*$/) next
				first_char = substr($0, 1, 1)
				if (first_char == "r" || first_char == "o" || first_char == "l" || first_char == "b" || first_char == "n" || first_char == "p" || first_char == "s") next
				
				if (first_char == "h") {
					clause = ""
					for (i = 2; i <= NF; i++) {
						if (i > 2) clause = clause " "
						clause = clause $i
					}
					print clause > "'$hard_clauses_file'"
					for (i = 2; i <= NF; i++) {
						if ($i != "0") {
							abs_lit = ($i < 0) ? -$i : $i
							if (abs_lit > var_count) var_count = abs_lit
						}
					}
				} else if (first_char ~ /[0-9]/) {
					print $0 > "'$soft_clauses_file'"
					for (i = 2; i <= NF; i++) {
						if ($i != "0") {
							abs_lit = ($i < 0) ? -$i : $i
							if (abs_lit > var_count) var_count = abs_lit
						}
					}
				}
			}
			END {
				print var_count > "'$tmpdir/var_count'"
			}
			' $f
			
			set var_count = (`sh -c 'cat "$1"' sh "$tmpdir/var_count"`)
			set var_count = "$var_count"
			set model_var_count = (`sh -c 'wc -l < "$1"' sh "$model_file"`)
			set model_var_count = "$model_var_count"
			if ( $model_var_count < $var_count ) then
				echo "ERROR: Model has fewer variables ($model_var_count) than expected ($var_count)"
				exit 1
			endif
			
			touch "$hard_clauses_file"
			touch "$soft_clauses_file"
			
			# Function to check if a clause is satisfied
			set check_clause = "$tmpdir/check_clause"
			cat > $check_clause << 'EOFCHECK'
#!/bin/csh -f
set clause_line = "$1"
set model_file = "$2"
set satisfied = 0

foreach lit (`echo $clause_line | awk '{for(i=1;i<=NF;i++) if($i != "0") print $i}'`)
	if ($lit == "0") break
	set abs_lit = `echo $lit | sed 's/-//'`
	set var_value = `sed -n "${abs_lit}p" $model_file`
	if ($lit > 0 && $var_value == 1) then
		set satisfied = 1
		break
	else if ($lit < 0 && $var_value == 0) then
		set satisfied = 1
		break
	endif
end

if ($satisfied == 1) then
	exit 0
else
	exit 1
endif
EOFCHECK
			chmod +x $check_clause
			
			# Verify all hard clauses are satisfied
			if ( -s "$hard_clauses_file" ) then
				set hard_clause_num = 0
				foreach hard_clause (`cat "$hard_clauses_file"`)
					set hard_clause_num = `expr $hard_clause_num + 1`
					$check_clause "$hard_clause" "$model_file"
					if ( $status != 0 ) then
						echo "ERROR: Hard clause $hard_clause_num is not satisfied: $hard_clause"
						exit 1
					endif
				end
			endif
			
			# Compute cost from unsatisfied soft clauses
			set computed_cost = 0
			if ( -s "$soft_clauses_file" ) then
				foreach soft_clause (`cat "$soft_clauses_file"`)
					# Extract weight (first field) and clause (fields 2..NF)
					set weight = (`sh -c 'echo "$1" | awk "{print \\$1}"' sh "$soft_clause"`)
					set weight = "$weight"
					set clause_part = (`sh -c 'echo "$1" | awk "{for(i=2;i<=NF;i++) {if(i>2) printf \\" \\"; printf \\"%s\\", \\$i}}"' sh "$soft_clause"`)
					set clause_part = "$clause_part"
					$check_clause "$clause_part" "$model_file"
					if ( $status == 0 ) then
						continue
					else
						set computed_cost = `expr $computed_cost + $weight`
					endif
				end
			endif
			
			# Verify computed cost matches reported objective
			if ( "$computed_cost" != "$reported_obj" ) then
				echo "ERROR: Cost mismatch - reported: $reported_obj, computed: $computed_cost"
				exit 1
			endif
			
			echo "MaxSAT SATISFIABLE - verification passed (objective: $reported_obj)"
		endif
	else
		# Regular SAT verification
		set ispcnf = (`sh -c 'grep -c "^p cnf" "$1"' sh "$f"`)
		set ispcnf = "$ispcnf"
		set newf = "${out_file}.cnf"
		cp "$f" "$newf"
		if ( $ispcnf == 0 ) then
			set clss = (`sh -c 'egrep -c "^-?[0-9]" "$1"' sh "$f"`)
			set clss = "$clss"
			sed -i "1i p cnf $vars $clss" "$newf"
			echo "Added p cnf $vars $clss to $newf"
		endif
		set clss = (`sh -c 'grep "^p cnf" "$1" | awk "{print \\$4}"' sh "$newf"`)
		set clss = "$clss"
		echo "clss = $clss"
		if ( $issat != 0 ) then
			if ( $clss == 0 ) then
				echo "SAT -- empty"
			else
				echo "SAT -- running $cs"
				echo "$cs $newf $out_file"
				$cs "$newf" "$out_file" > "$verify_out"
				set ok = (`sh -c 'grep -c "satisfiable and solution correct" "$1"' sh "$verify_out"`)
				set ok = "$ok"
				if ( $ok == 0 ) then
					echo "ERROR: SAT verification failed!"
					exit 130
				endif
				
				echo "SAT -- running $cu"
				echo "$cu $newf $drat_file $cupsat"
				$cu "$newf" "$drat_file" $cupsat > "$verify_out"
				set ok = (`sh -c 'grep -c "s DERIVATION" "$1"' sh "$verify_out"`)
				set ok = "$ok"
				if ( $ok == 0 ) then
					echo "ERROR: SAT derivation verification failed!"
					exit 135
				endif
			endif
		endif
		
		if ( $isunsat != 0 ) then
			echo "UNSAT -- running $cu"
			# If the proof is empty, the text format doesn't work for some reason in the drat verification tool, whereas "a" is interpreted as a binary "0"
			set lines_in_drat = (`sh -c 'wc -l "$1" | awk "{print \\$1}"' sh "$drat_file"`)
			set lines_in_drat = "$lines_in_drat"
			set empty_proof = (`sh -c 'grep -c "^0$" "$1"' sh "$drat_file"`)
			set empty_proof = "$empty_proof"
			if ( $lines_in_drat == 1 && $empty_proof == 1 ) then
				echo "a" > "$drat_file"
			endif
			echo "$cu $newf $drat_file $cupunsat"
			$cu "$newf" "$drat_file" $cupunsat > "$verify_out"
			set ok = (`sh -c 'grep -c "s VERIFIED" "$1"' sh "$verify_out"`)
			set ok = "$ok"
			if ( $ok == 0 ) then
				echo "ERROR: UNSAT verification failed!"
				exit 140
			endif
		endif
	endif
endif

rm -rf "$tmpdir"
echo "Ok"

