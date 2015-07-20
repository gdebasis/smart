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
 *1 local.retrieve.ctype_coll.ctype_inv_heap
 *2 sim_ctype_inv_heap (qvec, results, inst)
 *3   VEC *qvec;
 *3   RESULT *results;
 *3   int inst;
 *4 init_sim_ctype_inv_heap (spec, param_prefix)
 *5   "retrieve.rank_tr"
 *5   "retrieve.ctype_coll.trace"
 *5   "ctype.*.inv_file"
 *5   "ctype.*.inv_file.rmode"
 *5   "ctype.*.sim_ctype_weight"
 *4 close_sim_ctype_inv_heap (inst)

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
 *8 sim_ctype_inv_heap is most often called by retrieve.retrieve.inverted()
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
static char *inv_file;
static long inv_mode;
static float ctype_weight;
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix,  "inv_file",     getspec_dbfile,    (char *) &inv_file},
    {&prefix,  "inv_file.rmode", getspec_filemode,(char *) &inv_mode},
    {&prefix,  "sim_ctype_weight", getspec_float, (char *) &ctype_weight},
    };
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    /*inverted file info */
    char file_name[PATH_LEN];
    int inv_fd;
    float ctype_weight;                /* Weight for this particular ctype */
    int rank_tr_inst;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


#define LESS_TOP(top1,top2) (((top1).sim < (top2).sim) || (((top1).sim == (top2).sim) && ((top1).did < (top2).did)))
#define EXCHANGE(top1,top2) {\
    long temp_did = top1.did; \
    float temp_sim = top1.sim; \
    top1.did = top2.did; \
    top1.sim = top2.sim; \
    top2.did = temp_did; \
    top2.sim = temp_sim; \
}

#define BUBBLE_DOWN(current) \
while (2 * current <= results->num_top_results) { \
    if (LESS_TOP (heap[2*current], heap[current])) { \
        if ((2 * current + 1) <= results->num_top_results && \
             LESS_TOP (heap[2*current+1], heap[2*current])) { \
            EXCHANGE (heap[2*current+1], heap[current]); \
            current = 2 * current + 1; \
        } \
        else { \
            EXCHANGE (heap[2*current], heap[current]); \
            current = 2 * current; \
        } \
    } \
    else if ((2 * current + 1) <= results->num_top_results && \
             LESS_TOP (heap[2*current+1], heap[current])) { \
        EXCHANGE (heap[2*current+1], heap[current]); \
        current = 2 * current + 1; \
    } \
    else \
        break; \
}

static int comp_q_wt(), comp_top_increasing();

int
init_sim_ctype_inv_heap (spec, param_prefix)
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

    PRINT_TRACE (2, print_string, "Trace: entering init_sim_ctype_inv_heap");
    if (! VALID_FILE (inv_file)) {
        set_error (SM_ILLPA_ERR,"Non-existant inverted file", "sim_ctype_inv_heap");
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

    /* Open inverted file */
    if (UNDEF == (ip->inv_fd = open_inv(inv_file, inv_mode)))
        return (UNDEF);
    (void) strncpy (ip->file_name, inv_file, PATH_LEN);

    ip->ctype_weight = ctype_weight;
    ip->valid_info++;

    PRINT_TRACE (2, print_string, "Trace: leaving init_sim_ctype_inv_heap");
    return (new_inst);
}

int
sim_ctype_inv_heap (qvec, results, inst)
VEC *qvec;
RESULT *results;
int inst;
{
    STATIC_INFO *ip;
    CON_WT *query_con;
    CON_WT *last_query_con;
    register float query_wt;
    INV inv;
    register LISTWT *listwt_ptr, *end_listwt_ptr;
    VEC new_qvec;
    long i;

    float old_sim;
    long current;
    TOP_RESULT *heap;
    long num_in_heap;
    int is_heap;

    PRINT_TRACE (2, print_string, "Trace: entering sim_ctype_inv_heap");
    PRINT_TRACE (6, print_vector, qvec);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "sim_ctype_inv_heap");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* heap points to entry before top_results (make arithmetic simpler) */
    /* Make sure top_results is a "heap" upon entry, and find number of non_zero entries */
    heap = results->top_results - 1;
    num_in_heap = 0;
    is_heap = TRUE;
    for (i = 1; i < results->num_top_results; i++) {
        if (heap[i].sim != 0.0)
            num_in_heap++;
        if (2 * i <= results->num_top_results) {
            if (heap[i].sim > heap[2*i].sim ||
                (heap[i].sim == heap[2*i].sim &&
                 heap[i].did > heap[2*i].did))
                is_heap = FALSE;
        }
        if (2 * i + 1 <= results->num_top_results) {
            if (heap[i].sim > heap[2*i+1].sim ||
                (heap[i].sim == heap[2*i+1].sim &&
                 heap[i].did > heap[2*i+1].did))
                is_heap = FALSE;
        }
    }

    if (! is_heap) {
        /* Establish heapness by bruteforce complete sorting */
        (void) qsort ((char *) results->top_results,
                      (int) results->num_top_results,
                      sizeof (TOP_RESULT),
                      comp_top_increasing);
    }

    /* Copy query, and sort so that all negative query weights are first
       (so they don't ever enter top docs) and then by decreasing
       query weight (so fewer docs enter top docs) */
    if (UNDEF == copy_vec (&new_qvec, qvec))
        return (UNDEF);
    qsort ((char *) new_qvec.con_wtp,
           (int) new_qvec.num_conwt, 
           sizeof (CON_WT),
           comp_q_wt);

    /* Perform inverted file retrieval, sequentially going through query.
       Ignore ctype (qvec probably is only one ctype, either naturally or
       because this invoked through sim_vec_inv) */
    last_query_con = &new_qvec.con_wtp[new_qvec.num_conwt];
    for ( query_con = new_qvec.con_wtp; 
          query_con < last_query_con;
          query_con++) {

        /* Weight query term by ctype weight.  Skip this term if new query */
        /* weight is 0 */
        if (0.0 == (query_wt = ip->ctype_weight * query_con->wt)) {
            continue;
        }
        
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
                    set_error (errno, "Realloc partial sim", "sim_ctype_inv_heap");
                    return (UNDEF);
                }
                bzero ((char *) (&results->full_results
                                 [results->num_full_results]),
                       (int) (listwt_ptr->list + MAX_DID_DEFAULT -
                              results->num_full_results) * sizeof (float));
                results->num_full_results = listwt_ptr->list + MAX_DID_DEFAULT;
            }

            old_sim = results->full_results[listwt_ptr->list];
            results->full_results[listwt_ptr->list] +=
                query_wt * listwt_ptr->wt;

            /* Add to top results if new sim high enough  */
            if (results->full_results[listwt_ptr->list] > heap[1].sim || 
                (results->full_results[listwt_ptr->list] == heap[1].sim &&
                 listwt_ptr->list > heap[1].did)) {

                if (old_sim > 0.0 && (old_sim > heap[1].sim ||
                    (old_sim == heap[1].sim && listwt_ptr->list >= heap[1].did))) {
                    /* new already in heap. Look for it */
                    for (current = results->num_top_results; current > 0; current --) {
                        if (heap[current].did == listwt_ptr->list) {
                            heap[current].sim = results->full_results[listwt_ptr->list];
                            BUBBLE_DOWN (current);
                            break;
                        }
                    }
                }
                else if (num_in_heap < results->num_top_results) {
                    /* Add just before beginning of nonempty elements */
                    current = results->num_top_results - num_in_heap;
                    heap[current].did = listwt_ptr->list;
                    heap[current].sim = results->full_results[listwt_ptr->list];
                    BUBBLE_DOWN (current);
                    num_in_heap++;
                }
                else {
                    /* new doc is not already in heap */
                    current = 1;
                    heap[current].did = listwt_ptr->list;
                    heap[current].sim = results->full_results[listwt_ptr->list];
                    BUBBLE_DOWN (current);
                }
            }
        }
    }

    results->min_top_sim = heap[1].sim;

    PRINT_TRACE (20, print_full_results, results);
    PRINT_TRACE (4, print_top_results, results);
    PRINT_TRACE (2, print_string, "Trace: leaving sim_ctype_inv_heap");
    return (1);
}


int
close_sim_ctype_inv_heap (inst)
int inst;
{
    STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_sim_ctype_inv_heap");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "ctype_inv");
        return (UNDEF);
    }
    ip  = &info[inst];
    ip->valid_info--;

    if (ip->valid_info == 0) {
        if (UNDEF == rank_tr->close_proc(ip->rank_tr_inst))
            return (UNDEF);
        if (UNDEF == close_inv (ip->inv_fd))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_sim_ctype_inv_heap");
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

static int
comp_top_increasing (ptr1, ptr2)
TOP_RESULT *ptr1;
TOP_RESULT *ptr2;
{
    if (ptr1->sim < ptr2->sim)
        return (-1);
    if (ptr1->sim > ptr2->sim)
        return (1);
    if (ptr1->did < ptr2->did)
        return (-1);
    if (ptr1->did > ptr2->did)
        return (1);
    return (0);
}


/* Keep track of top docs by using heap */
/* heap[1] = root = smallest top results */
/* node i has children 2i, 2i+1, with heap[i] < heap[2i], heap[2i+1] */
/* Know whether doc is in heap already if old sim is > root sim */
/* Upon getting new doc not in heap,
         pop the root
         put its smallest child into its spot
         continue all the way down.
         when at end, put the new doc into the last spot vacated.
         push it up to its proper location as long as smaller than parent
   Upon getting doc already in heap,
         Find current spot in heap, sequentially searching from end
         update its sim.
         push it down to its proper location as long as larger than child
*/






