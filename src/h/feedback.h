#ifndef FEEDBACKH
#define FEEDBACKH
/*        $Header: /home/smart/release/src/h/feedback.h,v 11.2 1993/02/03 16:47:28 smart Exp $*/

#include "vector.h"
#include "tr_vec.h"

typedef struct {            /* Occurrence info about con,ctype */
    long  con;
    long  ctype;
    char  query;            /* Boolean. whether in original query */
    long  rel_ret;          /* number of relevant retrieved docs con is in */
    long  nrel_ret;         /* number of nonrelevant retrieved docs for con */
    long  nret;             /* number of nonretrieved docs for con */
    float weight;           /* final weight for con */
    float orig_weight;      /* weight of con in original query */
    float rel_weight;       /* weight of con due to relevant docs */
    float nrel_weight;      /* weight of con due to nonrelevant docs */
} OCC;

typedef struct {
    VEC *orig_query;        /* original vector query */
    TR_VEC *tr;             /* relevance info for original query */
    OCC *occ;               /* occurrence info for each con */
    long num_occ;           /* number of cons under consideration */
    long max_occ;           /* number of cons space reserved for */
    long num_rel;           /* Number of relevant docs seen */
} FEEDBACK_INFO;

#endif /* FEEDBACKH */
