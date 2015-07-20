#ifndef VECTORH
#define VECTORH
/*  $Header: /home/smart/release/src/h/vector.h,v 11.2 1993/02/03 16:47:54 smart Exp $ */

#include "eid.h"

typedef struct {
    long con;                   /* Actual concept number */
    float wt;                   /* and its weight */
} CON_WT;

/* a single vector, sorted on: ctypes,cons */
typedef struct {
        EID    id_num;          /* unique number for this vector within  */
                                /* collection */
        long   num_ctype;       /* number of ctypes for this vector */
        long   num_conwt;       /* no. of tuples in the vector */
        long   *ctype_len;      /* length of subvector for each ctype */
        CON_WT *con_wtp;        /* pointer to concepts, weights for vector*/

} VEC;

/* Multiple vectors */
typedef struct {
    VEC *vec;
    long num_vec;
    EID id_num;
} VEC_LIST;

typedef struct {
    VEC_LIST *vec_list1;
    VEC_LIST *vec_list2;
} VEC_LIST_PAIR;

typedef struct {
    VEC *vec1;
    VEC *vec2;
} VEC_PAIR;

typedef VEC SM_VECTOR;

typedef struct {
	long  did;			// document id of the parent document
	char  text[8192];	// text of the sentence
	VEC   svec;			// vector representation of this sentence
	float sim;			// similarity with query
}
Sentence;

#endif /* VECTORH */
