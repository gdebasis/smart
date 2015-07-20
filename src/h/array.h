#ifndef ARRAYH
#define ARRAYH
/* A single array element. Array is sorted by index */
typedef struct {
  long index;
  long info;
} ARRAY;

int countSort(void *base, size_t nmemb, size_t size, int(*getkey)(const void *), int max);
#endif /* ARRAYH */
