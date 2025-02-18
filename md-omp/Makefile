#===============================================================================
# User Options
#===============================================================================

# Compiler can be set below, or via environment variable
CC        = icpx
OPTIMIZE  = yes
DEBUG     = no
SP        = yes
DEVICE    = gpu


#===============================================================================
# Program name & source code list
#===============================================================================

program = main

source = main.cpp

obj = $(source:.cpp=.o)

#===============================================================================
# Sets Flags
#===============================================================================

# Standard Flags
CFLAGS := $(EXTRA_CFLAGS) -std=c++14 -I../md-cuda

# Linker Flags
LDFLAGS = 

# Debug Flags
ifeq ($(DEBUG),yes)
  CFLAGS += -g
  LDFLAGS  += -g
endif


# Optimization Flags
ifeq ($(OPTIMIZE),yes)
  CFLAGS += -O3
endif

ifeq ($(SP),yes)
  CFLAGS += -DSINGLE_PRECISION
endif

ifeq ($(DEVICE),gpu)
  CFLAGS +=-fiopenmp -fopenmp-targets=spir64 -D__STRICT_ANSI__
else
  CFLAGS +=-qopenmp
endif
#===============================================================================
# Targets to Build
#===============================================================================

$(program): $(obj)
	$(CC) $(CFLAGS) $(obj) -o $@ $(LDFLAGS)

%.o: %.cpp MD.h ../md-cuda/reference.h ../md-cuda/utils.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(program) $(obj)

run: $(program)
	./$(program) 3 1000

