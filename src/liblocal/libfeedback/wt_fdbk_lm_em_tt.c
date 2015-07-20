#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/wt_fdbk_lm_em.c,v 11.2 1993/02/03 16:49:58 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "spec.h"
#include "trace.h"
#include "feedback.h"
#include "dir_array.h"
#include "collstat.h"
#include "inv.h"
#include "smart_error.h"

/*
 * 	compute new sets of lambda values based on the values at the previous iteration
 *	according to the formula
 *  lambda(i, p+1) = summation (j=1..r) [ 
		lambda(i, p) * P(ti|dj) / ((1 - lambda(i, p)*P(ti) + lambda(i,p)*P(ti|dj)))]
 *  WARNING: Assumes the existense of an nnn inv file for obtaining the term frequencies
 *	of each query term.
 */

static long num_ctypes ;
static int doc_fd;
static int *inv_fd;
static char* doc_file ;
static long doc_mode;
static char* inv_file ;
static long inv_file_mode;
static int* totalDocFreq;  // total document frequencies for each ctype
static char* collstat_file ;
static long collstat_mode ;
static float gLambda ;

static SPEC_PARAM spec_args[] = {
    {"feedback.num_ctypes",        getspec_long, (char *) &num_ctypes},
    {"feedback.lm.lambda", getspec_float, (char*)&gLambda},
    {"feedback.wt.lm.nnn_doc_file",     getspec_dbfile,    (char *) &doc_file},
    {"feedback.wt.lm.nnn_doc_file.rmode", getspec_filemode,(char *) &doc_mode},
    TRACE_PARAM ("feedback.weight.lm.trace")
};
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

// nnn_inv file for each ctype
static char *prefix;
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    {&prefix, "feedback.wt.lm.nnn_inv_file",  getspec_dbfile, (char *) &inv_file},
    {&prefix, "feedback.wt.lm.nnn_inv_file.rmode", getspec_filemode, (char *) &inv_file_mode},
    {&prefix, "feedback.wt.lm.collstat_file",  getspec_dbfile, (char *) &collstat_file},
    {&prefix, "feedback.wt.lm.collstat_file.rmode", getspec_filemode, (char *) &collstat_mode},
};
static int num_prefix_spec_args =
    sizeof (prefix_spec_args) / sizeof (prefix_spec_args[0]);

static struct coll_info {
	long *freq ;
	long num_freq ;
} *collstats_info ; // array of collection stat information, one for each ctype

int
init_wt_fdbk_lm_em (spec, unused)
SPEC *spec;
char *unused;
{
    char param_prefix[64];
    long i, j ;
    DIR_ARRAY dir_array;
	int  collstats_fd ;

    PRINT_TRACE (2, print_string, "Trace: entering init_wt_fdbk_lm_em");

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    if (! VALID_FILE (doc_file))
		return UNDEF ;

    /* Open direct file */
   	if (UNDEF == (doc_fd = open_vector (doc_file, doc_mode)))
       	return (UNDEF);

    /* Reserve space for direct and inverted file descriptors */
    if (NULL == (inv_fd = (int *)
                 malloc ((unsigned) num_ctypes * sizeof (int)))) {
        return (UNDEF);
    }

	// Reserve space for collection statistics
    if (NULL == (collstats_info = (struct coll_info*)
                 malloc ((unsigned) num_ctypes * sizeof (struct coll_info)))) {
        return (UNDEF);
    }

	totalDocFreq = (int*) malloc (sizeof(int) * num_ctypes) ;
	if (totalDocFreq == NULL)
		return UNDEF ;

    prefix = param_prefix;
	// For each ctype
    for (i = 0; i < num_ctypes; i++) {
        (void) sprintf (param_prefix, "feedback.ctype.%ld.", i);
        if (UNDEF == lookup_spec_prefix (spec,
                                         &prefix_spec_args[0],
                                         num_prefix_spec_args))
            return (UNDEF);

        /* Open inverted file. Note there may not be a valid inverted file for all ctypes;
           may be OK as long as careful about feedback for that ctype */
        if (!VALID_FILE (inv_file))
            inv_fd[i] = UNDEF;
        else {
            if (UNDEF == (inv_fd[i] = open_inv (inv_file, inv_file_mode)))
                return (UNDEF);
        }

		if (! VALID_FILE (collstat_file)) {
            collstats_info[i].freq = NULL ;
            collstats_info[i].num_freq = 0;
        }
        else {
            if (UNDEF == (collstats_fd = open_dir_array (collstat_file, collstat_mode)))
                return (UNDEF);

            dir_array.id_num = COLLSTAT_COLLFREQ; // Get the collection frequency list from the file
            if (1 != seek_dir_array (collstats_fd, &dir_array) ||
                1 != read_dir_array (collstats_fd, &dir_array)) {
                collstats_info[i].freq = NULL;
                collstats_info[i].num_freq = 0;
            }
            else {
                // Read from file successful. Allocate 'freq' array and dump the
                // contents of the file in this list
                if (NULL == (collstats_info[i].freq = (long *)
                             malloc ((unsigned) dir_array.num_list)))
                    return (UNDEF);
                (void) bcopy (dir_array.list,
                          (char *) collstats_info[i].freq,
                          dir_array.num_list);
                collstats_info[i].num_freq = dir_array.num_list / sizeof (long);
            }

            if (UNDEF == close_dir_array (collstats_fd))
                return (UNDEF);
        }

		totalDocFreq[i] = 0 ;
    	for (j = 0; j < collstats_info[i].num_freq; j++) {
        	totalDocFreq[i] += collstats_info[i].freq[j] ;
    	}
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_wt_fdbk_lm_em");
    return (1);
}

// Get the lambda values from the augmented query vector.
static float getLambda(VEC* vec, long i)
{
	if ( num_ctypes == vec->num_ctype ) {
		// This is the first iteration and we have got an unaugmented
		// query vector.
		return gLambda ;
	}
 
	return vec->con_wtp[(vec->num_conwt>>1) + i].wt ;
}

// Computes the document length 
static int getDocLength(VEC* doc, long index, long ctype)
{
	int i, length = 0 ;

    if (UNDEF == seek_vector (doc_fd, doc) ||
	    UNDEF == read_vector (doc_fd, doc)) {
		return UNDEF ;
	}

	CON_WT* start = doc->con_wtp ;

	// Jump to the appropriate part of the vector
	for (i = 0; i < ctype; i++) {
		start += doc->ctype_len[i] ;
	}

	// Read ctype_len[ctype] weights and add them up.
	for (i = 0; i < doc->ctype_len[ctype]; i++) {
		length += start[i].wt ;
	}

	return length ;
}

int
wt_fdbk_lm_em (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    long    i, k ;
	VEC*    qvec = fdbk_info->orig_query ;
	INV     inv ;
	long    tr_index ;
	float   lambda ;
	VEC     docInfo ;
	int     docLength ;
	float   probTermImportance, probTermNonImportance ;
	static char msg[1024];

    PRINT_TRACE (2, print_string, "Trace: entering wt_fdbk_lm_em");

	// Iterate for each query term and the set of relevant documents 
    for (i = 0; i < fdbk_info->num_occ; i++) {

		if (fdbk_info->occ[i].ctype >= num_ctypes)
			break;	// this is the augmented part

		if (!fdbk_info->occ[i].query) {
			/* If the term is an expanded term set its weight to feedback.lambda */
			fdbk_info->occ[i].weight = gLambda;
			continue;
		} 

		// set the key of the inv structure to seek
		inv.id_num = fdbk_info->occ[i].con ;

        if (1 != seek_inv (inv_fd[fdbk_info->occ[i].ctype], &inv) ||
            1 != read_inv (inv_fd[fdbk_info->occ[i].ctype], &inv) ) {
            PRINT_TRACE (1, print_string, "occ_info: concept not found in inv_file:");
            PRINT_TRACE (1, print_long, &fdbk_info->occ[i].con);
            continue;
        }

        tr_index = 0;

        for (k = 0; k < inv.num_listwt; k++) {

            /* Discover whether doc is in top_docs */
            /* Assume both inverted list and tr are sorted */
            while (tr_index < fdbk_info->tr->num_tr &&
                 fdbk_info->tr->tr[tr_index].did < inv.listwt[k].list)
                tr_index++;

            if (tr_index < fdbk_info->tr->num_tr &&
                fdbk_info->tr->tr[tr_index].did == inv.listwt[k].list) {

                // If document is relevant 
                if ( fdbk_info->tr->tr[tr_index].rel ) {
					// inv.listwt[k].wt gives the term freq of this query term in the k-th document
					lambda = getLambda(qvec, i) ;
					docLength = getDocLength(&docInfo, inv.listwt[k].list, fdbk_info->occ[i].ctype);
					if (docLength == UNDEF)
						break;

					probTermImportance = inv.listwt[k].wt / (float)docLength ;
					probTermNonImportance = collstats_info[fdbk_info->occ[i].ctype].freq[fdbk_info->occ[i].con] /
											(float)totalDocFreq[fdbk_info->occ[i].ctype] ; 

					snprintf(msg, sizeof(msg), "added contribution for %ldth query term", i);
            		PRINT_TRACE (4, print_string, msg);
					fdbk_info->occ[i].weight +=
							lambda*probTermImportance / ((1 - lambda)*probTermNonImportance + lambda*probTermImportance);
                }
            }
        }

		if (fdbk_info->occ[i].rel_ret == 0)
			fdbk_info->occ[i].weight = gLambda ;
		else
	        fdbk_info->occ[i].weight /= (float)fdbk_info->occ[i].rel_ret;

		snprintf(msg, sizeof(msg), "%ldth query term fetched %ld rel docs", i, fdbk_info->occ[i].rel_ret);
        PRINT_TRACE (4, print_string, msg);
	}
	
    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving wt_fdbk_lm_em");

    return (1);
}

int
close_wt_fdbk_lm_em (inst)
int inst;
{
	long ctype ;

    PRINT_TRACE (2, print_string, "Trace: entering close_wt_fdbk_lm_em");

	if ( UNDEF == close_vector(doc_fd) )
		return UNDEF ;

    for (ctype = 0; ctype < num_ctypes; ctype++) {
        if (UNDEF == close_inv (inv_fd[ctype]))
            return (UNDEF);
        inv_fd[ctype] = UNDEF;
    }

	Free(collstats_info) ;
	Free(inv_fd) ;
	Free(totalDocFreq) ;

    PRINT_TRACE (2, print_string, "Trace: leaving close_wt_fdbk_lm_em");
    return (0);
}
