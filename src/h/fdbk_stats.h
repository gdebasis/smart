#ifndef FDBK_STATSH
#define FDBK_STATSH
/*        $Header: /home/smart/release/src/h/fdbk_stats.h,v 11.0 1992/07/21 18:18:37 chrisb Exp $*/

#include "vector.h"
#include "tr_vec.h"


typedef struct {            /* Occurrence info about con,ctype */
    long  ctype;
    long  con;
    long  rel_ret;          /* number of relevant retrieved docs con is in */
    long  nrel_ret;         /* number of nonrelevant retrieved docs for con */
    long  nret;             /* number of nonretrieved docs for con */
    float rel_weight;       /* total weight of con due to relevant docs */
    float nrel_weight;      /* total weight of con due to nonrelevant docs */
} OCCINFO;

typedef struct {
    long id_num;            /* Query id */
    long num_ret;           /* Total number of docs retrieved for query */
    long num_rel;           /* Number of relevant docs retrieved */
    long num_occinfo;       /* number of cons for which info stored */
    OCCINFO *occinfo;       /* occurrence info for each con (sorted by
                               ctype and then con) */
} FDBK_STATS;

#endif /* FDBK_STATSH */
