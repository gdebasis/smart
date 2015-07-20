#ifndef EVALH
#define EVALH
/*        $Header: /home/smart/release/src/h/sm_eval.h,v 11.0 1992/07/21 18:18:54 chrisb Exp chrisb $*/

/* Defined constants that are collection/purpose dependent */

/* Default number of cutoffs for recall,precision, and rel_precis measures. */
/* CUTOFF_VALUES gives the number of retrieved docs that these */
/* evaluation mesures are applied at. */
#define DEFAULT_NUM_CUTOFF  6
#define DEFAULT_CUTOFF_VALUES  {5, 15, 30, 100, 200, 500}

/* Maximum fallout value, expressed in number of non-rel docs retrieved. */
/* (Make the approximation that number of non-rel docs in collection */
/* is equal to the number of number of docs in collection) */
#define DEFAULT_MAX_FALL_RET  142

/* Maximum multiple of R (number of rel docs for this query) to calculate */
/* R-based precision at */
#define MAX_RPREC 2.0


/* ----------------------------------------------- */
/* Defined constants that are collection/purpose independent.  If you
   change these, you probably need to change comments and documentation,
   and some variable names may not be appropriate any more! */
#define DEFAULT_NUM_RP_PTS  11
#define DEFAULT_NUM_FR_PTS  11
#define DEFAULT_NUM_PREC_PTS 11

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

    /* Measures after each document (all have num_cutoff values) */
    long  num_cutoff;
    long  *cutoff;
    float *recall_cut;              /* Recall after cutoff[i] docs */

    float *precis_cut;              /* precision after cutoff[i] docs. If
                                       less than cutoff[i] docs retrieved,
                                       then assume an additional 
                                       cutoff[i]-num_ret non-relevant docs
                                       are retrieved. */
    float *rel_precis_cut;/* Relative precision after cutoff[i] 
                                       docs.   */

    /* Measures after each rel doc */
    float av_recall_precis;         /* average(integral) of precision at
                                       all rel doc ranks */
    float int_av_recall_precis;     /* Same as above, but the precision values
                                       have been interpolated, so that prec(X)
                                       is actually MAX prec(Y) for all 
                                       Y >= X   */
    long  num_rp_pts;
    float *int_recall_precis;        /* interpolated precision at 
                                       default 0.1 increments of recall */
    float int_nrp_av_recall_precis;   /* interpolated average at NUM_RP_PTS 
                                       intermediate points (recall_level) */

    /* Measures after each non-rel doc */
    long num_fr_pts;
    long max_fall_ret;
    float *fall_recall;             /* max recall after each non-rel doc,
                                       at 11 points starting at 0.0 and
                                       ending at MAX_FALL_RET /num_docs */
    float av_fall_recall;           /* Average of fallout-recall, after each
                                       non-rel doc until fallout of 
                                       MAX_FALL_RET / num_docs achieved */

    /* Measures after R-related cutoffs.  R is the number of relevant
     docs for a particular query, but note that these cutoffs are after
     R docs, whether relevant or non-relevant, have been retrieved.
     R-related cutoffs are really only applicable to a situation where
     there are many relevant docs per query (or lots of queries). */
    float R_recall_precis;          /* Recall or precision after R docs
                                       (note they are equal at this point) */
    float av_R_precis;              /* Average (or integral) of precision at
                                       each doc until R docs have been 
                                       retrieved */
    long num_prec_pts;
    float *R_prec_cut;              /* Precision measured after multiples of
                                       R docs have been retrieved.  11 
                                       equal points, with max multiple
                                       having value MAX_RPREC */
    float int_R_recall_precis;      /* Interpolated precision after R docs
                                       Prec(X) = MAX(prec(Y)) for all Y>=X */
    float int_av_R_precis;          /* Interpolated */
    float *int_R_prec_cut;          /* Interpolated */

} EVAL;


/* A list of eval objects.  This could be either one eval per query, or
   it could be one eval summary per run */
typedef struct {
    long num_eval;
    EVAL *eval;
    char *description;              /* If non-NULL, a description of the
                                       interpretation of this eval list */
} EVAL_LIST;

typedef struct {
    long num_eval_list;
    EVAL_LIST *eval_list;
    char *description;
} EVAL_LIST_LIST;


/* The input to the base evaluation function.  We pass in the ranks of the
   relevant retrieved docs, plus the total number of relevant docs, plus
   the total number of retrieved docs. All measures use only these values. */
typedef struct {
    long qid;
    long num_rel_ret;
    long *rel_ret_rank;
    long num_rel;
    long num_ret;
} EVAL_INPUT;

void print_eval(), print_eval_list(), 
    print_short_eval(), print_short_eval_list(),
    print_prec_eval(), print_prec_eval_list(), print_prec_eval_list_list();
int init_tr_eval_list(), tr_eval_list(), close_tr_eval_list();
int init_rr_eval_list(), rr_eval_list(), close_rr_eval_list();
int init_q_eval(), q_eval(), close_q_eval();
int init_ell_el_avg(), ell_el_avg(), close_ell_el_avg();
int init_ell_el_dev(), ell_el_dev(), close_ell_el_dev();
int init_ell_el_comp_over(), ell_el_comp_over(), close_ell_el_comp_over();
int init_ell_el_comp_over_equal(), ell_el_comp_over_equal(), close_ell_el_comp_over_equal();

#endif /* EVALH */
