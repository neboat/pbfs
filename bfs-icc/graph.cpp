// Copyright (c) 2010, Tao B. Schardl
/*
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

// Graph Representation
#ifndef GRAPH_CPP
#define GRAPH_CPP

#include <assert.h>
#include <vector>
#include <sys/types.h>
#include <climits>
#include <cilk/cilk.h>
#include <cstdlib>

#include "graph.h"
#include "bag.h"

#define GraphDebug 0
#define RAND 0

#define THRESHOLD 256
extern "C" void trivial(void);

//extern cilk::cilkview cv;

Graph::Graph(int *ir, int *jc, int m, int n, int nnz)
{
  this->nNodes = m;
  this->nEdges = nnz;

  this->nodes = new int[m+1];
  this->edges = new int[nnz];

  int *w = new int[m];
  //int *v = new int[m];
  for (int i = 0; i < m; ++i) {
    w[i] = 0;
  }

  for (int i = 0; i < jc[n]; ++i)
    w[ir[i]]++;

  int prev;
  int tempnz = 0;
  for (int i = 0; i < m; ++i) {
    prev = w[i];
    w[i] = tempnz;
    tempnz += prev;
  }
  this->nodes[m] = tempnz;
  //memcpy(this->nodes, w, sizeof(int) * m);
  for (int i = 0; i < m; ++i)
    this->nodes[i] = w[i];
  //memcpy(v, w, m);
  
  for (int i = 0; i < n; ++i) {
    for (int j = jc[i]; j < jc[i+1]; j++)
      this->edges[w[ir[j]]++] = i;
  }

  delete[] w;
  //delete[] v;
}

Graph::~Graph()
{
  delete[] this->nodes;
  delete[] this->edges;
}

int
Graph::bfs(const int s, unsigned int distances[])
const
{
  unsigned int *queue = new unsigned int[nNodes];
  unsigned int head, tail;
  unsigned int current, newdist;

  for (int i = 0; i < nNodes; ++i) {
    distances[i] = UINT_MAX;
  }

  if (s < 0 || s > nNodes)
    return -1;

  current = s;
  distances[s] = 0;
  head = 0;
  tail = 0;

  do {
    newdist = distances[current]+1;
    int edgeZero = nodes[current];
    int edgeLast = nodes[current+1];
    int edge;
    uint compare;
    uint currentDist;
    for (int i = edgeZero; i < edgeLast; i++) {
      edge = edges[i];
      // currentDist = distances[edge];
      // compare = (newdist < currentDist);
      if (newdist < distances[edge]) {
      	queue[tail++] = edge;
      	distances[edge] = newdist;
      }
      // distances[edge] = currentDist ^ ((currentDist ^ newdist) & -compare);
      // queue[tail] = edge;
      // tail += compare;
    }
    current = queue[head++];
  } while (head <= tail);

  delete[] queue;

  return 0;
}

static inline void
pbfs_proc_Node(const int n[],
	       int fillSize,
	       Bag_reducer<int> *next,
	       uint newdist,
	       uint distances[],
	       const int nodes[],
	       const int edges[])
{
  // Process the current element
  Bag<int>* bnext = &((*next).get_reference());
  for (int j = 0; j < fillSize; ++j) { 
    // Scan the edges of the current node and add untouched
    // neighbors to the opposite bag
    int edgeZero = nodes[n[j]];
    int edgeLast = nodes[n[j]+1];
    int edge;

    for (int i = edgeZero; i < edgeLast; ++i) {
      edge = edges[i];
      if (newdist < distances[edge]) {
	(*bnext).insert(edge);
	//(*next).insert(edge);
	distances[edge] = newdist;
      }
    }
  }
}

inline void
Graph::pbfs_walk_Bag(Bag<int> *b,
		     Bag_reducer<int>* next,
		     unsigned int newdist,
		     unsigned int distances[])
  const
{
  if (b->getFill() > 0) {
    // Split the bag and recurse
    Pennant<int> *p = NULL;

    b->split(&p); // Destructive split, decrements b->getFill()
    cilk_spawn pbfs_walk_Bag(b, next, newdist, distances);
    pbfs_walk_Pennant(p, next, newdist, distances);

    cilk_sync;

  } else {
    //pbfs_walk_Pennant(b->getFilling(), next, newdist, distances, b->getFillingSize());
    //pbfs_proc_Node(b->getFilling(), next, newdist, distances, b->getFillingSize());
    //pbfs_proc_Filling(b->getFilling(), next, newdist, distances, b->getFillingSize());
    int fillSize = b->getFillingSize();
    const int *n = b->getFilling();
    // Bag<int>* bnext = &((*next).get_reference());
    // for (int j = 0; j < fillSize; ++j) { 
    //   // Scan the edges of the current node and add untouched
    //   // neighbors to the opposite bag
    //   int edgeZero = nodes[n[j]];
    //   int edgeLast = nodes[n[j]+1];

    //   for (int i = edgeZero; i < edgeLast; ++i) {
    // 	if (newdist < distances[edges[i]]) {
    // 	  (*bnext).insert(edges[i]);
    // 	  //(*next).insert(edges[i]);
    // 	  distances[edges[i]] = newdist;
    // 	}
    //   }
    // }
    int extraFill = fillSize % THRESHOLD;
    cilk_spawn pbfs_proc_Node(n+fillSize-extraFill, extraFill,
			      next, newdist, distances,
			      nodes, edges);
    #pragma cilk grainsize = 1
    cilk_for (int i = 0; i < fillSize - extraFill; i += THRESHOLD) {
      pbfs_proc_Node(n+i, THRESHOLD,
		     next, newdist, distances,
		     nodes, edges);
    }
    cilk_sync;
  }
}

inline void
Graph::pbfs_walk_Pennant(Pennant<int> *p,
			 Bag_reducer<int>* next,
			 unsigned int newdist,
			 unsigned int distances[])
  const
{
  if (p->getLeft() != NULL)
    cilk_spawn pbfs_walk_Pennant(p->getLeft(), next, newdist, distances);

  if (p->getRight() != NULL)
    cilk_spawn pbfs_walk_Pennant(p->getRight(), next, newdist, distances);

  // // Process the current element
  // //pbfs_proc_Node(p->getElements(), next, newdist, distances);
  const int *n = p->getElements();
  // Bag<int>* bnext = &((*next).get_reference());
  // for (int j = 0; j < BLK_SIZE; ++j) { 
  //   // Scan the edges of the current node and add untouched
  //   // neighbors to the opposite bag
  //   int edgeZero = nodes[n[j]];
  //   int edgeLast = nodes[n[j]+1];

  //   for (int i = edgeZero; i < edgeLast; ++i) {
  //     if (newdist < distances[edges[i]]) {
  // 	(*bnext).insert(edges[i]);
  // 	//(*next).insert(edges[i]);
  // 	distances[edges[i]] = newdist;
  //     }
  //   }
  // }
  #pragma cilk grainsize=1
  cilk_for (int i = 0; i < BLK_SIZE; i+=THRESHOLD) {
    // This is fine as long as THRESHOLD divides BLK_SIZE
    pbfs_proc_Node(n+i, THRESHOLD,
		   next, newdist, distances,
		   nodes, edges);
  }
  delete p;
}

int
Graph::pbfs(const int s, unsigned int distances[]) const
{
  Bag_reducer<int> *queue[2];
  Bag_reducer<int> b1;
  Bag_reducer<int> b2;
  queue[0] = &b1;
  queue[1] = &b2;

  bool queuei = 1;
  unsigned int current, newdist;

  if (s < 0 || s > nNodes)
    return -1;

  cilk_for (int i = 0; i < nNodes; ++i) {
    distances[i] = UINT_MAX;
  }

  distances[s] = 0;

  // Scan the edges of the initial node and add untouched
  // neighbors to the opposite bag
  cilk_for (int i = nodes[s]; i < nodes[s+1]; ++i) {
    if (edges[i] != s) {
      (*queue[queuei]).insert(edges[i]);
      distances[edges[i]] = 1;
    }
  }
  newdist = 2;

  while (!((*queue[queuei]).isEmpty())) {
    (*queue[!queuei]).clear();
    pbfs_walk_Bag(&((*queue[queuei]).get_reference()), queue[!queuei], newdist, distances);
    queuei = !queuei;
    ++newdist;
  }

  return 0;
}

#endif
