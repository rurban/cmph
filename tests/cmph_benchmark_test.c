#include <unistd.h>  // for sleep
#include <limits.h>

#include "cmph_benchmark.h"

int bm_sleep(int iters) {
  (void)iters;
  sleep(1);
  return 0;
}

int bm_increment(int iters) {
  int i, v = 0;
  (void)iters;
  for (i = 0; i < INT_MAX; ++i) {
    v += i;
  }
  return 0;
}

int main(void) {
  BM_REGISTER(bm_sleep, 1);
  BM_REGISTER(bm_increment, 1);
  run_benchmarks();
  return 0;
}

