#ifndef PNORM_VECTORH
#define PNORM_VECTORH
typedef struct {
  int op;                              /* type of operator (AND, OR, NOT) */
  float p;                             /* p value of operator (0.0 for NOT) */
  float wt;                            /* clause wt of clause defined by op */
} OP_P_WT;


typedef struct {
  long info;                          /* pointer into OP_P_WT or CON_WT */
  short child;                        /* pointer to leftmost child */
  short sibling;                      /* ptr to next sibling to the right */
} TREE_NODE;


typedef struct {
  long id_num;                        /* query id */
  short num_ctype;                    /* number of ctypes in doc collection */
  short num_conwt;                    /* number concept-wt pairs in query */
  short num_nodes;                    /* number tree nodes in query */
  short num_op_p_wt;                  /* number of operator nodes in tree */
  short *ctype_len;                   /* number of tuples of each ctype */
  CON_WT *con_wtp;                    /* ptr to concept-weight tuples */
  TREE_NODE *tree;                    /* tree defining structure of query */
  OP_P_WT *op_p_wtp;                  /* ptr to op_p_wt tuples */
} PNORM_VEC;

#endif /* PNORM_VECTORH */
#ifndef PART_VECTORH
#define PART_VECTORH
/*  $Header: /home/smart/release/src/h/part_vector.h,v 11.2 1993/02/03 16:47:58 smart Exp $ */

#include "retrieve.h"

typedef struct {
    long con;                   /* Actual concept number */
    float wt;                   /* and its weight */
    long partnum;		/* part number this concept appears in */
} PART_CON_WT;

/* a single vector, sorted on: ctypes,cons */
typedef struct {
        long   id_num;          /* unique number for this vector within  */
                                /* collection */
	long   max_partnum;	/* largest partnum value that occurs */
	long   partnums_used;	/* number of distinct partnums listed */
        long   num_ctype;       /* number of ctypes for this vector */
        long   num_part_conwt;  /* no. of tuples in the vector */
        long   *ctype_len;      /* length of subvector for each ctype */
        PART_CON_WT *part_con_wtp;  /* pointer to concepts, wts for vector*/

} PART_VEC;

/* Multiple vectors */
typedef struct {
    PART_VEC *pvec;
    long num_pvec;
    long id_num;
} PART_VEC_LIST;

typedef struct {
    PART_VEC_LIST *pvec_list1;
    PART_VEC_LIST *pvec_list2;
} PART_VEC_LIST_PAIR;

typedef struct {
    PART_VEC *pvec1;
    PART_VEC *pvec2;
} PART_VEC_PAIR;

typedef struct {
    long num_results;
    RESULT_TUPLE *results;
    RESULT_TUPLE **res_ptr;/* elt i*maxrow+j points to which results[] used */
} PART_VEC_RESULTS;

typedef PART_VEC SM_PART_VECTOR;

int create_partvec(), open_partvec(), seek_partvec(), read_partvec(),
        write_partvec(), close_partvec(), destroy_partvec(), rename_partvec();
void print_partvec();

#endif /* PART_PARTVECH */
#ifndef OLDINVH
#define OLDINVH
/*  $Header: /home/smart/release/src/h/oldinv.h,v 11.2 1993/02/03 16:47:36 smart Exp $ */

/* a single node visible to the user */
typedef struct {
        long  key_num;          /* key to access this inverted list with*/
        unsigned short num_list; /* Number of elements in this list */
        long  *list;            /* pointer to list elements  */
        float *list_weights;    /* Pointer to weights for corresponding */
                                /* elements */
} OLDINV;

#endif /* OLDINVH */
#ifndef DISPLAYH
#define DISPLAYH
/*        $Header: /home/smart/release/src/h/display.h,v 11.2 1993/02/03 16:47:26 smart Exp $*/

#define TITLE_LEN 69

typedef struct  {
    long  id_num;                   /* Id of doc */
    short num_sections;             /* Number of sections in this did */
    long  *begin;                   /* beginning of sections 0..numsections-1*/
                                    /* (bytes from start of file - section 0 */
                                    /* is the start of the document */ 
    long  *end;                     /* end of sections 0..numsections -1 */
    char  *file_name;               /* File to find text of doc in */
    char  *title;                   /* Title of doc (up to TITLE_LEN chars */
} DISPLAY;
#endif /* DISPLAYH */
