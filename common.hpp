#ifndef MLP_COMMON_H_
#define MLP_COMMON_H_

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <iostream>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

/* max MLP value tested with the naked strategy */
constexpr int NAKED_MAX = 100;

void naked_measure_body(float (&time_measure)[NAKED_MAX], uint64_t *bigarray, size_t howmanyhits, size_t repeat);

typedef uint64_t (access_method_f)(const uint64_t* sp, const uint64_t *bigarray, size_t howmanyhits);

/** get the method implementing mlp_count access chains, up to NAKED_MAX */
extern access_method_f * all_methods[];

static inline access_method_f * get_method(size_t mlp) {
    assert(mlp >= 1 && mlp < NAKED_MAX);
    return all_methods[mlp - 1];
}

#endif
