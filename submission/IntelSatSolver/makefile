#-----------------------------------------------------------------------#
# IPAMIR static library for the IntelSAT solver (signature: IntelSatSolver)
#
# Self-contained MaxSAT Evaluation incremental-track submission.
# Place this directory under the IPAMIR repository as  maxsat/IntelSatSolver/
# and run `make` (optionally `make IPAMIRSOLVER=IntelSatSolver` from an app).
#
# Default goal builds:  libipamirIntelSatSolver.a
# containing the whole solver plus the IPAMIR interface (ToporIpamir.cc).
#
# The full solver source is bundled in ./solver_src so the build is standalone
# and does not depend on anything outside this directory (except the IPAMIR
# header, available as ../../ipamir.h per the maxsat/ convention).
#-----------------------------------------------------------------------#

SIG        = IntelSatSolver
TARGET     = libipamir$(SIG).a

# Bundled IntelSAT source tree. LIB is overridden so the produced archive has a
# predictable name regardless of this directory's name.
SOLVERROOT = solver_src
SOLVERLIBNAME = intelsat
SOLVERLIB  = $(SOLVERROOT)/lib$(SOLVERLIBNAME)_release.a

CXX        = g++
CXXFLAGS   = -DNDEBUG -O3 -std=c++20

all: $(TARGET)

# Build the solver's release library from the bundled source, then repackage it:
#   - drop Main.or (it carries main(), which would clash with the application's main())
#   - add glue.o, which supplies MainWallTimePassed() that Main.or used to provide
$(TARGET): glue.o
	$(MAKE) -C $(SOLVERROOT) libr LIB=$(SOLVERLIBNAME)
	cp $(SOLVERLIB) $(TARGET)
	ar d $(TARGET) Main.or
	ar r $(TARGET) glue.o
	ranlib $(TARGET)

glue.o: glue.cc
	$(CXX) $(CXXFLAGS) -c glue.cc -o glue.o

clean:
	-$(MAKE) -C $(SOLVERROOT) clean LIB=$(SOLVERLIBNAME)
	rm -f *.o $(TARGET)

.PHONY: all clean
