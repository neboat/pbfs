# CC=gcc
# TAPIR_BASE ?= /data/compilers/tapir/src-release_80
# TAPIR_ROOT ?= $(TAPIR_BASE)/build-debug
# CFLAGS= -O3
# CILK=icpc
# CILK=$(TAPIR_ROOT)/bin/clang++
CXX=clang++
# CILKFLAGS= -g -Wall -Werror -O3 -fcilkplus -ldl $(EXTRA_CFLAGS) # -falign-functions
CILKFLAGS= -g -Wall -O3 -fopencilk -ldl $(EXTRA_CFLAGS) # -falign-functions
LDFLAGS= -L$(CURDIR) $(EXTRA_LDFLAGS)
AR=ar

all: bfs

bfs : bfs.cpp graph.cpp graph.h bag.cpp bag.h util.h Makefile
	$(CXX) $(CILKFLAGS) $@.cpp $(LDFLAGS) -o $@

clean :
	rm -f bfs *~
