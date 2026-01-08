#! /bin/csh -f

if ($#argv < 1) then
    echo "This script tests incremental flow using IPAMIR example programs."
    echo "It compiles the examples (if needed) and runs them to verify IPAMIR functionality."
    echo ""
    echo "Parameters: <intel_sat_build_directory> [Additional build parameters]"
    echo "Example: scripts/test_ipamir_incremental.csh ."
    echo ""
    echo "The script will:"
    echo "  1. Build the IPAMIR library if needed"
    echo "  2. Compile IPAMIR example programs"
    echo "  3. Run each example and verify it completes successfully"
    exit 140
endif

set build_dir = $1
set script_dir = `dirname $0`
set root_dir = `cd $script_dir/.. && pwd`
set examples_dir = "$root_dir/ipamir_examples"
set additional_params = "$argv[2-]"

echo "========================================="
echo "Testing IPAMIR Incremental Flow"
echo "========================================="
echo "Build directory: $build_dir"
echo "Root directory: $root_dir"
echo "Examples directory: $examples_dir"

# Check if examples directory exists
if (! -d $examples_dir) then
    echo "ERROR: Examples directory does not exist: $examples_dir"
    exit 1
endif

# Check if we're in the build directory or if it's the root
set current_dir = `pwd`
if ($build_dir == ".") then
    set build_dir = $current_dir
else
    cd $build_dir
    set build_dir = `pwd`
endif

# Check if ToporIpamir.h exists
if (! -f "$root_dir/ToporIpamir.h") then
    echo "ERROR: ToporIpamir.h not found in $root_dir"
    exit 1
endif

# Check if library needs to be built
# The library name is determined by the Makefile LIB variable (defaults to directory name)
set lib_base_name = `basename $build_dir`
set lib_name = "lib${lib_base_name}_shared_release.so"
set lib_static_name = "lib${lib_base_name}_release.a"
set lib_exists = 0

# Try to find existing library (check for shared first, then static)
if (-f "$build_dir/$lib_name") then
    set lib_exists = 1
    echo "Found existing shared library: $build_dir/$lib_name"
else
    # Try to find any shared release library
    set found_lib = `ls $build_dir/lib*_shared_release.so 2>/dev/null | head -n 1`
    if ($found_lib != "") then
        set lib_name = `basename $found_lib`
        set lib_exists = 1
        echo "Found existing shared library: $build_dir/$lib_name"
    else if (-f "$build_dir/$lib_static_name") then
        set lib_exists = 1
        set lib_name = $lib_static_name
        echo "Found existing static library: $build_dir/$lib_name"
    else
        # Try to find any static release library
        set found_lib = `ls $build_dir/lib*_release.a 2>/dev/null | head -n 1`
        if ($found_lib != "") then
            set lib_name = `basename $found_lib`
            set lib_exists = 1
            echo "Found existing static library: $build_dir/$lib_name"
        else
            echo "Library not found. Will attempt to build it..."
        endif
    endif
endif

# Try to build the library if it doesn't exist
if ($lib_exists == 0) then
    echo ""
    echo "========================================="
    echo "Building IPAMIR library..."
    echo "========================================="
    cd $root_dir
    
    # Try to build the shared release library
    if (-f Makefile) then
        make librh $additional_params
        if ($status != 0) then
            echo "ERROR: Failed to build library. Trying alternative build..."
            # Try building static library as fallback
            make libr $additional_params
            if ($status != 0) then
                echo "ERROR: Failed to build library. Please build manually."
                exit 1
            else
                echo "Built static library instead. Will link statically."
                set lib_name = $lib_static_name
                set lib_exists = 1
            endif
        else
            set lib_exists = 1
        endif
    else
        echo "WARNING: Makefile not found. Assuming library is already built or will be linked differently."
    endif
    
    cd $current_dir
endif

# Determine compiler and flags
set CXX = `which g++`
if ($CXX == "") then
    set CXX = `which clang++`
endif
if ($CXX == "") then
    echo "ERROR: C++ compiler (g++ or clang++) not found"
    exit 1
endif

echo ""
echo "Using compiler: $CXX"

# Compile flags
set CXXFLAGS = "-std=c++20 -Wall -O2 -g"
set INCLUDES = "-I$root_dir"

# Determine library flags
set LIBFLAGS = ""
set LDPATH = ""
set is_shared = 0
set lib_suffix = `echo $lib_name | sed 's/.*\.//'`
if ($lib_suffix == "so") then
    set is_shared = 1
endif

if ($is_shared == 1) then
    # Shared library - extract library base name for linking
    # Remove "lib" prefix and ".so" suffix, then use with -l
    set lib_link_name = `echo $lib_name | sed 's/^lib//' | sed 's/_shared_release\.so$/_shared_release/' | sed 's/\.so$//'`
    set LIBFLAGS = "-L$build_dir -l${lib_link_name} -Wl,-rpath,$build_dir"
    set LDPATH = "LD_LIBRARY_PATH=$build_dir"
    echo "Linking with shared library: $lib_name (link name: ${lib_link_name})"
else
    # Static library - use full path
    set LIBFLAGS = "$build_dir/$lib_name"
    echo "Linking with static library: $lib_name"
endif

set LIBS = "-lpthread"

# Compile all example programs
echo ""
echo "========================================="
echo "Compiling IPAMIR example programs..."
echo "========================================="

set examples = ("example1_incremental" "example2_assumptions" "example3_complex")
set compiled_examples = ()

foreach example ($examples)
    set src_file = "$examples_dir/${example}.cpp"
    set exe_file = "$examples_dir/${example}"
    
    if (! -f $src_file) then
        echo "WARNING: Source file not found: $src_file"
        continue
    endif
    
    echo "Compiling: $example"
    echo "Command: $CXX $CXXFLAGS $INCLUDES -o $exe_file $src_file $LIBFLAGS $LIBS"
    $CXX $CXXFLAGS $INCLUDES -o $exe_file $src_file $LIBFLAGS $LIBS
    
    if ($status != 0) then
        echo "ERROR: Failed to compile $example"
        exit 1
    endif
    
    set compiled_examples = ($compiled_examples $exe_file)
    echo "Successfully compiled: $example"
end

echo ""
echo "Compiled ${#compiled_examples} example(s) successfully"

# Run all compiled examples
echo ""
echo "========================================="
echo "Running IPAMIR example programs..."
echo "========================================="

set all_passed = 1
set test_count = 0
set pass_count = 0

foreach exe ($compiled_examples)
    set example_name = `basename $exe`
    set test_count = `expr $test_count + 1`
    
    echo ""
    echo "----------------------------------------"
    echo "Running: $example_name"
    echo "----------------------------------------"
    
    # Set library path if using shared library
    if ($is_shared == 1) then
        env $LDPATH $exe
        set run_status = $status
    else
        $exe
        set run_status = $status
    endif
    
    if ($run_status == 0) then
        echo "PASSED: $example_name completed successfully"
        set pass_count = `expr $pass_count + 1`
    else
        echo "FAILED: $example_name returned exit code $run_status"
        set all_passed = 0
    endif
end

# Summary
echo ""
echo "========================================="
echo "Test Summary"
echo "========================================="
echo "Total tests: $test_count"
echo "Passed: $pass_count"
echo "Failed: `expr $test_count - $pass_count`"

if ($all_passed == 1) then
    echo ""
    echo "========================================="
    echo "All IPAMIR incremental flow tests PASSED"
    echo "========================================="
    exit 0
else
    echo ""
    echo "========================================="
    echo "Some tests FAILED"
    echo "========================================="
    exit 1
endif

