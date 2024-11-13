// Copyright (c) 2010-2024, Tao B. Schardl
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
#include "graph.h"
#include "bag.h"
#include <cilk/cilk.h>
#include <cilk/cilksan.h>
#include <climits>
#include <cstdlib>
#include <sys/types.h>

#define GraphDebug 0
#define RAND 0

#define THRESHOLD 256
#define EDGE_THRESHOLD 128
#define PARALLEL_EDGES false

Graph::Graph(int *ir, int *jc, int m, int n, int nnz) {
  this->nNodes = m;
  this->nEdges = nnz;

  this->nodes = new int[m + 1];
  this->edges = new int[nnz];

  int *w = new int[m];
  // int *v = new int[m];
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
  // memcpy(this->nodes, w, sizeof(int) * m);
  for (int i = 0; i < m; ++i)
    this->nodes[i] = w[i];
  // memcpy(v, w, m);

  for (int i = 0; i < n; ++i) {
    for (int j = jc[i]; j < jc[i + 1]; j++)
      this->edges[w[ir[j]]++] = i;
  }

  delete[] w;
  // delete[] v;
}

Graph::~Graph() {
  delete[] this->nodes;
  delete[] this->edges;
}

int Graph::bfs(const int s, unsigned int distances[]) const {
  unsigned int *queue = new unsigned int[nNodes];
  unsigned int head, tail;
  unsigned int current, newdist;

  if (s < 0 || s > nNodes)
    return -1;

  for (int i = 0; i < nNodes; ++i) {
    distances[i] = UINT_MAX;
  }

  current = s;
  distances[s] = 0;
  head = 0;
  tail = 0;

  do {
    newdist = distances[current] + 1;
    int edgeZero = nodes[current];
    int edgeLast = nodes[current + 1];
    for (int i = edgeZero; i < edgeLast; i++) {
      int edge = edges[i];
      if (newdist < distances[edge]) {
        queue[tail++] = edge;
        distances[edge] = newdist;
      }
    }
    current = queue[head++];
  } while (head <= tail);

  delete[] queue;

  return 0;
}

// Fake lock to ignore known races on reading and writing distances[] array.
Cilksan_fake_mutex mtx;

static inline void pbfs_proc_Node(const int n[], int fillSize,
                                  Bag_red<int> &next, uint newdist,
                                  uint distances[], const int nodes[],
                                  const int edges[]) {
  // Process the current element
  // Bag<int> &bnext = *&next;
  for (int j = 0; j < fillSize; ++j) {
    // Scan the edges of the current node and add untouched
    // neighbors to the opposite bag
    int edgeZero = nodes[n[j]];
    int edgeLast = nodes[n[j] + 1];

#if PARALLEL_EDGES
    cilk_for(int ii = 0;
             ii < (edgeLast - edgeZero + EDGE_THRESHOLD) / EDGE_THRESHOLD;
             ++ii) {
      int localEdgeZero = edgeZero + (ii * EDGE_THRESHOLD);
      int localEdgeLast =
          std::min(edgeLast, edgeZero + (EDGE_THRESHOLD * (ii + 1)));
#else
    {
      int localEdgeZero = edgeZero;
      int localEdgeLast = edgeLast;
#endif
      Bag<int> &bnext = *&next;
      for (int i = localEdgeZero; i < localEdgeLast; ++i) {
        // Ignore races on distances[edge]
        Cilksan_fake_lock_guard guard(&mtx);
        int edge = edges[i];
        if (newdist < distances[edge]) {
          bnext.insert(edge);
          distances[edge] = newdist;
        }
      }
    }
  }
}

inline void Graph::pbfs_walk_Bag(Bag<int> &b, Bag_red<int> &next,
                                 unsigned int newdist,
                                 unsigned int distances[]) const {
  if (b.getFill() > 0) {
    // Split the bag and recurse
    Pennant<int> *p = NULL;

    b.split(&p); // Destructive split, decrements b.getFill()
    cilk_spawn pbfs_walk_Pennant(p, next, newdist, distances);
    pbfs_walk_Bag(b, next, newdist, distances);
  } else {
    int fillSize = b.getFillingSize();
    const int *n = b.getFilling();
    int extraFill = fillSize % THRESHOLD;
    cilk_spawn pbfs_proc_Node(n + fillSize - extraFill, extraFill, next,
                              newdist, distances, nodes, edges);
    // #pragma cilk grainsize 1
    cilk_for(int i = 0; i < fillSize - extraFill; i += THRESHOLD) {
      pbfs_proc_Node(n + i, THRESHOLD, next, newdist, distances, nodes, edges);
    }
  }
}

inline void Graph::pbfs_walk_Pennant(Pennant<int> *p, Bag_red<int> &next,
                                     unsigned int newdist,
                                     unsigned int distances[]) const {
  cilk_scope {
    if (p->getLeft() != NULL)
      cilk_spawn pbfs_walk_Pennant(p->getLeft(), next, newdist, distances);

    if (p->getRight() != NULL)
      cilk_spawn pbfs_walk_Pennant(p->getRight(), next, newdist, distances);

    const int *n = p->getElements();
    // #pragma cilk grainsize 1
    cilk_for(int i = 0; i < BLK_SIZE; i += THRESHOLD) {
      // This is fine as long as THRESHOLD divides BLK_SIZE
      pbfs_proc_Node(n + i, THRESHOLD, next, newdist, distances, nodes, edges);
    }
  }
  delete p;
}

int Graph::pbfs(const int s, unsigned int distances[]) const {
#if REDUCER_IMPL == REDUCER_ARRAY
  Bag_red<int> queue[2];
#else // REDUCER_IMPL == REDUCER_PTRS
  Bag_red<int> *queue[2];
  Bag_red<int> b1;
  Bag_red<int> b2;
  queue[0] = __builtin_addressof(b1);
  queue[1] = __builtin_addressof(b2);
#endif

  bool queuei = 1;
  // unsigned int current;
  unsigned int newdist;

  if (s < 0 || s > nNodes)
    return -1;

  cilk_for(int i = 0; i < nNodes; ++i) distances[i] = UINT_MAX;

  distances[s] = 0;

  // Scan the edges of the initial node and add untouched
  // neighbors to the opposite bag
  cilk_for(int i = nodes[s]; i < nodes[s + 1]; ++i) {
    if (edges[i] != s) {
#if REDUCER_IMPL == REDUCER_PTRS
      queue[queuei]->insert(edges[i]);
#else  // REDUCER_IMPL == REDUCER_ARRAY or REDUCER_IMPL == REDUCER_ORIG
      queue[queuei].insert(edges[i]);
#endif // REDUCER_IMPL
      distances[edges[i]] = 1;
    }
  }
  newdist = 2;

#if REDUCER_IMPL == REDUCER_PTRS
  while (!(queue[queuei]->isEmpty()))
#else
  while (!(queue[queuei].isEmpty()))
#endif // REDUCER_IMPL
  {
#if REDUCER_IMPL == REDUCER_PTRS
    queue[!queuei]->clear();
    pbfs_walk_Bag(*queue[queuei], *queue[!queuei], newdist, distances);
#else  // REDUCER_IMPL == REDUCER_ARRAY
    queue[!queuei].clear();
    pbfs_walk_Bag(*&queue[queuei], queue[!queuei], newdist, distances);
#endif // REDUCER_IMPL
    queuei = !queuei;
    ++newdist;
  }

  return 0;
}
