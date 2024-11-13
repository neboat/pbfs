CXXFLAGS= -g -Wall -O3 -fopencilk -flto $(EXTRA_CFLAGS) # -falign-functions
LDFLAGS= -fopencilk -flto -fuse-ld=lld -O3 $(EXTRA_LDFLAGS)

all: bfs

%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $^

bfs : bfs.o graph.o
	$(CXX) $(LDFLAGS) -o $@ $^

clean :
	rm -f bfs *.o *.d *~
