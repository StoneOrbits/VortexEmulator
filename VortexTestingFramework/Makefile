# clear out all default make targets
.SUFFIXES:

# List all make targets which are not filenames
.PHONY: all tests clean wasm

# compiler tool definitions
ifdef WASM
CC=em++
AR=emar cru
else
CC=g++
AR=ar cru
endif

MAKE=make
RM=rm -rf
RANLIB=ranlib

CFLAGS=-O2 -Wall

# compiler defines
DEFINES=\

# compiler include paths
INCLUDES=\
	-I ./VortexEngine/VortexEngine/VortexLib/EngineDependencies/ \
	-I ./VortexEngine/VortexEngine/VortexLib/ \
	-I ./VortexEngine/VortexEngine/src/ \
	-I ./EngineDependencies/ \
	-I ./ \

# output filename
OUTTARGET=vortex

ifdef WASM
OUTTARGET=vortex.html
DEFINES += -D WASM
LIBS += --bind
endif

# only set them if they're not empty to prevent unnecessary whitespace
ifneq ($(DEFINES),)
    CFLAGS+=$(DEFINES)
endif
ifneq ($(INCLUDES),)
    CFLAGS+=$(INCLUDES)
endif

# local NONSTANDARD libraries to link with
# these MUST be exact filenames, cannot be -l syntax
# for example:
#   ../path/to/libname.a
# NOT THIS:
#   -L../path/to -lname
# You should NOT need to add a make target to build
# this library if you have added it correctly.
LLIBS=\
	./VortexEngine/VortexEngine/vortex.a \

# STANDARD libraries to link with (-l is fine here)
# MUST have LLIBS BEFORE the standard libraries
LIBS=\
	$(LLIBS) \

# source files
# local source files first, other sources after
SRC=\
    ./LinuxMain.cpp \
    ./TestFrameworkLinux.cpp \

# object files are source files with .c replaced with .o
OBJS=\
	$(SRC:.cpp=.o) \

# dependency files are source files with .c replaced with .d
DFILES=\
	$(SRC:.cpp=.d) \

# target dependencies
# this includes any script generated c/h files,
# the $(LLIBS) list, and the $(OBJS) list
DEPS=\
	$(LLIBS) \
	$(OBJS) \

# unit tests
TESTS=\

# target files
TARGETS=\
    $(OUTTARGET) \

# Default target for 'make' command
all: $(TARGETS)

# unit test target
tests:
	cd tests/ && ./runtests.sh

# target for detect_gibberish
$(OUTTARGET): $(DEPS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# force sub-build of wasm
wasm: FORCE
	env WASM=1 TESTFRAMEWORK=1 $(MAKE)

valgrind: vortex
	rm -f valg.out
	valgrind \
	  -v \
	  --leak-check=full \
	  --show-leak-kinds=all \
	  --track-origins=yes \
	  --gen-suppressions=all \
	  --log-file=valg.out \
	  ./vortex
	cat valg.out

# catch-all make target to generate .o and .d files
%.o: %.cpp
	$(CC) $(CFLAGS) -MMD -c $< -o $@

# catch-all for static libraries in the form of:
# <directory>/<library.a>
# this expects that the makefile in <directory> has a
# make target named <library>
%.a: FORCE
	# run make with VortexTestingFramework=1 in the environment
	env TESTFRAMEWORK=1 $(MAKE) -C $(dir $@) $(notdir $@)

# Empty rule that forces %.a to run all the time
FORCE:

# generic clean target
clean:
	@$(RM) $(DFILES) $(OBJS) $(TARGETS) $(TESTS) vortex.html vortex.js vortex.wasm *.txt FlashStorage.flash
	$(MAKE) -C ./VortexEngine/VortexEngine clean

# Now include our target dependency files
# the hyphen means ignore non-existent files
-include $(DFILES)
