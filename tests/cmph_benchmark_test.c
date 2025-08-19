#include <unistd.h>  // for sleep
#include <limits.h>

#include "cmph_benchmark.h"

void bm_sleep(int iters) {
  (void)iters;
  sleep(1);
}

void bm_increment(int iters) {
  int i, v = 0;
  (void)iters;
  for (i = 0; i < INT_MAX; ++i) {
    v += i;
  }
}

int main(void) {
  BM_REGISTER(bm_sleep, 1);
  BM_REGISTER(bm_increment, 1);
  run_benchmarks();
  return 0;
}

