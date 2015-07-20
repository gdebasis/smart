#ifndef DIR_ARRAYH
#define DIR_ARRAYH
/*  $Header: /home/smart/release/src/h/dir_array.h,v 11.2 1993/02/03 16:47:43 smart Exp $ */

/* a single node visible to the user */
typedef struct {
        long  id_num;           /* key to access this inverted list with*/
        long  num_list;         /* Number of elements in this list */
        char  *list;            /* pointer to list elements  */
} DIR_ARRAY;

#define REL_DIR_ARRAY 16
#endif /* DIR_ARRAYH */
