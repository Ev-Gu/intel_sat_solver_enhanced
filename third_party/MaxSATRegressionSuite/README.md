# README
The testSolver.py script runs a MaxSAT solver on a specified set of small instances that are easy to solve quickly. In order 
to participate in the main tracks of the MSE 2024, a solver needs to not incur any errors when invoked on the instances listed in 
 MSE22+23Unique.csv. The script is set up to test the right instances with the default arguments, so more specifically:

--> A solver participating in the exact weighted track needs to pass:

    ./testSolver.py "YOURSOLVER [arguments]"
    
example:

    ./testSolver.py "../MaxSATSolver/Pacose --gbmo" 
(*note the quotation marks if arguments are used*)

--> A solver participating **only** in the unweighted track needs to pass: 

    ./testSolver.py --unweighted "YOURSOLVER [arguments]"

--> A solver participating in the anytime track needs to pass

    ./testSolver.py --anytime "YOURSOLVER [arguments]"

**Note** The WCNF files can be downloaded from the MSE hompage directly. With ./install the instances will be downloaded and unzipped from the hompage: 

    https://maxsat-evaluations.github.io/2024/MaxSATRegressionSuiteWCNFs.zip

**Note** In order to work, the script needs access to a valid SAT solver. By default the script will assume that kissat is installed in "./kissat/build/kissat". 
A path to some other SAT solver (that uses the standard exit codes) can be provided with the --satSolver flag. 
e.g to test an exact solver participating in the weighted track using some other SAT solver one invokes:

    ./testSolver.py --satSolver path/to/satsolver "YOURSOLVER [arguments]"

## More Details 
The default invocation of testsolver uses the flags '--csv MSE22+23Unique.csv' and '--upperBound 2^60' telling it to run on the instances in MSE22+23Unique.csv that have a maximum sum of weights of 2^60. 

With the --unweighted flag, the arguments are set to '--csv allissues.csv' and '--maxWeight 1' in order to run it on all instances that have a maxweight of 1.

With the --anytime flag, the instances tested are the same as for the exact solvers, as well as the ones listed in MSE23anytime.csv. Additionally, SIGTERM is sent within one second (chosen randomly), after which the consistency of the solution returned by the solver is checked. 

A non-optimal solution marked with "s OPTIMUM FOUND" is treated as an error. If the solver marks a solution with "s SATISFIABLE" needs to provide a correct model -- and in the exact track, all solutions should be marked with "OPTIMUM FOUND" if no timeout occured.

### About Instance Generation 
The instances in the test suites were automatically generated using a fuzzing and delta-debugging tool (Paxian & Biere, 2023) that tested all MSE22/MSE23 exact and anytime solvers. The folders are named after the MSE year of the solvers that were tested. Each folder, labeled as MSEXXUnique, contains one instance for every type of bug triggered. In addition, we employed two proof-logging solvers (Pacose (not yet published) and CGSS (Berg et. al, 2023)) with a 15-second timeout in order certify the optimum costs of (most) solutions. For the anytime solver category, a longer timeout was used to select the instances for the anytime folder.

For the MSE24 test suite, we selected instances that:

1) at least one solver in either the 2022 or 2023 evaluation showed buggy behavior. 
2) at least one solver from MSE23 was able to solve within 1 second OR one of the certified solvers was able to solve and certify the result within 15 seconds. 

Only one instance for each type of bug discovered was included in the UNIQUE folders. 

**Note** that due to the way of construction, the regression suite contains:
    - instances for which all soft clauses are satisfiable (optimum cost 0)
    - instances that have no solutions (UNSATISFIABLE). 
We stress that all instances selected are solvable by at least one solve in MSE23 in one second. Additionally, we will do our best to ensure that all of the instances used in the evaluation tracks of MSE24 will have solutions and an optimum cost higher than 0. 

## Further Testing and Recommendations for Improving Solver Robustness

As the yearly MaxSAT Evaluation drives MaxSAT solver development forward, we want to recommend the following standardizations to the solvers to make them easier to use. These suggestions are in line with the mathematical logic of the Boolean satisfiability problem. 
    - An empty instance should yield a weight "o 0", with an empty model line "v" and the status line "s OPTIMUM FOUND".
    - An empty hard clause should result in an unsatisfiable instance.
    - An empty soft clause should be considered unsatisfied by all solutions and thus incur cost.
    - A solver should accept weight 0 for a soft clause.

**These kind of instances will not occur in the MSE24 tracks.** You can however, use the testsolver script in order to test some simple basic instances with these special cases with the option "--csv baseWCNFs.csv".

Additionally, starting in MSE2024 there are new rules for the Solver Exit Codes (https://maxsat-evaluations.github.io/2024/rules.html). As in the incremental track, the solver must return one of the following exit codes:
    0   if it cannot find a solution satisfying the hard clauses or prove unsatisfiability.
    10 if the solver finds a solution satisfying the hard clauses but does not prove it to be unsatisfiable.
    20 if it finds that the hard clauses are unsatisfiable.
    30 if it finds an optimal solution.

The testsolver script will tell you that as "ISSUES" -- with --verbose you can exactly see which instances have issues remaining.


## Obtaining a SAT Solver. 

The testSolver script applies a sat solver to check if the hard clauses are satisfiable.

Please provide a valid SAT solver with the option --satSolver or install kissat (https://github.com/arminbiere/kissat) with:
    
    git clone https://github.com/arminbiere/kissat.git   or:  
    git clone git@github.com:arminbiere/kissat.git
    cd kissat
    ./configure && make test

Per-default, the script looks for kissat in the folder "./kissat/build/kissat". Other paths can be specified with the --satSolver flag. 

## Further Examples

```
./testSolver.py --help
```

The script can be run with just a folder containing wcnf instances:
```
./testSolver.py --folder MSE23Big ../MaxSATSolver/MaxHS
```

Or you can run it with a csv file containing additional information as the best found solution so far together with a model and if a certified solver was able to proof this solution:
```
./testSolver.py --csv MSE23Big.csv ../MaxSATSolver/Exact
```

If you want to pass arguments to the solver you have to quote them:

```
./testSolver.py --csv MSE22Unique.csv "../MaxSATSolver/Pacose --gbmo"
```

To run all instances increase the upper bound. For the evaluation you need to pass only an upper bound of 2^60 for the sum of soft weights.

```
python -u testSolver.py --csv allInstances.csv "../MaxSATSolver/MSE22/Exact.sh" --timeout 3 --upperBound 18446744073709551615
```

Another for unweighted instances only (there are only a few):
 ```
 ./testSolver.py --csv MSE22Issues.csv ../MaxSATSolver/MaxCDCL --timeout 3 --verbose --satSolver ../cadical/build/cadical --maxWeight 1
```

# Further Notes:
Feel free to contribute to bug fixing and further development of the script. Additional scripts used for creating the CSV files will soon be made publicly available in the fuzzer repository on the MSE24 page.

If you discover any interesting or algorithmic bugs using this script, I would be very keen to hear about them. I am planning to write a follow-up paper on fuzzing and delta debugging. Please don't hesitate to get in touch with me, Tobias Paxian, at paxiant@informatik.uni-freiburg.de.


# Reference:
Paxian, T., & Biere, A. (2023). Uncovering and Classifying Bugs in MaxSAT Solvers through Fuzzing and Delta Debugging. In Proceedings of the Pragmatics of SAT (POS) 2023.

Berg, J., Bogaerts, B., Nordström, J., Oertel, A. and Vandesande, D. (2023). Certified core-guided MaxSAT solving. In International Conference on Automated Deduction (pp. 1-22). Cham: Springer Nature Switzerland.