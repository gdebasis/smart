#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libinter/segment.c,v 11.2 1993/02/03 16:52:09 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Generate segments for a document.
 *2 segment (is, unused)
 *3   INTER_STATE *is;
 *3   char *unused;
 *4 init_segment (spec, unused)
 *4 close_segment (inst)

 *7 Call compare_core to generate the text relationship map and apply the
 *7 segmentation algorithm to the results.

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

#define EID_NEAR(e1, e2, d) (e1.id == e2.id && e1.ext.type == e2.ext.type && \
			     abs(e1.ext.num - e2.ext.num) <= d)

static long vicinity;
static SPEC_PARAM spec_args[] = {
    PARAM_LONG("local.inter.segment.vicinity", &vicinity),
  TRACE_PARAM ("local.inter.segment.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


typedef struct {
  int valid_info;	/* bookkeeping */

  int compare_inst; 

  int max_results;
  ALT_RESULTS seg_altres;
  int max_eids, max_nodes;
  EID_LIST eidlist;
  SEGINFO *nodelist, **sorted_by_ce;

  int vicinity;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int compare_seginfo();


int
init_segment_core (spec, unused)
SPEC *spec;
char *unused;
{
    int new_inst;
    STATIC_INFO *ip;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_segment_core");

    NEW_INST( new_inst );
    if (UNDEF == new_inst)
        return UNDEF;
    ip = &info[new_inst];
    ip->valid_info = 1;

    if (UNDEF==(ip->compare_inst=init_compare_core(spec, "local.inter.segment.")))
	return(UNDEF);

    ip->max_nodes = ip->max_eids = 0;
    ip->max_results = 0;
    ip->vicinity = vicinity;

    PRINT_TRACE (2, print_string, "Trace: leaving init_segment_core");
    return new_inst;
}


int
close_segment_core (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_segment_core");
    if (!VALID_INST(inst)) {
        set_error( SM_ILLPA_ERR, "Instantiation", "close_segment_core");
        return UNDEF;
    }
    ip = &info[inst];

    if (UNDEF == close_compare_core(ip->compare_inst))
	return(UNDEF);

    if (ip->max_eids) {
        Free(ip->eidlist.eid);
	Free(ip->nodelist);
	Free(ip->sorted_by_ce);
    }
    if (ip->max_results) Free(ip->seg_altres.results);

    ip->valid_info = 0;
    PRINT_TRACE (2, print_string, "Trace: leaving close_segment_core");
    return (0);
}


int
segment_core(parse_results, seg_results, inst)
COMLINE_PARSE_RES *parse_results;
SEG_RESULTS *seg_results;
int inst;
{
    int num_segs, i, j;
    EID curr_id;
    STATIC_INFO *ip;
    RESULT_TUPLE *start, *end, *rtup, *dtup, *ptr;
    ALT_RESULTS altres;
    SEGINFO *seg_ptr;
    static int max_results;
    static ALT_RESULTS tmp_altres; /* keep this around to save on mallocs */

    PRINT_TRACE (2, print_string, "Trace: entering segment_core");
    if (!VALID_INST(inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "segment_core");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Check if the vicinity parameter has been set on the command line. 
     * All other options are ignored. 
     */
    for (i = 0; i < parse_results->params->num_mods; i++) {
        if (0 == strncmp(parse_results->params->modifiers[i], "vici", 4 )) {
	    long vicinity = atol(parse_results->params->modifiers[++i]);
	    if (vicinity > 0)
		(void) printf("Links within vicinity %d allowed\n",
			      ip->vicinity=vicinity);
	    else
		(void) printf("Vicinity unchanged, still %d\n", ip->vicinity);
	}
    }

    if (ip->vicinity <= 0) {
        printf("Invalid vicinity\n");
	return(UNDEF);
    }

    /* Get the map. */
    if (UNDEF == compare_core(parse_results, &altres, ip->compare_inst))
        return(UNDEF);
    
    /* Impose vicinity restriction. */
    if (altres.num_results > ip->max_results) {
        if (ip->max_results) Free(ip->seg_altres.results);
	ip->max_results += altres.num_results;
	if (NULL == (ip->seg_altres.results = Malloc(ip->max_results,
						     RESULT_TUPLE)))
	    return(UNDEF);
    }
    rtup = altres.results; dtup = ip->seg_altres.results;
    for (i=0, j=0; i < altres.num_results; i++)
        if (EID_NEAR(rtup[i].qid, rtup[i].did, ip->vicinity))
	    Bcopy(&rtup[i], &dtup[j++], sizeof(RESULT_TUPLE));
    ip->seg_altres.num_results = j;
    if (UNDEF == build_eidlist(&ip->seg_altres, &ip->eidlist, &ip->max_eids)) 
        return(UNDEF);

    /* Replace each `undirected' edge (result_tuple) by 2 `directed' edges
     * by making 2 copies of the result.
     */
    if (2 * ip->seg_altres.num_results > max_results) {
        if (max_results) Free(tmp_altres.results);
	max_results += 2 * ip->seg_altres.num_results;
	if (NULL == (tmp_altres.results = Malloc(max_results,
						 RESULT_TUPLE)))
	    return(UNDEF);
    }
    rtup = ip->seg_altres.results; dtup = tmp_altres.results;
    for (i=0, j=0; i < ip->seg_altres.num_results; i++) {
	dtup[j++] = rtup[i];
	dtup[j].qid = rtup[i].did;
	dtup[j].did = rtup[i].qid;
	dtup[j++].sim = rtup[i].sim;
    }

    /* Sort double results by source of the edge so that all edges for a 
     * node can be found at one place.
     */
    qsort((char *)dtup, 2*ip->seg_altres.num_results,
	  sizeof(RESULT_TUPLE), compare_rtups);

    /* Find the number of edges crossing each node. */
    if (ip->max_nodes < ip->eidlist.num_eids) {
        if (ip->max_nodes) {
	  Free(ip->nodelist);
	  Free(ip->sorted_by_ce);
	}
	ip->max_nodes += ip->eidlist.num_eids;
	if (NULL == (ip->nodelist = Malloc(ip->max_nodes, SEGINFO)) ||
	    NULL == (ip->sorted_by_ce = Malloc(ip->max_nodes, SEGINFO *)))
	    return(UNDEF);
    }
    for (i = 0; i < ip->eidlist.num_eids; i++) {
        ip->nodelist[i].did = ip->eidlist.eid[i];
	ip->nodelist[i].cross_edges = ip->nodelist[i].cross_sim = 0;
	ip->nodelist[i].beg_seg = ip->nodelist[i].end_seg = FALSE;
	ip->sorted_by_ce[i] = &(ip->nodelist[i]);
    }
    start = ptr = dtup;
    for (seg_ptr = ip->nodelist;
	 seg_ptr < ip->nodelist + ip->eidlist.num_eids &&
	 ptr < dtup + 2 * ip->seg_altres.num_results;
	 seg_ptr++) {
	assert(EID_EQUAL(seg_ptr->did, ptr->qid));
	curr_id = seg_ptr->did;
	/* cross-edges can start at 1st node for this doc (same part type) */
	if (start->qid.id != curr_id.id ||
	    start->qid.ext.type != curr_id.ext.type)
	    start = ptr;
	/* cross-edges cannot start at a later node */
	for (end = ptr; 
	     end < dtup + 2 * ip->seg_altres.num_results && 
	       EID_EQUAL(end->qid, curr_id); 
	     end++);
	/* count edges */
	for (ptr = start; ptr < end; ptr++) {
	    if (!eid_greater(&ptr->qid, &curr_id) && 
		eid_greater(&ptr->did, &curr_id)) {
		seg_ptr->cross_edges++;
		seg_ptr->cross_sim += ptr->sim;
	    }
	}
    }

    /* Sort nodes by number of cross-edges. */
    qsort((char *)ip->sorted_by_ce, ip->eidlist.num_eids, sizeof(SEGINFO *), 
	  compare_seginfo);

    /* Now find the segments. */
    if (ip->eidlist.num_eids > 0) 
        ip->nodelist[0].beg_seg = TRUE;	/* first node starts new segment */
    num_segs = 0;
    /* If no links cross a boundary, there is a segment break. */
    for (i = 0; i < ip->eidlist.num_eids && 
	   ip->sorted_by_ce[i]->cross_edges == 0; i++) {
	SEGINFO *next;

	for (seg_ptr=ip->nodelist; seg_ptr != ip->sorted_by_ce[i]; seg_ptr++);
	next = seg_ptr + 1;
	if (next < ip->nodelist + ip->eidlist.num_eids) next->beg_seg = TRUE;
	seg_ptr->end_seg = TRUE;
	num_segs++;
    }

    /* Nodes with less than vicinity/2 cross-edges may start new segment. */
    for (; i < ip->eidlist.num_eids && 
	   ip->sorted_by_ce[i]->cross_edges <= ip->vicinity/2; i++) {
        SEGINFO *next, *tmp_ptr;
	int found = FALSE;

	for (seg_ptr = ip->nodelist; seg_ptr != ip->sorted_by_ce[i]; 
	     seg_ptr++);
	next = seg_ptr+1;

	/* Ensure no node within vici of this node ended a segment. */
	for (tmp_ptr = seg_ptr;
	     tmp_ptr >= ip->nodelist &&
		 EID_NEAR(tmp_ptr->did, seg_ptr->did, ip->vicinity - 1); 
	     tmp_ptr--) {
	  if (tmp_ptr->end_seg) {
	      found = TRUE;
	      break;
	  }
	} 
	for (tmp_ptr = seg_ptr;
	     EID_NEAR(tmp_ptr->did, seg_ptr->did, ip->vicinity - 1) &&
	       tmp_ptr < ip->nodelist + ip->eidlist.num_eids; tmp_ptr++) {
	    if (tmp_ptr->end_seg) {
	        found = TRUE;
		break;
	    }
	}

	/* Check if the next node on the list can start a new segment. */
	for (tmp_ptr = next;
	     EID_NEAR(tmp_ptr->did, next->did, ip->vicinity - 1) &&
	       tmp_ptr >= ip->nodelist; tmp_ptr--) {
	    if (tmp_ptr->beg_seg) {
		found = TRUE;
		break;
	    }
	}
	for (tmp_ptr = next;
	     tmp_ptr < ip->nodelist + ip->eidlist.num_eids &&
		 EID_NEAR(tmp_ptr->did, next->did, ip->vicinity - 1); 
	     tmp_ptr++) {
	    if (tmp_ptr->beg_seg) {
		found = TRUE;
		break;
	    }
	}

	if (found) continue;	/* no segment break here */
	seg_ptr->end_seg = TRUE;
	if (next < ip->nodelist + ip->eidlist.num_eids) next->beg_seg = TRUE;
	num_segs++;
    }

    seg_results->num_segs = num_segs;
    seg_results->num_nodes = ip->eidlist.num_eids;
    seg_results->nodes = ip->nodelist;
    seg_results->seg_altres = &ip->seg_altres;

    PRINT_TRACE (2, print_string, "Trace: leaving segment_core");
    return(1);
}


static int
compare_seginfo( d1, d2 )
SEGINFO **d1;
SEGINFO **d2;
{
    if ((*d1)->cross_edges > (*d2)->cross_edges)
	return 1;
    if ((*d1)->cross_edges < (*d2)->cross_edges)
	return -1;

    if ((*d1)->cross_sim > (*d2)->cross_sim)
	return 1;
    if ((*d1)->cross_sim < (*d2)->cross_sim)
	return -1;

    if (eid_greater(&(*d1)->did, &(*d2)->did))
	return 1;
    if (eid_greater(&(*d2)->did, &(*d1)->did))
	return -1;

    return 0;
}
