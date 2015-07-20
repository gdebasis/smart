#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libretrieve/sim_cty.c,v 11.2 1993/02/03 16:52:55 smart Exp $";
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
 *8 sim_ctype_inv is most often called by retrieve.retrieve.inverted()

 This retrieval score computes the similarity score to approximate the
 edit distance computation score.
 Score(Q,D) = 	log[min(|Q|,|D|)/max(|Q|,|D|)] +
 				\sum_{qi} log(\sum_{j \in pos(qi)} 1/(|i-j|+delta))
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
#include "invpos.h"
#include "tr_vec.h"
#include "inst.h"

/*  Perform inner product similarity function using inverted file to access */
/*  document weights.  Query weights given by query vector.  Weight for */
/*  type of query term (ctype weight) is given by ret_spec file. */

/* Basic algorithm: malloc enough space to keep track of all similarities */
/*  of documents with direct access by did. */
/*  Top num_wanted similarites kept track of throughout. */

static PROC_TAB *rank_tr;

static SPEC_PARAM spec_args[] = {
    {"retrieve.rank_tr",          getspec_func, (char *) &rank_tr},
    TRACE_PARAM ("retrieve.ctype_coll.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *prefix;
static char *doc_file;
static long doc_mode;
static char *inv_file;
static long inv_mode;
static float ctype_weight;

static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix,  "inv_file",     getspec_dbfile,    (char *) &inv_file},
    {&prefix, "doc_file",     getspec_dbfile,    (char *) &doc_file},
    {&prefix, "doc_file.rmode", getspec_filemode,(char *) &doc_mode},
    {&prefix,  "inv_file.rmode", getspec_filemode,(char *) &inv_mode},
    {&prefix,  "sim_ctype_weight", getspec_float, (char *) &ctype_weight},
    };
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

// Collection information
typedef struct {
    long *freq;		// frequency of the ith term in the collection
    long num_freq;	// total number of terms in collection
} coll_info ;

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    /*inverted file info */
    char file_name[PATH_LEN];
    int inv_fd;
    int doc_fd;
    float ctype_weight;                /* Weight for this particular ctype */
    int rank_tr_inst;

} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int comp_q_wt();
static float getTotalDocFreq(STATIC_INFO*);

int
init_sim_ctype_invpos_noprior (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    prefix = param_prefix;
    if (UNDEF == lookup_spec_prefix (spec, &pspec_args[0], num_pspec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_sim_ctype_invpos_noprior");
    if (! VALID_FILE (inv_file)) {
        set_error (SM_ILLPA_ERR,"Non-existant inverted file", "sim_ctype_invpos_noprior");
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
    /* Open direct file for reading document lengths */
    if (UNDEF == (ip->doc_fd = open_vector (doc_file, doc_mode)))
        return (UNDEF);

    /* Open inverted file */
    if (UNDEF == (ip->inv_fd = open_invpos(inv_file, inv_mode)))
        return (UNDEF);
    (void) strncpy (ip->file_name, inv_file, PATH_LEN);

    ip->ctype_weight = ctype_weight;
    ip->valid_info++;

    PRINT_TRACE (2, print_string, "Trace: leaving init_sim_ctype_invpos_noprior");
    return (new_inst);
}

static float getDocLength(STATIC_INFO* ip, int id)
{
    int  i, length = 0;
	VEC  vec;

    vec.id_num.id = id;
    if (UNDEF == seek_vector (ip->doc_fd, &vec) ||
        UNDEF == read_vector (ip->doc_fd, &vec)) {
		return UNDEF;
	}

    for (i = 0; i < vec.ctype_len[0]; i++) {
        length += vec.con_wtp[i].wt ;
    }
	return length;
}

int
sim_ctype_invpos_noprior (qvec, results, inst)
VEC *qvec;
RESULT *results;
int inst;
{
    STATIC_INFO *ip;
    CON_WT *query_con;
    CON_WT *last_query_con;
    register float query_wt;
    INVPOS inv;
    register LISTWTPOS *listwt_ptr, *end_listwt_ptr, *prev_listwt_ptr;
    TOP_RESULT new_top;
	int qcon_offset;
	static float delta = 1.0f;
	float posdiff_comp;
	int  pos_abs_diff;
	float  docLength;
	int  qryLength;
	int  minpos_diff = 100000;

    PRINT_TRACE (2, print_string, "Trace: entering sim_ctype_invpos_noprior");
    PRINT_TRACE (6, print_vector, qvec);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "sim_ctype_invpos_noprior");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Perform inverted file retrieval, sequentially going through query.
       Ignore ctype (qvec probably is only one ctype, either naturally or
       because this invoked through sim_vec_inv) */
    last_query_con = &qvec->con_wtp[qvec->num_conwt];
	qryLength = qvec->ctype_len[0];
    for ( query_con = qvec->con_wtp + qryLength;	// start of the positional information 
          query_con < last_query_con;
          query_con++) {

        inv.id_num = query_con->con;
        if (1 != seek_invpos (ip->inv_fd, &inv) ||
            1 != read_invpos (ip->inv_fd, &inv)) {
            continue;
        }

        end_listwt_ptr = &inv.listwtpos[inv.num_listwt];
        for (listwt_ptr = inv.listwtpos, prev_listwt_ptr = NULL;
             listwt_ptr < end_listwt_ptr;
             prev_listwt_ptr = listwt_ptr++) {
            /* Ensure that did does not exceed bounds for partial similarity*/
            /* array.  Realloc if necessary */
            if (listwt_ptr->list >= results->num_full_results) {
                if (NULL == (results->full_results = (float *)
                        realloc ((char *) results->full_results,
                                 (unsigned)(listwt_ptr->list+MAX_DID_DEFAULT) *
                                 sizeof (float)))) {
                    set_error (errno, "Realloc partial sim", "sim_ctype_invpos_noprior");
                    return (UNDEF);
                }
                bzero ((char *) (&results->full_results
                                 [results->num_full_results]),
                       (int) (listwt_ptr->list + MAX_DID_DEFAULT -
                              results->num_full_results) * sizeof (float));
                results->num_full_results = listwt_ptr->list + MAX_DID_DEFAULT;
            }

			pos_abs_diff = abs((int)query_con->wt - listwt_ptr->pos);
			if (prev_listwt_ptr) {
				if (listwt_ptr->list == prev_listwt_ptr->list) {
					if (pos_abs_diff < minpos_diff) {
						minpos_diff = pos_abs_diff;
					}
				}
				else {
					// a new document id... not mutiple occs in same document
					// Add the component log (1 + 1/(|qpos(t) - dpos(t)|+delta))
					posdiff_comp = log (1 + 1/(minpos_diff + delta));
            		results->full_results[prev_listwt_ptr->list] += ctype_weight * posdiff_comp;
					minpos_diff = pos_abs_diff;
				}
			}
			else {
				minpos_diff = pos_abs_diff;
			}
#if 0			
	        // Add the prior probability of relevance. In the case of edit-distance
			// computation, the closer the document and the query length, the more is the prior
			// probability of relevance.
            if (results->full_results[listwt_ptr->list] == 0) {
                docLength = getDocLength(ip, listwt_ptr->list);
                results->full_results[listwt_ptr->list] = log(1 + MIN(docLength, qryLength)/(float)MAX(docLength, qryLength)) ;
            }
#endif			
		
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
		posdiff_comp = log (1 + 1/(minpos_diff + delta));
   		results->full_results[end_listwt_ptr->list] += ctype_weight * posdiff_comp;
    }

    PRINT_TRACE (20, print_full_results, results);
    PRINT_TRACE (4, print_top_results, results);
    PRINT_TRACE (2, print_string, "Trace: leaving sim_ctype_invpos_noprior");
    return (1);
}


int
close_sim_ctype_invpos_noprior (inst)
int inst;
{
    STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_sim_ctype_invpos_noprior");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "ctype_invpos_noprior");
        return (UNDEF);
    }
    ip  = &info[inst];
    ip->valid_info--;

    if (ip->valid_info == 0) {
        if (UNDEF == rank_tr->close_proc(ip->rank_tr_inst))
            return (UNDEF);
        if (UNDEF == close_invpos (ip->inv_fd))
            return (UNDEF);
    	if (UNDEF == close_vector (ip->doc_fd))
        	return (UNDEF);
	}

    PRINT_TRACE (2, print_string, "Trace: leaving close_sim_ctype_invpos");
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
