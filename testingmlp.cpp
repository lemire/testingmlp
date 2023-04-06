#include "common.hpp"
#ifdef __linux__
#include "page-info.h"
#endif 
#include <chrono>
#ifdef __linux__
#include <malloc.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <random>

#include <sys/mman.h>

int getenv_int(const char *var, int def) {
    const char *val = getenv(var);
    return val ? atoi(val) : def;
}

bool getenv_bool(const char *var) {
    const char *val = getenv(var);
    return val && strcmp(val, "1") == 0;
}

const int do_csv       = getenv_int("MLP_CSV", 0);
const int do_distr     = getenv_int("MLP_DISTRIBUTION", 0);
const size_t len_start = getenv_int("MLP_START", 32 * 1024) * 1024ull; // 1 KiB
const size_t len_end   = getenv_int("MLP_STOP",  32 * 1024) * 1024ull; // 256 MiB


FILE* ifile = do_csv ? stderr : stdout;

#define printi(...) fprintf(ifile, __VA_ARGS__)

template <typename T>
std::tuple<T,T,T,T> compute_min_mean_std_max(const std::vector<T>& input) {

  T m = 0;
  T ma = input[0];
  T mi = input[0];

  for(T v : input) {
    if(ma < v) { ma = v; }
    if(mi > v) { mi = v; }
    m += v;
  }
  m /= input.size();

  T std = 0;
  for(T v : input) {
    std += (v - m) * (v - m) / input.size();
  }
  std = sqrt(std);
  return {mi, m, std, ma};
}

float time_one(const uint64_t* sp,
               const uint64_t *bigarray,
               size_t howmanyhits,
               size_t repeat,
               access_method_f *method,
               size_t lanes,
               float firsttime,
               float lasttime) {
  using namespace std::chrono;
  double mintime = 99999999999;
  uint64_t bogus = 0;
  std::vector<double> timings(repeat);
  for (size_t r = 0; r < repeat; r++) {
    auto begin_time = high_resolution_clock::now();
    bogus += method(sp, bigarray, howmanyhits);
    auto end_time = high_resolution_clock::now();
    double tv = duration<double>(end_time - begin_time).count();
    timings[r] = tv;
    if (tv < mintime) {
      mintime = tv;
    }
  }
  auto [min_timing, mean_timing, std_timing, max_timing]  = compute_min_mean_std_max(timings);
  if (bogus == 0x010101) {
    printf("ping!");
  }
  // compute the bandwidth
  size_t cachelineinbytes = 64;
  size_t rounded_hits = ( howmanyhits / lanes * lanes );
  size_t volume = rounded_hits * cachelineinbytes;
  double mbpers = volume / mintime / (1024.0 * 1024.);
  double nanoperquery = 1000 * 1000 *  1000 * mintime / rounded_hits;
  double expected = lasttime * (lanes - 1) / lanes;  // expected time if at max efficiency
  double efficiency = lanes == 1 ? 0 : 100.0 * (lasttime - mintime) / (lasttime - expected);
  double speedup = lanes == 1 ? 1 : firsttime / mintime;
  if (do_csv) {
    switch (do_csv) {
      case 1: printf(",%.1f", mbpers); break;
      case 2: printf(",%.3f", speedup); break;
      default: assert(false);
    }
  } else {
    printf("%12zu %12f %10.0f  %8.1f  %6.0f%%  %9.1f\n",
      lanes, mintime, round(mbpers), nanoperquery, efficiency, speedup);
  }
  if(do_distr) {
    double top_sigma = (max_timing - mean_timing) / std_timing;
    double bottom_sigma = (mean_timing - min_timing) / std_timing;
    printf("timing distribution: N: %zu, min: %.3f, mean: %.3f, std: %f,  max : %.3f, bottom sigma: %.1f, top sigma: %.1f \n",
      repeat, min_timing, mean_timing, std_timing, max_timing, bottom_sigma, top_sigma);
  }

  return mintime;
}

/* advance starting at p, n times */
size_t incr(const uint64_t* array, uint64_t p, size_t n) {
  while (n--) {
    p = array[p];
  }
  return p;
}

size_t cycle_dist(const uint64_t* array, uint64_t from, uint64_t to) {
  size_t dist = 0;
  while (from != to) {
    from = array[from];
    dist++;
  }
  return dist;
}

size_t cycle_total(const uint64_t* array) {
  auto first = array[0];
  return 1 + cycle_dist(array, first, 0);
}

/** make a cycle of length starting at element 0 of the given array */
void make_cycle(uint64_t* array, uint64_t* index, size_t length) {
// create a cycle of maximum length within the bigarray
  for (size_t i = 0; i < length; i++) {
    array[i] = i;
  }

  if (!do_csv) {
    printi("Applying Sattolo's algorithm... ");
    fflush(ifile);
  }
  // Sattolo
  std::mt19937_64 engine;
  engine.seed(0xBABE);
  for (size_t i = 0; i + 1 < length; i++) {
    std::uniform_int_distribution<size_t> dist{i + 1, length - 1};
    size_t swapidx = dist(engine);
    std::swap(array[i], array[swapidx]);
  }

  size_t total = 0;
  uint64_t cur = 0;
  do {
    index[total++] = cur;
    cur = array[cur];
    assert(cur < length);
  } while (cur != 0);

  if (!do_csv) printi("chain total: %zu\n", cycle_total(array));
  assert(total == length);
}

void setup_pointers(uint64_t* sp, const uint64_t* array, const uint64_t* index, size_t length, size_t mlp) {
  std::fill(sp, sp + NAKED_MAX, -1);
  sp[0] = 0;
  size_t totalinc = 0;
  for (size_t m = 1; m < mlp; m++) {
    totalinc += length / mlp;
    // sp[m] = incr(array, 0, totalinc);
    assert(totalinc < length);
    sp[m] = index[totalinc];
  }

  if (!do_csv && mlp > 1) {
    // printi("Verifying the neighboring distance... \n");
    size_t mind = -1, maxd = 0;
    for (size_t i = 0; i < mlp; i++) {
      bool last = (i + 1 == mlp);
      uint64_t from = sp[i];
      uint64_t to   = sp[last ? 0 : i + 1];
      size_t dist = cycle_dist(array, from, to);
      mind = std::min(mind, dist);
      maxd = std::max(maxd, dist);
    }
    // printi("inter-chain dists: ideal=%zu, min=%zu, max=%zu\n", length / mlp, mind, maxd);
    assert(mind >= length / mlp); // check that the min distance is as good as expected
  }
}

int naked_measure(uint64_t* bigarray, uint64_t* index, size_t length, size_t max_mlp) {

  make_cycle(bigarray, index, length);

  if (!do_csv) {
    uint64_t sum1 = 0, sum2 = 0, sum3 = 0, sum4 = 0;
    clock_t begin_time, end_time;
    int sumrepeat = 10;
    float mintime = 99999999999;
    while (sumrepeat-- >0) {
      begin_time = clock();
      for(size_t i = 0; i < length - 3 * 8; i+= 32) {
        sum1 ^= bigarray[i];
        sum2 ^= bigarray[i + 8];
        sum3 ^= bigarray[i + 16];
        sum4 ^= bigarray[i + 24];
      }
      end_time = clock();
      float tv = float(end_time - begin_time) / CLOCKS_PER_SEC;
      if (tv < mintime)
        mintime = tv;
    }
    if((sum1 ^ sum2 ^ sum3 ^ sum4) == 0x1010) printf("bug");
    printi("Time to sum up the array (linear scan) %.3f s (x 8 = %.3f s), bandwidth = %.1f MB/s \n",
        mintime, 8*mintime, length * sizeof(uint64_t) / mintime / (1024.0 * 1024.0));
  }

  float time_measure[NAKED_MAX] = {};
  size_t howmanyhits = length;
  int repeat = 30;
  if (do_csv) {
    printi("Running test for length %zu\n", length);
    // printf("lanes,time,bw,ns/hit,eff,speedup\n");
    printf("%zu", length);
  } else {
    printi("Size: %zu (%5.2f KiB, %5.2f MiB)\n", length, length * 8. / 1024., length * 8. / 1024. / 1024.);
    printi("---------------------------------------------------------------------\n");
    printi("- # of lanes --- time (s) ---- BandW -- ns/hit -- %% Eff -- Speedup --\n");
    printi("---------------------------------------------------------------------\n");
  }

  uint64_t starting_pointers[NAKED_MAX];

  // naked_measure_body(time_measure, bigarray, howmanyhits, repeat);
  for (size_t m = 1; m <= max_mlp; m++) {
    setup_pointers(starting_pointers, bigarray, index, length, m);
    time_measure[m] = time_one(starting_pointers, bigarray, howmanyhits, repeat, get_method(m), m, time_measure[1], time_measure[m - 1]);
  }

  if (do_csv) {
    printf("\n");
  } else {
    for (size_t i = 2; i < NAKED_MAX; i++) {
      float ratio = (time_measure[i - 1] - time_measure[i]) / time_measure[i - 1];

      if (ratio < 0.01) // if a new lane does not add at least 1% of performance...
      {
        std::cout << "Maybe you have about " << i - 1 << " parallel paths? "
                  << std::endl;
        return i - 1;
        break;
      }
    }
    printi("--------------------------------------------------------------\n");
  }

  return 10000;
}

#ifdef __linux__
void print_page_info(uint64_t *array, size_t length) {
  constexpr int KPF_THP = 22;
  page_info_array pinfo = get_info_for_range(array, array + length);
  flag_count thp_count = get_flag_count(pinfo, KPF_THP);
  if (thp_count.pages_available) {
    printi("Source pages allocated with transparent hugepages: %4.1f%% (%lu pages, %4.1f%% flagged)\n",
        100.0 * thp_count.pages_set / thp_count.pages_total, thp_count.pages_total,
        100.0 * thp_count.pages_available / thp_count.pages_total);
  } else {
    printi("Couldn't determine hugepage info (you are probably not running as root)\n");
  }
}
#endif

void *malloc_aligned(size_t size, size_t alignment) {
#ifdef __linux__
  size = ((size - 1) / alignment + 1) * alignment;
  return memalign(alignment, size);
#else
  return malloc(size); // ignores alignment?
#endif
}

int main() {
  assert(do_csv >= 0 && do_csv <= 2);
  size_t max_mlp = getenv_int("MLP_MAX_MLP", 40);
  printi("Initializing array made of %zu 64-bit words (%5.2f MiB).\n", len_end, len_end * 8. / 1024. / 1024.);
  uint64_t *array = (uint64_t *)malloc_aligned(sizeof(uint64_t) * len_end, 2 * 1024 * 1024);
#ifdef __linux__  
  madvise(array, len_end * 8, MADV_HUGEPAGE);
#endif
  std::fill(array, array + len_end, -1);
  uint64_t *index    = (uint64_t *)malloc_aligned(sizeof(uint64_t) * len_end, 2 * 1024 * 1024);
#ifdef __linux__  
  print_page_info(array, len_end);
#endif

  if (!do_csv) {
    printi("Legend:\n"
        "  BandW: Implied bandwidth (assuming 64-byte cache line) in MB/s\n"
        "  %% Eff: Effectiness of this lane count compared to the prior, as a %% of ideal\n"
        "  Speedup: Speedup factor for this many lanes versus one lane\n"
    );
  } else {
    printf("size,");
    for (size_t m = 1; m <= max_mlp; m++) {
      printf("%zu%s", m, m == max_mlp ? "\n" : ",");
    }
  }

  // for (size_t length = len_start; length <= len_end; length *= 2) {
  for (int i = 0; ; i++) {
    size_t length = round(len_start * pow(2., i / 4.));
    if (length > len_end)
      break;
    naked_measure(array, index, length, max_mlp);
  }
}
