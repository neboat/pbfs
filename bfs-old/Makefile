CC=gcc
CFLAGS= -O3
CILK=$(HOME)/cilk/bin/cilk++
CILKFLAGS= -O3
LDFLAGS= -L$(CURDIR)
AR=ar

all: bfs

bfs : bfs.cilk graph.cilk graph.h bag.cpp bag.h util.h Makefile
	$(CILK) $(CILKFLAGS) $@.cilk $(LDFLAGS) -lmiser -lcilkutil -o $@

clean :
	rm bfs
