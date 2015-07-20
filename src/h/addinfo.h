#ifndef ADDINFOH
#define ADDINFOH
/*  $Header: /home/smart/release/src/h/addinfo.h,v 11.2 1993/02/03 16:47:21 smart Exp $ */

/* same as INV and VEC, but with additional information */
/* WARNING: At the moment, this is not fully incorporated into the system. */
/* In particular, you must include "vector.h" before including this file */

typedef struct {
        long ainfo1;
        long ainfo2;
        long ainfo3;
        long ainfo4;
} ADDINFO;

#define a_para_num  ainfo1;
#define a_sent_num  ainfo2;
#define a_word_num  ainfo3;

/* a single inverted node visible to the user */
typedef struct {
        long  key_num;          /* key to access this inverted list with*/
        long num_list;          /* Number of elements in this list */
        long  *list;            /* pointer to list elements  */
        float *list_weights;    /* Pointer to weights for corresponding */
                                /* elements */
        ADDINFO *addinfo;       /* Pointer to addinfo record */
} ADDINV;

/* a single vector, sorted on: ctypes,cons */
typedef struct {
        long   id_num;          /* unique number for this vector within  */
                                /* collection */
        long   num_ctype;       /* number of ctypes for this vector */
        long   num_conwt;       /* no. of tuples in the vector */
        long   *ctype_len;      /* length of subvector for each ctype */
        CON_WT *con_wtp;        /* pointer to concepts, weights for vector*/

        ADDINFO *addinfo;       /* Pointer to addinfo record */
} ADDVEC;
#endif /* ADDINFOH */
