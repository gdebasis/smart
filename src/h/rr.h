#ifndef RRH
#define RRH
/*        $Header: /home/smart/release/src/h/rr.h,v 11.2 1993/02/03 16:47:34 smart Exp $*/

typedef struct {
    long  qid;          /* query id */
    long  did;          /* document id */
    long  rank;         /* rank of document */
    float sim;          /* similarity of did to qid */
} RR;


#define MAX_NUM_RR 20

typedef struct {
    char file_name[PATH_LEN];   /* Path of rel rank file */
    int  fd;                    /* file descriptor for file_name */
    RR   *beginning_rr;         /* Beginning of in-core rel_rank relation */
    RR   *current_rr;           /* Current location in rel-rank relation */
    RR   *end_rr;               /* Last valid entry in rel-rank relation */
    long size_rr;               /* Number of bytes reserved for in-core */
				/* relation */

    long file_size;             /* Size in bytes of file version of relation*/
    REL_HEADER rh;              /* Info about relation - see rel_header.h */

    long last_seek;             /* What the last seek done on the file */
				/* returned. If 0, indicates positioned at */
				/* space immediately preceding current_rr. */
				/* If 1, positioned at current_rr (will */
				/* over-write if next operation is a write) */
    char opened;                /* Whether this record contains valid info */
    long mode;                  /* see io.h */
} _SRR_FILES;

#define DEFAULT_RR_SIZE 100
#endif /* RRH */
