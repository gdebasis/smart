#ifndef HASHH
#define HASHH
/*        $Header: /home/smart/release/src/h/hash.h,v 11.2 1993/02/03 16:47:24 smart Exp $*/

#ifndef S_RH_HASH
#include "rel_header.h"
#endif

typedef struct {
    char  *token;               /* actual string */
    long info;                  /* Usage dependent field */
    long  con;                  /* unique index for this token */
} HASH_ENTRY;


typedef struct hashtab_entry {
    short collision_ptr;               /* offset of next word hashed here   */
                                       /* If IN_OVF_TABLE, then need to go to*/
                                       /* (next) overflow hash table */
    short prefix;                      /* Two bytes of entry->token */
    long str_tab_off;                  /* offset of var (string(byte offset))*/
    long info;                         /* Usage dependent field */
} HASHTAB_ENTRY;

/* Maximum value of collision_ptr. Constrained to be less than 2**15. */
#define MAX_HASH_OFFSET  (512)

#define IN_OVFL_TABLE -2
#define EMPTY_NODE(h) ((h)->str_tab_off == UNDEF)
#define VALID_HASH_INDEX(i) (i >= 0 && i < MAX_NUM_HASH && hash[i].opened)

#define HASH_MALLOC_SIZE (64 * 1024 - 40)

#define MAX_NUM_HASH 40

#define GET_HASH_NODE(concept,hash_ptr,hash_node) \
    if (concept < 0 || concept >= hash_ptr->hsh_tab_size) \
        hash_node = NULL; \
    else { \
        if (hash_ptr->mode & SINCORE) { \
            hash_node = &hash_ptr->hsh_table[concept]; \
        } \
        else { \
            if (-1 == lseek (hash_ptr->fd, \
                             (long) (sizeof (REL_HEADER) + \
                                     (concept * sizeof (HASHTAB_ENTRY))), \
                             0) || \
                sizeof (HASHTAB_ENTRY) != read ( hash_ptr->fd, \
                                      (char *) &(hash_ptr->actual_hashtab_entry),\
                                      sizeof(HASHTAB_ENTRY))) { \
                set_error (errno, hash_ptr->file_name, "hash");\
                return (UNDEF); \
            } \
           hash_node = &(hash_ptr->actual_hashtab_entry); \
        } \
    }


typedef struct {
    char file_name[PATH_LEN];
    int  fd;
    long file_size;

    HASHTAB_ENTRY *hsh_table;
    char       *str_table;
    long hsh_tab_size;
    long str_tab_size;
    char *str_next_loc;

    HASHTAB_ENTRY actual_hashtab_entry;
    HASHTAB_ENTRY *current_hashtab_entry;
    long current_concept;
    char current_token[MAX_TOKEN_LEN]; /* Buffer to read current token into */
    long mode;
#define BEGIN_HASH_ENTRY 0
#define END_HASH_ENTRY   1
    char current_position;      /* if BEGIN, current_hashtab_entry is the valid */
				/* entry to be returned for next read or */
				/* write. If END, current_hashtab_entry is the */
				/* entry preceding the current position. */

    char opened;                /* Valid header if opened */
    char shared;                /* If 1, then buffers are shared. */
    char  in_ovfl_file;
    short next_ovfl_index;
    char  next_ovfl_file_name[PATH_LEN];
    REL_HEADER rh;
} _SHASH;
#endif /* HASHH */
