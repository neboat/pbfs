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

#include "graph.h"
#include "util.h"
#include <cilk/cilk.h>
#include <cilk/cilkscale.h>
#include <cstdio>
#include <sys/time.h>
#include <sys/types.h>

using namespace std;

const bool DEBUG = false;
const int TRIALS = 10;

// Helper function for checking correctness of result
static bool check(unsigned int distances[], unsigned int distverf[],
                  int nodes) {
  for (int i = 0; i < nodes; i++) {
    if (distances[i] != distverf[i]) {
      fprintf(stderr, "distances[%d] = %d; distverf[%d] = %d\n", i,
              distances[i], i, distverf[i]);
      return false;
    }
  }

  return true;
}

unsigned long long todval(struct timeval *tp) {
  return tp->tv_sec * 1000 * 1000 + tp->tv_usec;
}

int main(int argc, char **argv) {
  Graph *graph;
  unsigned long long runtime_ms;

  BFSArgs bfsArgs = parse_args(argc, argv);

  if (DEBUG)
    printf("algorithm = %s\n", ALG_NAMES[bfsArgs.alg_select]);

  if (parseBinaryFile(bfsArgs.filename, &graph) != 0)
    return -1;

  // Initialize extra data structures
  int numNodes = graph->numNodes();
  unsigned int *distances = new unsigned int[numNodes];

  // Pick a starting node
  int s = 0;

  // Execute BFS
  for (int t = 0; t < TRIALS; ++t) {
    struct timeval t1, t2;
    wsp_t wsp1, wsp2;
    // cilk_for (int i = 0; i < numNodes; ++i) {
    //   distances[i] = UINT_MAX;
    // }
    switch (bfsArgs.alg_select) {
    case BFS:
      wsp1 = wsp_getworkspan();
      gettimeofday(&t1, 0);
      graph->bfs(s, distances);
      gettimeofday(&t2, 0);
      wsp2 = wsp_getworkspan();
      break;
    case PBFS:
      wsp1 = wsp_getworkspan();
      gettimeofday(&t1, 0);
      graph->pbfs(s, distances);
      gettimeofday(&t2, 0);
      wsp2 = wsp_getworkspan();
      break;
    // case PBFS_WLS:
    //   gettimeofday(&t1,0);
    //   graph->pbfs_wls(s, distances);
    //   gettimeofday(&t2,0);
    //   break;
    default:
      break;
    }

    runtime_ms = (todval(&t2) - todval(&t1)) / 1000;

    // Verify correctness
    if (bfsArgs.check_correctness) {

      unsigned int *distverf = new unsigned int[numNodes];
      // cilk_for (int i = 0; i < numNodes; ++i) {
      //   distverf[i] = UINT_MAX;
      // }
      graph->bfs(s, distverf);
      if (!check(distances, distverf, numNodes))
        fprintf(stderr, "Error found in %s result.\n",
                ALG_NAMES[bfsArgs.alg_select]);

      delete[] distverf;
    }

    // Print results if debugging
    if (DEBUG) {
      for (int i = 0; i < numNodes; i++)
        printf("Distance to node %d: %d\n", i + 1, distances[i]);
    }

    // Print runtime result
    switch (bfsArgs.alg_select) {
    case BFS:
      printf("BFS on %s: %f seconds\n", bfsArgs.filename.c_str(),
             runtime_ms / 1000.0);
      break;
    case PBFS:
      printf("PBFS on %s: %f seconds\n", bfsArgs.filename.c_str(),
             runtime_ms / 1000.0);
      break;
    // case PBFS_WLS:
    //   printf("PBFS_WLS on %s: %f seconds\n", bfsArgs.filename.c_str(),
    //   runtime_ms/1000.0); break;
    default:
      break;
    }

    wsp_dump(wsp2 - wsp1, "alg");
  }

  delete[] distances;
  delete graph;

  return 0;
}
