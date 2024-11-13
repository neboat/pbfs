CC=gcc
CFLAGS= -O3
CILK=icc
CILKFLAGS= -Wall -Werror -O3
LDFLAGS= -L$(CURDIR)
AR=ar

all: bfs

bfs : bfs.cpp graph.cpp graph.h bag.cpp bag.h util.h Makefile
	$(CILK) $(CILKFLAGS) $@.cpp $(LDFLAGS) -o $@

clean :
	rm -f bfs *~
