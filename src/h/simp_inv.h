#ifndef SIMP_INVH
#define SIMP_INVH
/*  $Header: /home/smart/release/src/h/simp_inv.h,v 11.2 1993/02/03 16:47:43 smart Exp $ */

/* a single node visible to the user */
typedef struct {
        long  id_num;           /* key to access this inverted list with*/
        long  num_list;         /* Number of elements in this list */
        long  *list;            /* pointer to list elements  */
} SIMP_INV;

#endif /* SIMP_INVH */
