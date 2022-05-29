# clear out all default make targets
.SUFFIXES:

# List all make targets which are not filenames
.PHONY: all tests clean

# compiler tool definitions
CC=g++
MAKE=make
RM=rm -rf
AR=ar cru
RANLIB=ranlib

CFLAGS=-g -O0 -Wall

# compiler defines
DEFINES=\
	-D LINUX_FRAMEWORK \
	-D TEST_FRAMEWORK \

# compiler include paths
INCLUDES=\
	-I ./VortexFramework/VortexFramework/src/ \
	-I ./ \

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

# STANDARD libraries to link with (-l is fine here)
# MUST have LLIBS BEFORE the standard libraries
LIBS=\
	$(LLIBS) \
	-lncurses \

# source files
# local source files first, other sources after
SRC=\
    ./LinuxMain.cpp \
    ./Arduino.cpp \
    ./FastLED.cpp \
    ./FlashStorage.cpp \
    ./TestFrameworkLinux.cpp \
    $(shell find ./VortexFramework/VortexFramework/src/ -type f -name '*.cpp') \

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
    vortex_framework \

# Default target for 'make' command
all: $(TARGETS)

# unit test target
tests: $(TESTS)

# target for detect_gibberish
vortex_framework: $(DEPS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)
	@rm -f *.txt

# catch-all make target to generate .o and .d files
%.o: %.cpp
	$(CC) $(CFLAGS) -MMD -c $< -o $@

# catch-all for static libraries in the form of:
# <directory>/<library.a>
# this expects that the makefile in <directory> has a
# make target named <library>
%.a:
	$(MAKE) -C $(dir $@) $(notdir $@)

# generic clean target
clean:
	@$(RM) $(DFILES) $(OBJS) $(TARGETS) $(TESTS) *.txt FlashStorage.flash

# Now include our target dependency files
# the hyphen means ignore non-existent files
-include $(DFILES)
