#ifndef INVPOSH
#define INVPOSH
/*  $Header: /home/smart/release/src/h/invpos.h,v 11.2 1993/02/03 16:47:33 smart Exp $ */

typedef struct {
    long  list;
    float wt;
    int   pos;
} LISTWTPOS;

/* a single node visible to the user */
typedef struct {
    long  id_num;           /* key to access this inverted list with*/
    long  num_listwt;       /* Number of elements in this list */
    LISTWTPOS *listwtpos;   /* Pointer to actual list */
} INVPOS;

#endif /* INVPOSH */
