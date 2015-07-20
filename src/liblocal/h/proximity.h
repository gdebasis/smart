#ifndef PROXIMITYH
#define PROXIMITYH

typedef struct {
    long con;                   /* Actual concept number */
    long num_coc;	        /* no. of times this term co-occurs */
    long num_doc;	        /* no. of docs this term co-occurs */
    long num_terms;		/* no. of query terms this term co-occurs with */
    float wt;                   /* its weight */
} PROX_WT;

/* a single vector, sorted on: ctypes,cons */
typedef struct {
        long     con;             /* the main term */
        long     num_occ;	  /* no. of times this term occurs */
	long	 num_doc;	  /* no. of top-ranked docs. this term occurs */
        long     num_ctype;       /* number of cooccurrence ctypes */
        long     num_conwt;       /* no. of cooccurrence tuples */
        long     *ctype_len;      /* length of subvector for each ctype */
        PROX_WT *con_wtp;        /* pointer to cooccurrences */
} PROXIMITY;

typedef struct {
    PROXIMITY *prox;
    long num_prox;
} PROXIMITY_LIST;

#endif /* PROXIMITYH */
