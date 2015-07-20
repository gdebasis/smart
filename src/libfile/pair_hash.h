#ifndef PAIR_HASHH
#define PAIR_HASHH
/*        $Header: /home/smart/release/src/h/pair_hash.h,v 11.2 1993/02/03 16:47:24 smart Exp $*/

#ifndef S_RH_DICT
#include "rel_header.h"
#endif

#include "hash.h"



typedef struct {
    long  *pair_ptr;            /* pointer to array of length two */
    long info;                  /* Usage dependent field */
    long  con;                  /* unique index for this token */
} PAIR_HASH_ENTRY;

long pair_hash_hash_func();
int pair_hash_compare_hash_entry();
int pair_hash_enter_string_table();
int pair_hash_get_var_entry();

int create_hash(), open_hash(), seek_hash(), read_hash(), write_hash(),
    close_hash();

#define create_pair_hash(file_name, rh) create_hash (file_name, rh)
    
#define open_pair_hash(file_name, mode)  open_hash (file_name, mode)

#define seek_pair_hash(pair_hash_index, entry)  \
     seek_hash (pair_hash_index, (HASH_ENTRY *) entry, \
		pair_hash_compare_hash_entry, pair_hash_hash_func)

#define read_pair_hash(pair_hash_index, pair_hash_entry) \
     read_hash (pair_hash_index, (HASH_ENTRY *) pair_hash_entry, pair_hash_get_var_entry)

#define write_pair_hash(pair_hash_index, pair_hash_entry) \
     write_hash (pair_hash_index, (HASH_ENTRY *)pair_hash_entry, pair_hash_enter_string_table)

#define close_pair_hash(pair_hash_index) close_hash (pair_hash_index)

#endif /* PAIR_HASHH */
