#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libretrieve/sim_cty_inv_lm_idf_aug.c,v 11.2 1993/02/03 16:52:55 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Perform inverted retrieval algorithm for a single ctype query 
 *1 local.retrieve.ctype_coll.ctype_inv
 *2 sim_ctype_inv (qvec, results, inst)
 *3   VEC *qvec;
 *3   RESULT *results;
 *3   int inst;
 *4 init_sim_ctype_inv (spec, param_prefix)
 *5   "retrieve.rank_tr"
 *5   "retrieve.ctype_coll.trace"
 *5   "ctype.*.inv_file"
 *5   "ctype.*.inv_file.rmode"
 *5   "ctype.*.sim_ctype_weight"
 *4 close_sim_ctype_inv (inst)

 *7 This is the work-horse retrieval procedure.  It takes a query
 *7 vector qvec, most often a single ctype vector, and runs it against the
 *7 inverted file specified by the parameter whose name is the
 *7 concatenation of param_prefix and "inv_file".  Normally this parameter
 *7 name will be "ctype.N.inv_file", where N is the ctype for this query.
 *7 The results are accumulated in the data structures given by results,
 *7 which may contain partial results from other ctypes of this query vector.
 *7 The basic algorithm is
 *7    for each concept C in qvec
 *7        for each document D in inverted list for concept C
 *7            full_results (D) = full_results (D) + 
 *7               sim_ctype_weight * (weight of C in qvec) * (weight of C in D)
 *7 The array results->full_results contains the partial similarity 
 *7 of document D to the full query so far.
 *7 The array results->top_results contains the top num_wanted similarities 
 *7 so far computed.  These are the docs that will be returned to the user.
 *7 This array is maintained by procedure rank_tr.
 *8 sim_ctype_inv* is most often called by retrieve.retrieve.inverted()
 *9 This procedure chooses the intial lambdas by idfs of query terms.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "retrieve.h"
#include "rel_header.h"
#include "vector.h"
#include "inv.h"
#include "tr_vec.h"
#include "inst.h"
#include "collstat.h"
#include "dir_array.h"

/*  Perform inner product similarity function using inverted file to access */
/*  document weights.  Query weights given by query vector.  Weight for */
/*  type of query term (ctype weight) is given by ret_spec file. */

/* Basic algorithm: malloc enough space to keep track of all similarities */
/*  of documents with direct access by did. */
/*  Top num_wanted similarites kept track of throughout. */

static PROC_TAB *rank_tr;

static char  *prefix;
static char  *doc_file;
static long  doc_mode;
static char  *inv_file;
static long  inv_mode;
static float ctype_weight;
static float maxIdf = 0 ;

// Collection information
struct coll_info {
    long *freq;     // frequency of the ith term in the collection
    long num_freq;  // total number of terms in collection
} ;

static struct coll_info collstats_info, numdocs_info ;   // array of collection stat frequencies
static int collstats_fd;                   // an array of fds one for each concept type
static char *collstat_file;
static char *collstat_mode;
static float lambda ;

static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix,  "doc_file",     getspec_dbfile,    (char *) &doc_file},
    {&prefix,  "doc_file.rmode", getspec_filemode,(char *) &doc_mode},
    {&prefix,  "inv_file",     getspec_dbfile,    (char *) &inv_file},
    {&prefix,  "inv_file.rmode", getspec_filemode,(char *) &inv_mode},
    {&prefix,  "sim_ctype_weight", getspec_float,(char *) &ctype_weight},
    {&prefix, "collstat_file",   getspec_dbfile,    (char *) &collstat_file},
    {&prefix, "collstat.rwmode", getspec_filemode,  (char *) &collstat_mode},
    };
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

static SPEC_PARAM spec_args[] = {
    {"retrieve.rank_tr",   getspec_func, (char *) &rank_tr},
    {"retrieve.lm.lambda", getspec_float, (char *) &lambda},  // this parameter is the initial lambda of LM
    TRACE_PARAM ("retrieve.sim_ctype_inv_lm_idf_aug.trace")
};
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    /*inverted file info */
    char file_name[PATH_LEN];
    int doc_fd;
    int inv_fd;
    float ctype_weight;                /* Weight for this particular ctype */
    int rank_tr_inst;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int comp_q_wt();

int
init_sim_ctype_inv_lm_idf_aug (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    DIR_ARRAY dir_array;
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    prefix = param_prefix;
    if (UNDEF == lookup_spec_prefix (spec, &pspec_args[0], num_pspec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_sim_ctype_inv_lm_idf_aug");

    if (! VALID_FILE (collstat_file)) {
        collstats_info.freq = NULL ;
        collstats_info.num_freq = 0;
    }
    else {
        if (UNDEF == (collstats_fd = open_dir_array (collstat_file, collstat_mode)))
            return (UNDEF);

        // Get the frequency mode to use
        dir_array.id_num =  COLLSTAT_COLLFREQ ;

        // Get the collection frequency list from the file
        if (1 != seek_dir_array (collstats_fd, &dir_array) ||
            1 != read_dir_array (collstats_fd, &dir_array)) {
            collstats_info.freq = NULL;
            collstats_info.num_freq = 0;
        }
        else {
            // Read from file successful. Allocate 'freq' array and dump the
            // contents of the file in this list
            if (NULL == (collstats_info.freq = (long *)
                         malloc ((unsigned) dir_array.num_list)))
                return (UNDEF);
            (void) bcopy (dir_array.list,
                      (char *) collstats_info.freq,
                      dir_array.num_list);
            collstats_info.num_freq = dir_array.num_list / sizeof (long);
        }

		// Read the number of documents
        dir_array.id_num = COLLSTAT_NUMDOC ;

        if (1 != seek_dir_array (collstats_fd, &dir_array) ||
            1 != read_dir_array (collstats_fd, &dir_array)) {
            numdocs_info.freq = NULL;
            numdocs_info.num_freq = 0;
        }
        else {
            // Read from file successful. Allocate 'freq' array and dump the
            // contents of the file in this list
            if (NULL == (numdocs_info.freq = (long *)
                         malloc ((unsigned) dir_array.num_list)))
                return (UNDEF);
            (void) bcopy (dir_array.list,
                      (char *) numdocs_info.freq,
                      dir_array.num_list);
            numdocs_info.num_freq = dir_array.num_list / sizeof (long);
        }

        if (UNDEF == close_dir_array (collstats_fd))
            return (UNDEF);
    }

    if (! VALID_FILE (inv_file)) {
        set_error (SM_ILLPA_ERR,"Non-existant inverted file", "sim_ctype_inv_lm_idf_aug");
        return (UNDEF);
    }

    PRINT_TRACE (4, print_string, inv_file);

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    /* Call all initialization procedures */
    if (UNDEF == (ip->rank_tr_inst = rank_tr->init_proc (spec, NULL))) {
        return (UNDEF);
    }

    /* Open direct file */
    if (UNDEF == (ip->doc_fd = open_vector (doc_file, doc_mode)))
        return (UNDEF);

    /* Open inverted file */
    if (UNDEF == (ip->inv_fd = open_inv(inv_file, inv_mode)))
        return (UNDEF);
    (void) strncpy (ip->file_name, inv_file, PATH_LEN);

    ip->ctype_weight = ctype_weight; 
    ip->valid_info++;

    PRINT_TRACE (2, print_string, "Trace: leaving init_sim_ctype_inv_lm_idf_aug");
    return (new_inst);
}

static float getLambdaFromIdf(VEC* vec, long i)
{
	return collstats_info.freq[i] == 0 ? 0.5 : log(*(numdocs_info.freq) / collstats_info.freq[i]) ;
}

// Get the lambda values from the augmented query vector.
static float getLambda(VEC* vec, long i)
{
    if ( vec->ctype_len[1] == 0 ) {
        // This is the first iteration and we have got an unaugmented
        // query vector.
        return getLambdaFromIdf(vec, i) ;
    }

    return vec->con_wtp[vec->ctype_len[0] + i].wt ;
}

static CON_WT* getAuxCon(CON_WT* start, long len, CON_WT* key)
{
	CON_WT* end = start + len ;
	CON_WT* con_wtp ;
 
	for (con_wtp = start; con_wtp < end; con_wtp++) {
		if ( con_wtp->con == key->con )
			return con_wtp ;
	}
	return NULL ;
}

static void sortAugmentedVector(VEC* vec)
{
	long i ;
	CON_WT* conwt_ptr, tmp ;

	// Sort the first part of the augmented vector ordered by term weights.
    qsort ((char *) vec->con_wtp,
           (int) half(vec->num_conwt), 
           sizeof (CON_WT),
           comp_q_wt);

	// Rearrange the augmented part
	for (i = 0; i < half(vec->num_conwt); i++) {
		conwt_ptr = getAuxCon(	vec->con_wtp + half(vec->num_conwt) + i,
								half(vec->num_conwt) - i, &vec->con_wtp[i] ) ;
		if (conwt_ptr != NULL) {
			// Swap the con_wts
			tmp = vec->con_wtp[half(vec->num_conwt) + i] ;
			vec->con_wtp[half(vec->num_conwt) + i] = *conwt_ptr ;
			*conwt_ptr = tmp ;
		}
	}
}

static long getDocLength(VEC* vec)
{
    int  i ;
	long length = 0 ;
    for (i = 0; i < vec->ctype_len[0]; i++) {
        length += vec->con_wtp[i].wt ;
    }
    return length ;
}

int
sim_ctype_inv_lm_idf_aug (qvec, results, inst)
VEC *qvec;
RESULT *results;
int inst;
{
    STATIC_INFO     *ip;
    CON_WT          *query_con;
    CON_WT          *last_query_con;
    register float  query_wt;
    INV             inv;
    register LISTWT *listwt_ptr, *end_listwt_ptr;
    TOP_RESULT      new_top;
    VEC new_qvec,   docInfo ;
	float           thisQueryTermLambda ;
	long            conwt_offset ;
	long            docLength ;
	static char     msg[256] ;
	float           idf ;

    PRINT_TRACE (2, print_string, "Trace: entering sim_ctype_inv_lm_idf_aug");
    PRINT_TRACE (6, print_vector, qvec);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "sim_ctype_inv_lm_idf_aug");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Copy query, and sort so that all negative query weights are first
       (so they don't ever enter top docs) and then by decreasing
       query weight (so fewer docs enter top docs) */
    if (UNDEF == copy_vec (&new_qvec, qvec))
        return (UNDEF);

	sortAugmentedVector(&new_qvec) ;

    /* Perform inverted file retrieval, sequentially going through query.
       Ignore ctype (qvec probably is only one ctype, either naturally or
       because this invoked through sim_vec_inv_lm_idf_aug)
	*/
    last_query_con = &new_qvec.con_wtp[new_qvec.num_conwt];
	maxIdf = 0;

	// Find the max-idf only if this is the first iteration.
    if ( qvec->ctype_len[1] == 0 ) {
		// In this loop calculate the maximum value of idf so as to normalize
		// the lambda values
    	for ( query_con = new_qvec.con_wtp, conwt_offset = 0; 
        	  query_con < last_query_con;
	          query_con++, conwt_offset++) {
    	    if ( 0.0 == (query_wt = ip->ctype_weight * query_con->wt) ) {
        	    continue;
	        }
			idf = collstats_info.freq[conwt_offset] == 0 ? 0.01 : log(*(numdocs_info.freq) / collstats_info.freq[conwt_offset]) ;
			maxIdf += idf;
		}
		snprintf(msg, sizeof(msg), "Trace: maxIdf = %f", maxIdf) ;
    	PRINT_TRACE (2, print_string, msg) ; 
	}

    for ( query_con = new_qvec.con_wtp, conwt_offset = 0; 
          query_con < last_query_con;
          query_con++, conwt_offset++) {

        if ( 0.0 == (query_wt = ip->ctype_weight * query_con->wt) ) {
            continue;
        }

		thisQueryTermLambda = getLambda(&new_qvec, conwt_offset) / maxIdf ;
		if (thisQueryTermLambda >= 1 )
			thisQueryTermLambda = 0.99 ;
		if (thisQueryTermLambda <= 0 )
			thisQueryTermLambda = 0.01 ;

		thisQueryTermLambda = pow(thisQueryTermLambda, 1/(float)3); 
		snprintf(msg, sizeof(msg), "Trace: lambda = %f", thisQueryTermLambda) ;
    	PRINT_TRACE (2, print_string, msg) ; 
        
        inv.id_num = query_con->con;
        if (1 != seek_inv (ip->inv_fd, &inv) ||
            1 != read_inv (ip->inv_fd, &inv)) {
            continue;
        }

        end_listwt_ptr = &inv.listwt[inv.num_listwt];
        for (listwt_ptr = inv.listwt;
             listwt_ptr < end_listwt_ptr;
             listwt_ptr++) {

            /* Ensure that did does not exceed bounds for partial similarity*/
            /* array.  Realloc if necessary */
            if (listwt_ptr->list >= results->num_full_results) {
                if (NULL == (results->full_results = (float *)
                        realloc ((char *) results->full_results,
                                 (unsigned)(listwt_ptr->list+MAX_DID_DEFAULT) *
                                 sizeof (float)))) {
                    set_error (errno, "Realloc partial sim", "sim_ctype_inv_lm_idf_aug");
                    return (UNDEF);
                }
                bzero ((char *) (&results->full_results
                                 [results->num_full_results]),
                       (int) (listwt_ptr->list + MAX_DID_DEFAULT -
                              results->num_full_results) * sizeof (float));
                results->num_full_results = listwt_ptr->list + MAX_DID_DEFAULT;
            }

            // Add the term log(docLength) for LM similarity measure.
            if ( results->full_results[listwt_ptr->list] == 0 ) {
                // If we are here for the first time, results->full_results[listwt_ptr->list] = 0
                // and so we add the docLength term.
            	// Seek the direct file to read the corresponding vector to
	            // this document.
    	        docInfo.id_num.id = listwt_ptr->list ;

        	    if (UNDEF == seek_vector (ip->doc_fd, &docInfo) ||
            	    UNDEF == read_vector (ip->doc_fd, &docInfo)) {
                	return UNDEF ;
	            }

    	        docLength = getDocLength(&docInfo) ;

                // Get the document length from this vector
                results->full_results[listwt_ptr->list] = log(docLength) ;

				snprintf(msg, sizeof(msg), "Trace: log(docLength) = %f", results->full_results[listwt_ptr->list]) ;
    			PRINT_TRACE (4, print_string, msg) ; 
            }

			// Each entry in the inverted list stores the value - (tf * sum of dfs) / (df * doc length)
			// This number is multiplied by lambda / (1 - lambda).
			// Note that the order of the documents in the inverted list is invariant under the
			// above transformation.
            results->full_results[listwt_ptr->list] +=
                query_wt * log(1 + thisQueryTermLambda / (1 - thisQueryTermLambda) * listwt_ptr->wt) ;

            /* Add to top results (potentially) if new sim exceeds top_thresh*/
            /* (new_top points to last entry in top_results subarray of */
            /* results) */
            if (results->full_results[listwt_ptr->list] >=
                results->min_top_sim) {
                new_top.sim = results->full_results[listwt_ptr->list];
                new_top.did = listwt_ptr->list;
                if (UNDEF == rank_tr->proc (&new_top,
                                            results,
                                            ip->rank_tr_inst))
                    return (UNDEF);
            }
        }
    }

    (void) free_vec (&new_qvec);
    PRINT_TRACE (20, print_full_results, results);
    PRINT_TRACE (4, print_top_results, results);
    PRINT_TRACE (2, print_string, "Trace: leaving sim_ctype_inv_lm_idf_aug");
    return (1);
}

int
close_sim_ctype_inv_lm_idf_aug (inst)
int inst;
{
    STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_sim_ctype_inv_lm_idf_aug");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "ctype_inv");
        return (UNDEF);
    }
    ip  = &info[inst];
    ip->valid_info--;

    if (ip->valid_info == 0) {
        if (UNDEF == rank_tr->close_proc(ip->rank_tr_inst))
            return (UNDEF);
        if (UNDEF == close_vector(ip->doc_fd))	// close direct file
            return (UNDEF);
        if (UNDEF == close_inv (ip->inv_fd))
            return (UNDEF);
    }

	FREE(collstats_info.freq) ;
	FREE(numdocs_info.freq) ;

    PRINT_TRACE (2, print_string, "Trace: leaving close_sim_ctype_inv_lm_idf_aug");
    return (0);
}


/* All negative wts first, then by decreasing weight */
static int
comp_q_wt (ptr1, ptr2)
CON_WT *ptr1;
CON_WT *ptr2;
{
    if (ptr1->wt < 0.0) {
        if (ptr2->wt >= 0.0)
            return (-1);
        return (0);
    }
    if (ptr2->wt < 0.0)
        return (1);
    if (ptr1->wt < ptr2->wt)
        return (1);
    if (ptr1->wt > ptr2->wt)
        return (-1);
    return (0);
}
