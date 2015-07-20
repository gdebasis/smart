#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/form_query_lm.c,v 11.0 1992/07/21 18:20:41 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "vector.h"
#include "feedback.h"
#include "smart_error.h"
#include "spec.h"
#include "inst.h"
#include "trace.h"
#include "retrieve.h"

static long  num_ctypes ;
static float lambda ;
static int   isAugmentable;

static SPEC_PARAM spec_args[] = {
    {"feedback.lm.lambda", getspec_float, (char*)&lambda},
    {"feedback.num_ctypes", getspec_long, (char *) &num_ctypes},
    {"feedback.form_query_lm.augment_vec", getspec_bool, (char *) &isAugmentable},
    TRACE_PARAM ("feedback.form_query_lm.trace")
};

static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    VEC vec;
    /* ctype info */
    long *ctype_len;         /* lengths of each subvector in vec */
    long num_ctype;          /* Maximum number of sub-vectors space reserved for */
    /* conwt_info */
    CON_WT *con_wtp;         /* pointer to concepts,weights for vec */
    long num_conwt;          // Maximum number of CON_WTs reserved
}
STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_form_query_lm (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_form_query_lm");

    /* Reserve space for this new instantiation's static variables */
    NEW_INST (new_inst);
    if (new_inst == UNDEF)
        return (UNDEF);

    ip = &info[new_inst];
    
    ip->num_ctype = 0;
    ip->num_conwt = 0;
    ip->valid_info = 1;
	ip->con_wtp = NULL ;
	ip->ctype_len = NULL ;
    
    PRINT_TRACE (2, print_string, "Trace: leaving init_form_query_lm");
    return (new_inst);
}

/*  Stores the lambda values of language model.
	Store lambdas of ctype[i] in ctype[i + num_ctypes].
*/
static void addLambdas(STATIC_INFO* ip, FEEDBACK_INFO* fdbk_info, long this_query_num_ctype,
				long this_query_num_conwt)
{
	long    ctype, i, j ;
	CON_WT* con_wtp ;

	con_wtp = ip->con_wtp ;

	// Advance the pointer to the start of our writing area
	con_wtp += this_query_num_conwt ;
	j = 0 ;

	for (ctype = 0; ctype < this_query_num_ctype; ctype++) {
		ip->ctype_len[ctype + this_query_num_ctype] = ip->ctype_len[ctype] ; // modify the ctype_len values 

		for (i = 0; i < ip->ctype_len[ctype]; i++) {
			// store the lambdas
			con_wtp[j].con = fdbk_info->occ[j].con ; 
			con_wtp[j].wt = fdbk_info->occ[j].weight ;

			if (fdbk_info->occ[j].weight == UNDEF) {
				// The number of relevant documents for this query is 0.
				con_wtp[j].wt = lambda;
			}
			j++ ;
		}
	}
}

int
form_query_lm (fdbk_info, new_query, inst)
FEEDBACK_INFO *fdbk_info;
QUERY_VECTOR *new_query;
int inst;
{
    STATIC_INFO *ip;
    long i;
    CON_WT *conwt_ptr;
    VEC *vec;
    long this_query_num_ctype = 0 ;  // this vector's number of ctypes
    long this_query_num_conwt = 0 ;  // this vector's number of weights

    PRINT_TRACE (2, print_string, "Trace: entering form_query_lm");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "form_query_lm");
        return (UNDEF);
    }
    ip  = &info[inst];

	// Find this query vector's number of terms and number concept types
    for (i = 0; i < fdbk_info->num_occ; i++) {
        if (fdbk_info->occ[i].ctype >= this_query_num_ctype)
            this_query_num_ctype = fdbk_info->occ[i].ctype + 1;
        this_query_num_conwt++;
    }

	if (isAugmentable) {
		// This is the first call to feedback. Hence
		// we are working with unaugmented vectors.
		this_query_num_conwt = twice(this_query_num_conwt) ;
		this_query_num_ctype = twice(this_query_num_ctype) ;
	}
 
    /* Make sure enough space reserved for concept,weight pairs */
    if (this_query_num_conwt > ip->num_conwt) {
		if ( ip->con_wtp == NULL ) {
			// No buffer is yet allocated
			if ( NULL == (ip->con_wtp = (CON_WT*) malloc ( (unsigned) this_query_num_conwt * sizeof(CON_WT)) ) )
				return UNDEF ;
		}
		else {
			// Need to allocate a larger buffer
			if ( NULL == (ip->con_wtp = (CON_WT*) realloc (ip->con_wtp, (unsigned) this_query_num_conwt * sizeof(CON_WT)) ) )
				return UNDEF ;
		}

		ip->num_conwt = this_query_num_conwt ;  // update the maximum
    }

    if (this_query_num_ctype > ip->num_ctype) {
		if ( ip->ctype_len == NULL ) {
			// No buffer is yet allocated
	        if (NULL == (ip->ctype_len = (long *) malloc ((unsigned) this_query_num_ctype * sizeof (long))))
    	        return (UNDEF);
		}
		else {	
			// Need to allocate a larger buffer
	        if (NULL == (ip->ctype_len = (long *) realloc (ip->ctype_len, (unsigned) this_query_num_ctype * sizeof (long))))
    	        return (UNDEF);
		}

		ip->num_ctype = this_query_num_ctype ;	// update the maximum
    }

	if (isAugmentable) {
		this_query_num_conwt = half(this_query_num_conwt) ;
		this_query_num_ctype = half(this_query_num_ctype) ;
	}
 
	bzero(ip->ctype_len, this_query_num_ctype * sizeof(long)) ;

    conwt_ptr = ip->con_wtp;

    for (i = 0; i < fdbk_info->num_occ; i++) {

        conwt_ptr->con = fdbk_info->occ[i].con;
		if (!fdbk_info->occ[i].query) {
			// This is an expansion term. Set its weight to 1 in the new query vector
			conwt_ptr->wt = fdbk_info->occ[i].rel_ret > 0 ? 1.0 : 0.0;
		}
		else {
       		conwt_ptr->wt = fdbk_info->occ[i].orig_weight ;
		}

        ip->ctype_len[fdbk_info->occ[i].ctype]++;
        conwt_ptr++;
    }

	if (isAugmentable)
		addLambdas(ip, fdbk_info, this_query_num_ctype, this_query_num_conwt);
	else {
		conwt_ptr = ip->con_wtp + half(fdbk_info->num_occ); 
		/* Store the new values in place of the older ones. */
    	for (i = 0; i < half(fdbk_info->num_occ); i++, conwt_ptr++) {
        	conwt_ptr->con = fdbk_info->occ[i].con;
			conwt_ptr->wt = fdbk_info->occ[i].weight;
		}
	}

    vec = &ip->vec;
    vec->ctype_len = ip->ctype_len;
    vec->con_wtp = ip->con_wtp;

	if ( isAugmentable ) {
		this_query_num_conwt = twice(this_query_num_conwt) ;
		this_query_num_ctype = twice(this_query_num_ctype) ;
	}

    vec->num_ctype = this_query_num_ctype ; 
    vec->num_conwt = this_query_num_conwt ;
    vec->id_num = fdbk_info->orig_query->id_num;
    
    new_query->qid = vec->id_num.id;
    new_query->vector = (char *) vec;
    
    PRINT_TRACE (4, print_vector, vec);
    PRINT_TRACE (2, print_string, "Trace: leaving form_query_lm");
    return (1);
}

int
close_form_query_lm (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_form_query_lm");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_form_query_lm");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (ip->num_conwt > 0)
		FREE(ip->con_wtp) ; 
    if (ip->num_ctype > 0) 
        FREE(ip->ctype_len);

    ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_form_query_lm");
    return (1);
}
