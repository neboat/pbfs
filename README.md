# README

The bfs program reads in a file storing a binary representation of a graph and executes either BFS or PBFS on that graph.

## Usage

```console
./bfs [-f <filename>] [-a <algorithm>] [-c]
Flags are:
	-f <filename>	: Specify the name of the test file to use.
	-a <algorithm>	: Specify the BFS algorithm to use.
	Valid values for <algorithm> are:
		b for Serial BFS
		p for PBFS (default)
	-c		: Check result for correctness.
```

## Compilation

To compile the bfs executable, simply run:

```console
make
```

The Makefile assumes you have OpenCilk `clang++` and `lld` present in your path for compilation to succeed.  You can specify a custom path to OpenCilk `clang++` by setting the `CXX` variable as follows:

```console
make CXX=/path/to/opencilk/bin/clang++
```

You can also set the `EXTRA_CFLAGS` and `EXTRA_LDFLAGS` variables to add or modify the compilation and linking flags, respectively.  For example, the following command builds this code with [TCMalloc](https://github.com/google/tcmalloc), if it's installed on your system:

```console
make EXTRA_LDFLAGS="-ltcmalloc"
```

To cleanup the results of make, run:

```console
make clean
```

## Graph input files

The input file to bfs is a binary file with the following format:

```text
<#rows><#columns><#non-zeros><row vector><column vector><logical array>
```

All values except for those in the logical array stored in the binary file are unsigned 32-bit integers.  The values in the logical array are stored as doubles.  Such a file may be generated for the graph stored in the matrix A using the following MATLAB code*:

```Matlab
[i, j, v] = find(A); 

fwrite(f, size(A,1), 'uint32');
fwrite(f, size(A,2), 'uint32');
fwrite(f, nnz(A), 'uint32');

fwrite(f, (i-1), 'uint32');
fwrite(f, (j-1), 'uint32');
fwrite(f, v, 'double');
```

For convenience, this MATLAB code is reproduced in the included MATLAB function `dumpbinsparse(A, output)`, which outputs the matrix `A` into the binary file `output` in the correct format.

## Acknowledgments

Thanks to Aydin Buluc for providing this MATLAB code for creating valid input graphs.

To cite this work, please cite the SPAA paper:
> Charles E. Leiserson and Tao B. Schardl. 2010. A work-efficient parallel breadth-first search algorithm (or how to cope with the nondeterminism of reducers). In Proceedings of the twenty-second annual ACM symposium on Parallelism in algorithms and architectures (SPAA '10). Association for Computing Machinery, New York, NY, USA, 303–314. https://doi.org/10.1145/1810479.1810534

```bibtex
@inproceedings{LeisersonSc10,
  author = {Leiserson, Charles E. and Schardl, Tao B.},
  title = {A work-efficient parallel breadth-first search algorithm (or how to cope with the nondeterminism of reducers)},
  year = {2010},
  isbn = {9781450300797},
  publisher = {Association for Computing Machinery},
  address = {New York, NY, USA},
  url = {https://doi.org/10.1145/1810479.1810534},
  doi = {10.1145/1810479.1810534},
  booktitle = {Proceedings of the Twenty-Second Annual ACM Symposium on Parallelism in Algorithms and Architectures},
  pages = {303–-314},
  numpages = {12},
  keywords = {breadth-first search, cilk, graph algorithms, hyperobjects, multithreading, nondeterminism, parallel algorithms, reducers, work-stealing},
  location = {Thira, Santorini, Greece},
  series = {SPAA '10}
}
```
