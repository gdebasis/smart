#ifndef TRH
#define TRH
/*        $Header: /home/smart/release/src/h/tr.h,v 11.2 1993/02/03 16:47:49 smart Exp $*/

typedef struct {
    long  qid;          /* query id */
    long  did;          /* document id */
    long  rank;         /* Rank of this document */
    char  rel;          /* whether doc judged relevant(1) or not(0) */
    char  action;       /* what action a user has taken with doc */
    char  iter;         /* Number of feedback runs for this query */
    char  tr_unused;    /* Field unused for now */
    float sim;          /* similarity of did to qid */
} NTR;

typedef struct {
    long  qid;          /* query id */
    long  did;          /* document id */
    unsigned char  rank;/* Rank of this document */
    char  rel;          /* whether doc judged relevant(1) or not(0) */
    char  action;       /* what action a user has taken with doc */
    char  iter;         /* Number of feedback runs for this query */
    float sim;          /* similarity of did to qid */
} TR;

#define MAX_NUM_TR 20

typedef struct {
    char file_name[PATH_LEN];   /* Path of top rank file */
    int  fd;                    /* file descriptor for file_name */
    TR   *beginning_tr;         /* Beginning of in-core top_rank relation */
    TR   *current_tr;           /* Current location in top-rank relation */
    TR   *end_tr;               /* Last valid entry in top-rank relation */
    long size_tr;               /* Number of bytes reserved for in-core */
				/* relation */

    long file_size;             /* Size in bytes of file version of relation*/
    REL_HEADER rh;              /* Info about relation - see rel_header.h */

    long last_seek;             /* What the last seek done on the file */
				/* returned. If 0, indicates positioned at */
				/* space immediately preceding current_tr. If*/
				/* 1, positioned at current_tr (will */
				/* over-write if next operation is a write) */
    char opened;                /* Whether this record contains valid info */
    long mode;                  /* see io.h */
} _STR_FILES;

#define DEFAULT_TR_SIZE 100
#endif /* TRH */
