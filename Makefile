-include config.mk

# Define programs and commands.
SH     = sh
# Archive-maintaining program;
AR     = ${CROSS_COMPILE}ar
# Program for doing assembly;
AS     = ${CROSS_COMPILE}as
# Program for compiling C programs;
CC     = ${CROSS_COMPILE}gcc
# Program for running the C preprocessor, with results to standard output;
CPP     = $(CC) -E
# Command to remove a file;
RM     = rm -f
# Command make a file;
MAKE   = make
# Command strip
STRIP	= $(CROSS_COMPILE)strip --strip-unneeded
# Command strip
INSTALL	= install

# Target executable file name
TARGET  = \
	record \
	replay

# Target library file name
LIBRARY = #common

# Installation path "make install"
INSTDIR	= $(DEMO_INSTALL_PATH)/usr/local/bin

# Project
PRJDIR = $(shell pwd)
IN_INC = $(PRJDIR)/inc
IN_SRC = $(PRJDIR)/src
IN_RUN = $(PRJDIR)/run
OUTLIB = $(PRJDIR)/lib
OUTEXE = $(PRJDIR)/out
OUTOBJ = $(PRJDIR)/obj

# Additional project with separate makefile
PRJSUB =

EXTINC =  $(IN_INC)

# Additional libraries "-lcommon"
EXTLIB	=

# Place -I options here
INCLUDES = -I. $(addprefix -I,$(EXTINC))

# List C source files here. (C dependencies are automatically generated.)
SOURCES = $(shell find $(IN_SRC)/ -type f -name '*.c')

# Prepare "C" files list
CFILES  = $(filter %.c,$(SOURCES))

# Define "C" object files.
COBJS	= $(patsubst $(IN_SRC)/%,$(OUTOBJ)/%,$(CFILES:%.c=%.o))
# Define all object files.
OBJS	= $(COBJS)

# List C source files here. (C dependencies are automatically generated.)
DEMOS   = $(shell find $(IN_RUN)/ -type f -name '*.c')
# Prepare "C" files list
XFILES  = $(filter %.c,$(DEMOS))
# Define "C" object files.
COBJX   = $(patsubst $(IN_RUN)/%,$(OUTOBJ)/%,$(XFILES:%.c=%.o))
# Define all object files.
OBJX    = $(COBJX)

# Define all dependencies
DEPS    = $(OBJS:%.o=%.d) $(OBJX:%.o=%.d)

# Define output executable target
RUNNABLE = $(OUTEXE)/$(TARGET)

# Define dynamic library file name
DYNAMICLIB = $(OUTLIB)/lib$(LIBRARY).so

# Define static library file name
STATICLIB = $(OUTLIB)/lib$(LIBRARY).a

# Place -D or -U options here
DEF = -DNDEBUG

# Define CPU flags "-march=cpu-type"
CPU =

# Define specific MPU flags "-mno-fp-ret-in-387"
MPU =

# Define "C" standart
STD = -std=gnu99

# Global flags
OPT  = $(MPU) $(MPU) $(STD) -fPIC -O2

# Generate dependencies
DEP  = -MMD

WRN  = -Werror -Wall -Wclobbered -Wempty-body -Wignored-qualifiers -Wmissing-field-initializers -Wsign-compare -Wtype-limits -Wuninitialized

# Define only valid "C" optimization flags
COPT = $(OPT) -fomit-frame-pointer -fno-stack-check

# Define only valid "C" warnings flags
CWRN = $(WRN) \
	-Wmissing-parameter-type -Wold-style-declaration -Wimplicit-int -Wimplicit-function-declaration -Wimplicit -Wignored-qualifiers \
	-Wformat-nonliteral -Wcast-align -Wpointer-arith -Wbad-function-cast -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations \
	-Wnested-externs -Wshadow -Wwrite-strings -Wfloat-equal -Woverride-init -fno-builtin

# Extra flags to give to the C compiler.
CFLAGS = $(MCU) $(DEF) $(DEP) $(COPT) $(CWRN)

# Extra flags to give to the C compiler.
override CFLAGS += $(INCLUDES)

# Define global linker flags
EXFLAGS = $(EXTLIB) -L $(OUTLIB)

# Linker flags to give to the linker.
override LDFLAGS = $(EXFLAGS) -Wl,--no-as-needed -Wl,-nostdlib

# Special linker flags to build shared library
override SHFLAGS = $(EXFLAGS) -shared -s

# Executable not defined
ifeq ($(TARGET),)
 # Output library defined
 ifneq ($(LIBRARY),)
  all: $(DYNAMICLIB) $(STATICLIB)
 else
  all:
	@echo "WARNING: Neither output, neither library defined!"
 endif
# Executable defined
else
 # Executable defined, but output library not defined
 ifeq ($(LIBRARY),)
  all: $(RUNNABLE)
 # Executable and output library defined
 else
  all: $(DYNAMICLIB) $(STATICLIB) $(RUNNABLE)
 endif
endif

# Executable defined
ifneq ($(TARGET),)
 # Output library not defined
 ifeq ($(LIBRARY),)
  $(RUNNABLE): $(OBJX) $(OBJS)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(IN_RUN)/$(@F).c -o $(OUTEXE)/$(@F) $(LDFLAGS) $(OBJS)
 # Executable and output library defined
 else
  $(RUNNABLE): $(OBJX) $(STATICLIB) $(DYNAMICLIB)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(IN_RUN)/$(@F).c -o $(OUTEXE)/$(@F) $(LDFLAGS) $(DYNAMICLIB)
 endif
endif

ifneq ($(LIBRARY),)
 $(DYNAMICLIB): $(OBJS)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ $^ $(SHFLAGS)

 $(STATICLIB): $(OBJS)
	mkdir -p $(@D)
	$(AR) rcs -o $@ $^
endif

# Sub-project defined
ifneq ($(PRJSUB),)
	$(MAKE) -C $(PRJSUB) all
endif

$(OBJS): $(OUTOBJ)/%.o: $(IN_SRC)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $< $(DEP) $(CFLAGS)

$(OBJX): $(OUTOBJ)/%.o: $(IN_RUN)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $< $(DEP) $(CFLAGS)

$(OUTEXE):
	@mkdir -p $@

$(OUTOBJ):
	@mkdir -p $@

$(OUTLIB):
	@mkdir -p $@

# Target: clean project.
clean:
ifneq ($(PRJSUB),)
	$(MAKE) -C $(PRJSUB) clean
endif
	@echo Cleaning objects
	@$(RM) -rf $(OUTOBJ)
	@$(RM) $(OUTEXE)/*.d

# Target: clean all.
distclean:
ifneq ($(PRJSUB),)
	$(MAKE) -C $(PRJSUB) distclean
endif
	@echo Cleaning all objects and executable
	@$(RM) $(OBJS)
	@$(RM) $(DEPS)

	@$(RM) -rf $(OUTLIB)
	@$(RM) -rf $(OUTEXE)
	@$(RM) -rf $(OUTOBJ)

# Listing of phony targets.
.PHONY : all clean $(DYNAMICLIB) $(STATICLIB) $(RUNNABLE)

-include subsys_config.mk

-include $(DEPS)
