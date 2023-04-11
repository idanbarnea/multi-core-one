#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef _MSC_VER
#include <intrin.h> /* for rdtscp and clflush */
#pragma optimize("gt",on)
#else
#include <x86intrin.h> /* for rdtscp and clflush */
#endif
#include <xmmintrin.h>

#define jump_size 512
#define four_k 4096

unsigned int array1_size = 16;
uint8_t unused1[64];
uint8_t array1[160] = {
  1,
  2,
  3,
  4,
  5,
  6,
  7,
  8,
  9,
  10,
  11,
  12,
  13,
  14,
  15,
  16
};
uint8_t unused2[64];
uint8_t array2[256 * jump_size];
uint8_t temp = 0; /* Used so compiler won’t optimize out victim_function() */

void victim_function(size_t x) {
  if (x < array1_size) {
    temp &= array2[array1[x] * jump_size];
  }
}


void with_intrinsic(){
        float value = 42;
    float stored = 14;
    printf("Before Stored Mem Value: %f\n", stored);
    __m128 xmm_value = _mm_set_ss(value);
    _mm_store_ss(&stored, xmm_value);
    printf("After Stored Mem Value: %f\n", stored);
    int loaded_value = _mm_cvtss_si32(_mm_load_ss((&stored)+4096));
    printf("Loaded Value: %d\n", loaded_value);
}

int main() {

    int value = 42;
    int loaded_value;

    // Store value to memory location
    __asm__ __volatile__ (
        "movl %0, %%eax\n\t"
        "movl %%eax, (%1)\n\t"
        :
        : "r"(value), "r"(&loaded_value)
        : "%eax", "memory"
    );

    // Load value from memory location
    __asm__ __volatile__ (
        "movl (%0), %%eax\n\t"
        "movl %%eax, %1\n\t"
        :
        : "r"(&loaded_value), "r"(loaded_value)
        : "%eax", "memory"
    );

    printf("Value: %d\n", loaded_value);

    return 0;
}