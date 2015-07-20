#ifndef __STEM_H
#define __STEM_H

#include "uthash.h"

typedef struct {
    char *original, *stemmed;
    UT_hash_handle hh;	// a more efficient version uses a hash table instead of a sorted array.
} WORD_STEM;

#endif

