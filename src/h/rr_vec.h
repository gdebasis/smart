#ifndef RR_VECH
#define RR_VECH
/*        $Header: /home/smart/release/src/h/rr_vec.h,v 11.2 1993/02/03 16:47:42 smart Exp $*/

typedef struct {
    long  did;          /* document id */
    long  rank;         /* Rank of this document */
    float sim;          /* similarity of did to qid */
} RR_TUP;

typedef struct {
    long  qid;          /* query id */
    long  num_rr;       /* Number of tuples for rr_vec */
    RR_TUP *rr;         /* tuples.  Invariant: rr sorted increasing did */
} RR_VEC;

#endif /* RR_VECH */
