#ifndef DICTNOINFOH
#define DICTNOINFOH
/*        $Header: /home/smart/release/src/h/dict_noinfo.h,v 11.2 1993/02/03 16:47:56 smart Exp $*/

#ifndef S_RH_DICT_NOINFO
#include "rel_header.h"
#endif

typedef struct {
    char  *token;               /* actual string */
    long  con;                  /* unique index for this token */
} DICT_NOINFO;


typedef struct hash_entry {
    short collision_ptr;               /* offset of next word hashed here   */
                                       /* If IN_OVF_TABLE, then need to go to*/
                                       /* (next) overflow hash table */
    char prefix[2];                    /* First two bytes of string */
    long str_tab_off;                  /* position of string (byte offset)  */
} HASH_NOINFO;

/* Maximum value of collision_ptr. Constrained to be less than 2**15. */
#define MAX_DICT_OFFSET  (512)

#define IN_OVFL_TABLE -2
#define EMPTY_NODE(h) ((h)->str_tab_off == UNDEF)
#define VALID_DICT_INDEX(i) (i >= 0 && i < MAX_NUM_DICT && dict[i].opened)

#define DICT_MALLOC_SIZE (64 * 1024 - 40)

#define MAX_NUM_DICT 40

typedef struct {
    char file_name[PATH_LEN];
    int  fd;
    long file_size;

    HASH_NOINFO *hsh_table;
    char       *str_table;
    long hsh_tab_size;
    long str_tab_size;
    char *str_next_loc;

    HASH_NOINFO actual_hash_entry;
    HASH_NOINFO *current_hash_entry;
    long current_concept;
#define BEGIN_DICT_ENTRY 0
#define END_DICT_ENTRY   1
    char current_position;      /* if BEGIN, current_hash_entry is the valid */
				/* entry to be returned for next read or */
				/* write. If END, current_hash_entry is the */
				/* entry preceding the current position. */

    char opened;                /* Valid header if opened */
    char shared;                /* If 1, then buffers are shared. */
    long mode;
    short next_ovfl_index;
    char  next_ovfl_file_name[PATH_LEN];
    char  in_ovfl_file;
    REL_HEADER rh;
} _SDICT_NOINFO;
#endif /* DICTNOINFOH */
