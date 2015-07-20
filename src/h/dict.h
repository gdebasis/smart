#ifndef DICTH
#define DICTH
/*        $Header: /home/smart/release/src/h/dict.h,v 11.2 1993/02/03 16:47:24 smart Exp $*/

#ifndef S_RH_DICT
#include "rel_header.h"
#endif

#include "hash.h"

typedef struct {
    char  *token;               /* actual string */
    long info;                  /* Usage dependent field */
    long  con;                  /* unique index for this token */
} DICT_ENTRY;

long dict_hash_func();
int dict_compare_hash_entry();
int dict_enter_string_table();
int dict_get_var_entry();

int create_hash(), open_hash(), seek_hash(), read_hash(), write_hash(),
    close_hash(), destroy_hash();

#define create_dict(file_name, rh) create_hash (file_name, rh)
    
#define open_dict(file_name, mode)  open_hash (file_name, mode)

#define seek_dict(dict_index, entry) \
	seek_hash (dict_index, (HASH_ENTRY *) entry)

#define read_dict(dict_index, dict_entry) \
	read_hash (dict_index, (HASH_ENTRY *) dict_entry)

#define write_dict(dict_index, dict_entry) \
     write_hash (dict_index, (HASH_ENTRY *) dict_entry)

#define close_dict(dict_index) close_hash (dict_index)

#define destroy_dict(file_name) destroy_hash (file_name)

#endif /* DICTH */
