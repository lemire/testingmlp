#!/usr/bin/env python3

print("#include \"common.hpp\"\n#include \"math.h\"\n")

for i in range(1,100):
    print("uint64_t naked_access_%d(const uint64_t *sp, const uint64_t *bigarray, size_t howmanyhits) {"%(i))
    for j in range(i):
        print("  uint64_t val%d = sp[%d];" %(j,j))
    print ("  size_t howmanyhits_perlane = howmanyhits / %d;"%(i))
    print("  for (size_t counter = 0; counter < howmanyhits_perlane; counter++) {")
    for j in range(i):
        print("    val%d = bigarray[val%d];" %(j,j))
    print("  }")
    print("  return ", "+".join("val%d"%j for j in range(i)), ";")
    print("}")

# make the method array
print('access_method_f * all_methods[] = {')
print(',\n'.join("\tnaked_access_%d"%i for i in range(1,100)))
print('\n};')
