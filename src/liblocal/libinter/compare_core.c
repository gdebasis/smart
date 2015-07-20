#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libinter/compare_doc.c,v 11.2 1993/02/03 16:52:09 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Basic engine for comparing queries, documents, and parts.
 *2 compare_core (parse_results, altres, inst)
 *3    COMLINE_PARSE_RES *parse_results;
 *3    ALT_RESULTS *altres;
 *3    int inst;
 *4 init_compare_core (spec, prefix)
 *4 close_compare_core (inst)

 *7 
***********************************************************************/

#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "compare.h"
#include "context.h"
#include "local_eid.h"
#include "functions.h"
#include "inst.h"
#include "inter.h"
#include "param.h"
#include "proc.h"
#include "retrieve.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"

static PROC_TAB *vec_vec_ptab;
static SPEC_PARAM spec_args[] = {
    PARAM_FUNC("local.inter.compare.vec_vec", &vec_vec_ptab),
    TRACE_PARAM ("local.inter.compare_core.trace")
};
static int num_spec_args = sizeof(spec_args) / sizeof(spec_args[0]);

static float csim, density;
static char *prefix;
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    {&prefix, "csim", getspec_float, (char *)&csim},
    {&prefix, "density", getspec_float, (char *)&density},
    };
static int num_prefix_spec_args = sizeof (prefix_spec_args) /
         sizeof (prefix_spec_args[0]);


typedef struct {
  int valid_info;	/* bookkeeping */

  int eid_inst, qvec_inst, vv_inst;  
  PROC_TAB *vec_vec;

  int max_eids;
  EID_LIST eidlist;
  int max_vecs;
  VEC *vecs;
  int max_results, num_saved_results;
  ALT_RESULTS altres;
  float csim, density;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int
init_compare_core (spec, passed_prefix)
SPEC *spec;
char *passed_prefix;
{
    int new_inst;
    STATIC_INFO *ip;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_compare_core");
    NEW_INST( new_inst );
    if (UNDEF == new_inst)
        return UNDEF;
    ip = &info[new_inst];
    ip->valid_info = 1;

    prefix = passed_prefix;
    if (UNDEF == lookup_spec_prefix (spec, &prefix_spec_args[0],
				     num_prefix_spec_args))
	return UNDEF;

    ip->vec_vec = vec_vec_ptab;
    if (UNDEF == (ip->eid_inst = init_eid_util(spec, NULL)) ||
	UNDEF == (ip->qvec_inst = init_qid_vec(spec, NULL)) ||
	UNDEF == (ip->vv_inst = ip->vec_vec->init_proc(spec, NULL)))
	return(UNDEF);

    ip->max_eids = 0;
    ip->max_vecs = 0;
    ip->max_results = 0; 
    ip->num_saved_results = UNDEF;
    ip->csim = csim;
    ip->density = density;

    PRINT_TRACE (2, print_string, "Trace: leaving init_compare_core");
    return new_inst;
}


int
close_compare_core (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_compare_core");
    if (!VALID_INST(inst)) {
        set_error( SM_ILLPA_ERR, "Instantiation", "close_compare_core");
        return UNDEF;
    }
    ip = &info[inst];

    if (UNDEF == close_eid_util(ip->eid_inst) ||
	UNDEF == close_qid_vec(ip->qvec_inst) ||
	UNDEF == ip->vec_vec->close_proc(ip->vv_inst))
	return(UNDEF);

    if (ip->max_eids) Free(ip->eidlist.eid);
    if (ip->max_vecs) Free(ip->vecs);
    if (ip->max_results) Free(ip->altres.results); 

    ip->valid_info = 0;
    PRINT_TRACE (2, print_string, "Trace: leaving close_compare_core");
    return (0);
}


int
compare_core(parse_results, altres, inst)
COMLINE_PARSE_RES *parse_results;
ALT_RESULTS *altres;
int inst;
{
    char use_previous = FALSE;
    int num_res, i;
    EID *eptr;
    VEC_PAIR vpair;
    RESULT_TUPLE *rptr;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering compare_core");
    if (!VALID_INST(inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "compare_core");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Check if the link density has been set on the command line, or 
     *  the `same' option has been used. All other options are ignored. */
    for (i = 0; i < parse_results->params->num_mods; i++) { 
	if (0 == strncmp(parse_results->params->modifiers[i], "dens", 4)) {
	    double dens = atof(parse_results->params->modifiers[++i]);
	    if ( dens >= 0.0 ) {
		ip->density = (float)dens;
		(void) printf("Link density set to %f\n", ip->density );
	    }
	    else (void) printf("Link density unchanged (still %f)\n",
			       ip->density );
	    continue;
	}
	if (0==strcasecmp(parse_results->params->modifiers[i], "same")) {
	    use_previous = TRUE;
	    continue;
	}
    }

    if (!use_previous) { 
	/* Get all the vectors needed at once so that we don't have to get
	 * a vector everytime we do a comparison.
	 */
	if (ip->max_vecs < parse_results->eids->num_eids) {
	    if (ip->max_vecs) Free(ip->vecs);
	    ip->max_vecs += parse_results->eids->num_eids;
	    if (NULL == (ip->vecs = Malloc(ip->max_vecs, VEC)))
		return(UNDEF);
	}
	for (eptr = parse_results->eids->eid, i = 0; 
	     eptr < parse_results->eids->eid + parse_results->eids->num_eids; 
	     eptr++, i++) { 
	    if (eptr->ext.type == QUERY) {
		if (UNDEF == qid_vec(&eptr->id, &ip->vecs[i], ip->qvec_inst) ||
		    UNDEF == save_vec(&ip->vecs[i])) 
		    return(UNDEF);
		ip->vecs[i].id_num.ext.type = QUERY;
	    }
	    else 
		if (UNDEF == eid_vec(*eptr, &ip->vecs[i], ip->eid_inst) ||
		    UNDEF == save_vec(&ip->vecs[i]))
		    return(UNDEF);
	}

	/* Required vectors are already in ip->vecs. Find them and do the 
	 * pairwise comparisons to get the similarities. 
	 */
	num_res = parse_results->res->num_results; 
	for (rptr = parse_results->res->results; 
	     rptr < parse_results->res->results + num_res;
	     rptr++) {
	    vpair.vec1 = vpair.vec2 = NULL;
	    for (i = 0; i < parse_results->eids->num_eids; i++) {
		if (EID_EQUAL(rptr->qid, ip->vecs[i].id_num))
		    vpair.vec1 = &ip->vecs[i];
		if (EID_EQUAL(rptr->did, ip->vecs[i].id_num))
		    vpair.vec2 = &ip->vecs[i];
	    }
	    if (vpair.vec1 == NULL || vpair.vec2 == NULL ||
		UNDEF == ip->vec_vec->proc(&vpair, &rptr->sim, ip->vv_inst))
		return(UNDEF);
	}
	
	/* Free up all the vectors. */
	for (i = 0; i < parse_results->eids->num_eids; i++)
	    free_vec(&ip->vecs[i]);

	/* Save results in case user used `same' option later. Since no result-
	 * tuples are actually thrown away, this basically amounts to saving 
	 * simply the original number of pairs i.e. before any processing 
	 * (using dens) is done.
	 */
	if (num_res > ip->max_results) {
	    if (ip->max_results) Free(ip->altres.results);
	    ip->max_results += num_res;
	    if (NULL == (ip->altres.results = Malloc(ip->max_results, 
						     RESULT_TUPLE)))
		return(UNDEF);
	}
	Bcopy(parse_results->res->results, ip->altres.results, 
	      num_res * sizeof(RESULT_TUPLE));
	ip->num_saved_results = ip->altres.num_results = num_res;
    }
    else {
	if (ip->num_saved_results == -1) {
	    (void) printf("No previous results\n");
	    return(UNDEF);
	}
	ip->altres.num_results = ip->num_saved_results;
    }

    /* Impose csim restriction. */
    qsort((char *)ip->altres.results, ip->altres.num_results, 
	  sizeof(RESULT_TUPLE), compare_rtups_sim);

    while (ip->altres.num_results > 0 && 
	   ip->altres.results[ip->altres.num_results-1].sim < ip->csim) 
        ip->altres.num_results--;
    /* Impose density restriction. */
    if (UNDEF == build_eidlist(&ip->altres, &ip->eidlist, &ip->max_eids)) 
	return(UNDEF);
    if (ip->density > 0)
	ip->altres.num_results = MIN(ip->altres.num_results, 
				     ip->density * ip->eidlist.num_eids);
    *altres = ip->altres;

    PRINT_TRACE (2, print_string, "Trace: leaving compare_core");
    return(1);
}
