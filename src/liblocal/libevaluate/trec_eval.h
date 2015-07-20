#ifndef TRECEVALH
#define TRECEVALH
/*        $Header: /home/smart/release/src/h/sm_eval.h,v 11.0 1992/07/21 18:18:54 chrisb Exp chrisb $*/

/* Defined constants that are collection/purpose dependent */

/* Number of cutoffs for recall,precision, and rel_precis measures. */
/* CUTOFF_VALUES gives the number of retrieved docs that these */
/* evaluation mesures are applied at. */
#define NUM_CUTOFF  6
#define CUTOFF_VALUES  {5, 15, 30, 100, 200, 500}

/* Maximum fallout value, expressed in number of non-rel docs retrieved. */
/* (Make the approximation that number of non-rel docs in collection */
/* is equal to the number of number of docs in collection) */
#define MAX_FALL_RET  142

/* Maximum multiple of R (number of rel docs for this query) to calculate */
/* R-based precision at */
#define MAX_RPREC 2.0


/* ----------------------------------------------- */
/* Defined constants that are collection/purpose independent.  If you
   change these, you probably need to change comments and documentation,
   and some variable names may not be appropriate any more! */
#define NUM_RP_PTS  11
#define THREE_PTS {2, 5, 8}    
#define NUM_FR_PTS  11
#define NUM_PREC_PTS 11

typedef struct {
    long  qid;                      /* query id  (for overall average figures,
                                       this gives number of queries in run) */
    /* Summary Numbers over all queries */
    long num_rel;                   /* Number of relevant docs */
    long num_ret;                   /* Number of retrieved docs */
    long num_rel_ret;               /* Number of relevant retrieved docs */

    /* Measures after num_ret docs */
    float exact_recall;             /* Recall after num_ret docs */
    float exact_precis;             /* Precision after num_ret docs */
    float exact_rel_precis;         /* Relative Precision (or recall) */
                                    /* Defined to be precision / max possible
                                       precision */

    /* Measures after each document */
    float recall_cut[NUM_CUTOFF];   /* Recall after cutoff[i] docs */

    float precis_cut[NUM_CUTOFF];   /* precision after cutoff[i] docs. If
                                       less than cutoff[i] docs retrieved,
                                       then assume an additional 
                                       cutoff[i]-num_ret non-relevant docs
                                       are retrieved. */
    float rel_precis_cut[NUM_CUTOFF];/* Relative precision after cutoff[i] 
                                       docs.   */

    /* Measures after each rel doc */
    float av_recall_precis;         /* average(integral) of precision at
                                       all rel doc ranks */
    float int_av_recall_precis;     /* Same as above, but the precision values
                                       have been interpolated, so that prec(X)
                                       is actually MAX prec(Y) for all 
                                       Y >= X   */
    float int_recall_precis[NUM_RP_PTS];/* interpolated precision at 
                                       0.1 increments of recall */
    float int_av3_recall_precis;    /* interpolated average at 3 intermediate 
                                       points */
    float int_av11_recall_precis;   /* interpolated average at NUM_RP_PTS 
                                       intermediate points (recall_level) */

    /* Measures after each non-rel doc */
    float fall_recall[NUM_FR_PTS];  /* max recall after each non-rel doc,
                                       at 11 points starting at 0.0 and
                                       ending at MAX_FALL_RET /num_docs */
    float av_fall_recall;           /* Average of fallout-recall, after each
                                       non-rel doc until fallout of 
                                       MAX_FALL_RET / num_docs achieved */

    /* Measures after R-related cutoffs.  R is the number of relevant
     docs for a particular query, but note that these cutoffs are after
     R docs, whether relevant or non-relevant, have been retrieved.
     R-related cutoffs are really only applicable to a situtation where
     there are many relevant docs per query (or lots of queries). */
    float R_recall_precis;          /* Recall or precision after R docs
                                       (note they are equal at this point) */
    float av_R_precis;              /* Average (or integral) of precision at
                                       each doc until R docs have been 
                                       retrieved */
    float R_prec_cut[NUM_PREC_PTS]; /* Precision measured after multiples of
                                       R docs have been retrieved.  11 
                                       equal points, with max multiple
                                       having value MAX_RPREC */
    float int_R_recall_precis;      /* Interpolated precision after R docs
                                       Prec(X) = MAX(prec(Y)) for all Y>=X */
    float int_av_R_precis;          /* Interpolated */
    float int_R_prec_cut[NUM_PREC_PTS]; /* Interpolated */

} TREC_EVAL;

#endif /* TRECEVALH */





