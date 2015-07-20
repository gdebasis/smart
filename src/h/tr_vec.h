#ifndef TR_VECH
#define TR_VECH
/*        $Header: /home/smart/release/src/h/tr_vec.h,v 11.2 1993/02/03 16:47:51 smart Exp $*/

typedef struct {
    long  did;          /* document id */
    long  rank;         /* Rank of this document */
    char  action;       /* what action a user has taken with doc */
    char  rel;          /* whether doc judged relevant(1) or not(0) */
    char  iter;         /* Number of feedback runs for this query */
    char trtup_unused;  /* Presently unused field */
    float sim;          /* similarity of did to qid */
} TR_TUP;

typedef struct {
    long  qid;          /* query id */
    long  num_tr;       /* Number of tuples for tr_vec */
    TR_TUP *tr;         /* tuples.  Invariant: tr sorted increasing did */
} TR_VEC;

#endif /* TR_VECH */
