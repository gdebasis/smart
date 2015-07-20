#ifndef SMEVALH
#define SMEVALH
/*        $Header: /home/smart/release/src/h/sm_eval.h,v 11.2 1993/02/03 16:47:45 smart Exp $*/

#define NUM_RP_PTS  11
#define THREE_PTS {2, 5, 8}    
#define NUM_CUTOFF  4
#define CUTOFF_VALUES  {5, 10, 15, 30}

typedef struct {
    long  qid;                      /* query id  (for overall average figures,
                                       this gives number of queries in run) */
    long num_rel;                   /* Number of relevant docs */
    long num_ret;                   /* Number of retrieved docs */
    long num_rel_ret;               /* Number of relevant retrieved docs */
    long num_trunc_ret;             /* MIN (Number of retrieved docs, poorest
                                       rank of any relevant doc - whether
                                       retrieved of not). */

    float recall_precis[NUM_RP_PTS];/* Recall precision at 0.1 increments */
    float av_recall_precis;         /* average at all points */
    float three_av_recall_precis;   /* average at 3 intermediate points */

    float exact_recall;             /* Recall after num_ret docs */
    float recall_cut[NUM_CUTOFF];   /* Recall after cutoff[i] docs */

    float exact_precis;             /* Precision after num_ret docs */
    float precis_cut[NUM_CUTOFF];   /* precision after cutoff[i] docs. If
                                       less than cutoff[i] docs retrieved,
                                       then assume an additional 
                                       cutoff[i]-num_ret non-relevant docs
                                       are retrieved. */
    float exact_trunc_precis;       /* Precision after num_trunc_ret docs */
    float trunc_cut[NUM_CUTOFF];    /* precision after cutoff[i] docs or
                                       rank of last rel doc, whichever is
                                       less. If not all rel docs retrieved and
                                       less than cutoff[i] docs retrieved,
                                       then assume an additional 
                                       cutoff[i]-num_ret non-relevant docs
                                       are retrieved. */
} SM_EVAL;

#define ONUM_RP_PTS  21

typedef struct {
    long  qid;                      /* query id  (for overall average figures,
                                       this gives number of queries in run) */
    long num_rel;                   /* Number of relevant docs */
    long num_ret;                   /* Number of retrieved docs */
    long num_rel_ret;               /* Number of relevant retrieved docs */
    long num_trunc_ret;             /* MIN (Number of retrieved docs, poorest
                                       rank of any relevant doc - whether
                                       retrieved of not). */

    float recall_precis[ONUM_RP_PTS];/* Recall precision at .05 increments */
    float av_recall_precis;         /* average at 3 intermediate points */

    float exact_recall;             /* Recall after num_ret docs */
    float recall_cut[NUM_CUTOFF];   /* Recall after cutoff[i] docs */

    float exact_precis;             /* Precision after num_ret docs */
    float precis_cut[NUM_CUTOFF];   /* precision after cutoff[i] docs. If
                                       less than cutoff[i] docs retrieved,
                                       then assume an additional 
                                       cutoff[i]-num_ret non-relevant docs
                                       are retrieved. */
    float exact_trunc_precis;       /* Precision after num_trunc_ret docs */
    float trunc_cut[NUM_CUTOFF];    /* precision after cutoff[i] docs or
                                       rank of last rel doc, whichever is
                                       less. If not all rel docs retrieved and
                                       less than cutoff[i] docs retrieved,
                                       then assume an additional 
                                       cutoff[i]-num_ret non-relevant docs
                                       are retrieved. */
} OLDSM_EVAL;

#endif /* SMEVALH */





