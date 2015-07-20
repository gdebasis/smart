#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libinter/path.c,v 11.2 1993/02/03 16:52:09 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Generate various reading paths for a document.
 *2 path (is, unused)
 *3   INTER_STATE *is;
 *3   char *unused;
 *4 init_path (spec, unused)
 *4 close_path (inst)

 *7 Call compare_core to generate the text relationship map and apply the
 *7 desired path generation algorithm to the results.

***********************************************************************/

#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "compare.h"
#include "context.h"
#include "functions.h"
#include "inst.h"
#include "inter.h"
#include "local_eid.h"
#include "local_functions.h"
#include "param.h"
#include "proc.h"
#include "retrieve.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"

#define BUSHY 0
#define DFP 1
#define SEG_BUSHY 2
#define SEG_DFP 3

static char *ptype;
static double percent;

static SPEC_PARAM spec_args[] = {
    {"local.inter.path.path_type", getspec_string, (char *)&ptype},
    {"inter.path.default_percent", getspec_double, (char *)&percent},
  TRACE_PARAM ("local.inter.path.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

typedef struct {
    EID node_id;
    int degree;
    char beg_seg, end_seg;
} NODEINFO;

typedef struct {
    int valid_info;	/* bookkeeping */

    int lcom_parse_inst, comp_inst, seg_inst, show_inst; 
    
    EID first_node;
    int max_pathlen;
    EID_LIST path;

    int max_nodes;
    EID_LIST nodelist;
    int max_nodeinfo;
    NODEINFO *nodes;

    char ptype;
    double percent;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static char buf[PATH_LEN];
static char *path_type[] = {" Global bushy ", " Global depth-first ", 
			    " Segmented bushy ", " Segmented depth-first "};
static int global_path_core(), seg_path_core(), bushy(), dfp(), seg_bushy(), seg_dfp();
static int compare_degree();


int
init_path (spec, unused)
SPEC *spec;
char *unused;
{
    int new_inst;
    STATIC_INFO *ip;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_path");
    NEW_INST( new_inst );
    if (UNDEF == new_inst)
        return UNDEF;
    ip = &info[new_inst];
    ip->valid_info = 1;

    if (UNDEF==(ip->lcom_parse_inst = init_local_comline_parse(spec, NULL)) ||
	UNDEF==(ip->comp_inst = init_compare_core(spec, "local.inter.compare.")) ||
	UNDEF==(ip->seg_inst = init_segment_core(spec, NULL)) ||
	UNDEF==(ip->show_inst = init_show_doc_part(spec, NULL)))
	return(UNDEF);

    ip->first_node.id = UNDEF;
    ip->max_pathlen = 0;
    ip->max_nodes = 0; 
    ip->max_nodeinfo = 0;

    switch (tolower(*ptype)) {
    case 'b': 
	ip->ptype = BUSHY;
	break;
    case 'd':
	ip->ptype = DFP;
	break;
    case 's':
	if (*(++ptype) && tolower(*ptype) == 'd')
	    ip->ptype = SEG_DFP;
	else ip->ptype = SEG_BUSHY;
	break;
    default:
	set_error(SM_ILLPA_ERR, "Unknown path type", "init_path");
	return(UNDEF);
    }
    ip->percent = percent;

    PRINT_TRACE (2, print_string, "Trace: leaving init_path");
    return new_inst;
}


int
close_path (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_path");
    if (!VALID_INST(inst)) {
        set_error( SM_ILLPA_ERR, "Instantiation", "close_path");
        return UNDEF;
    }
    ip = &info[inst];

    if (UNDEF == close_local_comline_parse(ip->lcom_parse_inst) ||
	UNDEF == close_compare_core(ip->comp_inst) ||
	UNDEF == close_segment_core(ip->seg_inst) ||
	UNDEF == close_show_doc_part(ip->show_inst))
	return(UNDEF);

    if (ip->max_pathlen) Free(ip->path.eid);
    if (ip->max_nodes) Free(ip->nodelist.eid);
    if (ip->max_nodeinfo) Free(ip->nodes);

    ip->valid_info = 0;
    PRINT_TRACE (2, print_string, "Trace: leaving close_path");
    return (0);
}


int
path (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    char *param_str, *str;
    int seg_count, status, i;
    COMLINE_PARSE_RES parse_results;
    EID_LIST path;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering path");
    if (!VALID_INST(inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "path");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (1 != (status = local_comline_parse(is, &parse_results, 
					   ip->lcom_parse_inst)))
	return(status);

    /* Check if the path type or path length has been set on the command 
     * line. All other options are ignored. 
     */
    for (i = 0; i < parse_results.params->num_mods; i++) { 
	param_str = parse_results.params->modifiers[i];
        if (param_str[strlen(param_str)-1] == '%' ) {
            double percent = atof(param_str);
            if (percent > 0 && percent <= 100) {
		printf("Will generate %4.2f%% summary\n", ip->percent=percent);
	    }
	    else printf("Illegal length specification: %4.2f\n", percent);
            continue;
        }
	if (0 == strncmp( param_str, "type", 5 )) {
	    param_str = parse_results.params->modifiers[++i];
	    switch (tolower(*param_str)) {
	    case 'b': 
		ip->ptype = BUSHY;
		break;
	    case 'd':
		ip->ptype = DFP;
		break;
	    case 's':
		if (*(++param_str) && tolower(*param_str) == 'd')
		    ip->ptype = SEG_DFP;
		else ip->ptype = SEG_BUSHY;
		break;
	    default:
		printf("Unknown path type %c.\n", *param_str);
		break;
	    }
	    continue;
	}    
    }

    /* Get the path. */
    if (ip->ptype == BUSHY || ip->ptype == DFP) {
	if (UNDEF == global_path_core(&parse_results, &path, inst)) {
	    if (UNDEF == add_buf_string("Couldn't generate reading path\n",
					&is->err_buf))
		return UNDEF;
	    return(0);
	}
    }
    if (ip->ptype == SEG_BUSHY || ip->ptype == SEG_DFP) {
	if (UNDEF == seg_path_core(&parse_results, &path, inst)) {
	    if (UNDEF == add_buf_string("Couldn't generate reading path\n",
					&is->err_buf))
		return UNDEF;
	    return(0);
	}
    }

    /* Display path. */
    qsort((char *) path.eid, path.num_eids, sizeof(EID), compare_eids);
    sprintf(buf, "-- %2.1f%%%spath (%ld nodes) --\n\n-- Path nodes --\n", 
	    ip->percent, path_type[(int)ip->ptype], path.num_eids);
    for (seg_count = 1, i=0; i < path.num_eids; i++) {
	eid_to_string(path.eid[i], &str);
	strcat(buf, str);
	strcat(buf, (((i+1)%5 == 0) && i<path.num_eids-1) ? "\n" : "\t");
    }
    if (UNDEF == add_buf_string( buf, &is->output_buf ))
	return UNDEF;

    sprintf(buf, "\n\n-- Path text -- \n");
    if (UNDEF == add_buf_string( buf, &is->output_buf ))
	return UNDEF;
    for (i=0; i < path.num_eids; i++) {
	if (1 != (status = show_named_eid(is, path.eid[i])))
	    return(status);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving path"); 
    return(1);
}


static int 
global_path_core(parse_results, path, inst)
COMLINE_PARSE_RES *parse_results;
EID_LIST *path;
int inst;
{
    int path_len, min_deg, i;
    RESULT_TUPLE *rtup;
    ALT_RESULTS altres;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering global_path_core");
    if (!VALID_INST(inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "global_path_core");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (UNDEF == compare_core(parse_results, &altres, ip->comp_inst))
	return(UNDEF);

    /* Get the list of nodes on the map. compare_core does not do this
     * after processing the density restriction.			*/
    if (UNDEF == build_eidlist(&altres, &ip->nodelist, &ip->max_nodes))
	return(UNDEF);

    /* Count the degree of each node. */
    if (ip->max_nodeinfo < ip->nodelist.num_eids) {
	if (ip->max_nodeinfo) Free(ip->nodes);
	ip->max_nodeinfo += ip->nodelist.num_eids;
	if (NULL == (ip->nodes = Malloc(ip->max_nodeinfo, NODEINFO)))
	    return(UNDEF);
    }
    for (i = 0; i < ip->nodelist.num_eids; i++) { 
	ip->nodes[i].node_id = ip->nodelist.eid[i];
	ip->nodes[i].degree = 0;
    }
    for (rtup = altres.results; rtup < altres.results + altres.num_results;
	 rtup++) {
	for (i = 0; i < ip->nodelist.num_eids; i++) { 
	    if (EID_EQUAL(ip->nodes[i].node_id, rtup->qid))
		ip->nodes[i].degree++;
	    if (EID_EQUAL(ip->nodes[i].node_id, rtup->did))
		ip->nodes[i].degree++;
	}
    }

    /* Initialize path with first node (should have degree >= n/5). */
    path_len = (int) ceil(ip->percent*ip->nodelist.num_eids/100.0);
    if (ip->max_pathlen < path_len) {
	if (ip->max_pathlen) Free(ip->path.eid);
	ip->max_pathlen += path_len;
	if (NULL == (ip->path.eid = Malloc(ip->max_pathlen, EID)))
	    return(UNDEF);
    }
    ip->path.num_eids = path_len;

    min_deg = (long)((20.0*ip->nodelist.num_eids)/100.0);
    for (i = 0;
	 i < ip->nodelist.num_eids && ip->nodes[i].degree < min_deg; i++);
    if (i == ip->nodelist.num_eids) i = 0;
    ip->path.eid[0] = (ip->first_node.id == UNDEF) ? 
	ip->nodes[i].node_id : ip->first_node;

    /* Sort the nodes by bushiness. */
    qsort((char *)ip->nodes, ip->nodelist.num_eids, sizeof(NODEINFO),
	  compare_degree);

    if (BUSHY == ip->ptype && UNDEF == bushy(ip, &altres))
	return(UNDEF);
    if (DFP == ip->ptype && UNDEF == dfp(ip, &altres))
	return(UNDEF);

    *path = ip->path;

    PRINT_TRACE (2, print_string, "Trace: leaving global_path_core");
    return 1;
}


static int 
seg_path_core(parse_results, path, inst)
COMLINE_PARSE_RES *parse_results;
EID_LIST *path;
int inst;
{
    int path_len, eff_path_len, seg_path_len, seg_len, status, i;
    NODEINFO *beg, *end;
    RESULT_TUPLE *rtup;
    ALT_RESULTS altres;
    SEG_RESULTS segres;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering seg_path_core");
    if (!VALID_INST(inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "seg_path_core");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* All links of a node are considered in computing bushiness. Segmentation
     * removes long distance links. So, we need to get the regular map in 
     * addition to the segmentation results. */
    if (UNDEF == compare_core(parse_results, &altres, ip->comp_inst))
	return(UNDEF);
    if (1 != (status = segment_core(parse_results, &segres, ip->seg_inst)))
	return(status);

    /* Count the degree of each node using the regular map. */
    if (ip->max_nodeinfo < segres.num_nodes) {
	if (ip->max_nodeinfo) Free(ip->nodes);
	ip->max_nodeinfo += segres.num_nodes;
	if (NULL == (ip->nodes = Malloc(ip->max_nodeinfo, NODEINFO)))
	    return(UNDEF);
    }
    for (i = 0; i < segres.num_nodes; i++) { 
	ip->nodes[i].node_id = segres.nodes[i].did;
	ip->nodes[i].beg_seg = segres.nodes[i].beg_seg;
	ip->nodes[i].end_seg = segres.nodes[i].end_seg;
	ip->nodes[i].degree = 0;
    }
    for (rtup = altres.results; rtup < altres.results + altres.num_results;
	 rtup++) {
	for (i = 0; i < segres.num_nodes; i++) { 
	    if (EID_EQUAL(ip->nodes[i].node_id, rtup->qid))
		ip->nodes[i].degree++;
	    if (EID_EQUAL(ip->nodes[i].node_id, rtup->did))
		ip->nodes[i].degree++;
	}
    }

    /* Initialize path. */
    path_len = (int) ceil(ip->percent*segres.num_nodes/100.0);
    if (ip->max_pathlen < path_len) {
	if (ip->max_pathlen) Free(ip->path.eid);
	ip->max_pathlen += 2 * path_len;
	if (NULL == (ip->path.eid = Malloc(ip->max_pathlen, EID)))
	    return(UNDEF);
    }

    /* Each segment must be represented by at least 1 paragraph. This leaves
     * the following length to be distributed among segments according to  
     * their lengths.
     */
    eff_path_len = path_len - segres.num_segs; 
    if(eff_path_len < 0) eff_path_len = 0;
    beg = ip->nodes;
    ip->path.num_eids = 0; 
    qsort((char *) segres.seg_altres->results, segres.seg_altres->num_results,
	  sizeof(RESULT_TUPLE), compare_rtups);

    /* Find paths for each segment. */
    while (beg < ip->nodes + segres.num_nodes) {
	/* find the no. of nodes to be taken from this segment */
	assert(beg->beg_seg);
	end = beg+1;
	while (end < ip->nodes + segres.num_nodes && !end->beg_seg)
	    end++;
	seg_len = end - beg;
	seg_path_len = 
	    (int) floor((double)seg_len*eff_path_len/(double)segres.num_nodes
			+ 0.5) + 1;
	seg_path_len = MIN(seg_path_len, seg_len);

	/* sort nodes by bushiness */
	qsort((char *)beg, seg_len, sizeof(NODEINFO), compare_degree);

	/* get path for this segment */
	if (SEG_BUSHY == ip->ptype && 
	    UNDEF == seg_bushy(beg, end, seg_path_len, &ip->path))
	    return(UNDEF);
	if (SEG_DFP == ip->ptype && 
	    UNDEF == seg_dfp(beg, end, seg_path_len, &ip->path, 
			     segres.seg_altres))
	    return(UNDEF);

	beg = end;
    }

    *path = ip->path;

    PRINT_TRACE (2, print_string, "Trace: leaving seg_path_core");
    return 1;
}


/******************************************************************
 * Bushy path: Initial node + n bushy nodes in N
 * Build a path made up of the initial node and the most bushy of 
 * the remaining nodes, to make up a p% summary.
 ******************************************************************/
static int
bushy( ip, altres )
STATIC_INFO *ip;
ALT_RESULTS *altres;
{
    int curr_path_len, i;

    curr_path_len = 1;
    for (i=0; i<ip->nodelist.num_eids && curr_path_len<ip->path.num_eids; i++)
	if (!(EID_EQUAL(ip->nodes[i].node_id, ip->path.eid[0])) &&
	    eid_greater(&ip->nodes[i].node_id, &ip->path.eid[0]))
	    ip->path.eid[curr_path_len++] = ip->nodes[i].node_id;
    ip->path.num_eids = curr_path_len;
    return 1;
}


/*************************************************************************
 * Generate a set of alt_results with all the results for a docid in one
 * place. Setup pointers to the start of the results for each docid.
 * For example: (here '*' can be anything)
 *
 * doc_list[0] -----> 12017.* 12017.*  0.75
 * 		      12017.*  3191.*  0.45
 * doc_list[1] -----> 15128.*  9620.*  0.88 ....
 * 
 * This way, all the edges for a node can be found at one place and finding
 * the next node in a dfp becomes easy. Also, this helps in keeping the
 * chronological sequence within a document.
 *************************************************************************/
static int
build_doclist(altres, temp_altres, doc_list, num_dids, ip)
ALT_RESULTS *altres;
ALT_RESULTS *temp_altres;
RESULT_TUPLE ***doc_list;
long *num_dids;
STATIC_INFO *ip;
{
    RESULT_TUPLE *res_ptr, *tmp_ptr;

    /* Replace each `undirected' edge (result_tuple) by 2 `directed' edges
     * by making 2 copies of the result as in segment_core.		*/
    for (res_ptr = altres->results, tmp_ptr = temp_altres->results;
	 res_ptr < altres->results + altres->num_results;
	 res_ptr++, tmp_ptr++) {
	Bcopy(res_ptr, tmp_ptr++, sizeof(RESULT_TUPLE));
	tmp_ptr->qid = (tmp_ptr-1)->did;
	tmp_ptr->did = (tmp_ptr-1)->qid;
	tmp_ptr->sim = (tmp_ptr-1)->sim;
    }
    temp_altres->num_results = 2 * altres->num_results;

    /* Sort by source of the edge. */
    qsort((char *)temp_altres->results, temp_altres->num_results,
	  sizeof(RESULT_TUPLE), compare_rtups);

    /* Assign pointers. */
    *num_dids = 0;
    (*doc_list)[(*num_dids)++] = temp_altres->results;
    for (tmp_ptr = temp_altres->results + 1;
	 tmp_ptr < temp_altres->results + temp_altres->num_results;
	 tmp_ptr++ )
	if (tmp_ptr->qid.id != (tmp_ptr-1)->qid.id)
	    (*doc_list)[(*num_dids)++] = tmp_ptr;

    return 1;
}


static int
chronological(doc_list, did, num_dids)
RESULT_TUPLE **doc_list;
EID did;
int num_dids;
{
    int i;

    for (i = 0; i < num_dids; i++)
	if (doc_list[i] != NULL && did.id == doc_list[i]->qid.id)
	    break;
    if (i == num_dids || eid_greater(&doc_list[i]->qid, &did))
	return 0;
    return 1;
}


static int
already_present(list, list_length, eid)
EID *list;
long list_length;
EID eid;
{
    int i;

    for (i = 0 ; i < list_length && !EID_EQUAL(list[i], eid); i++);
    if ( i == list_length ) return 0;
    else return 1;
}


/******************************************************************
 * Depth-first path
 * Build a path made up of a depth first path starting at initial node. 
 * Add to it all the depth first path begining at any good node. Stop as 
 * soon as we have p% nodes.
 ******************************************************************/
static int
dfp( ip, altres )
STATIC_INFO *ip;
ALT_RESULTS *altres;
{
    int curr_path_len, num_docs, i;
    RESULT_TUPLE *res_ptr, **doc_list;
    ALT_RESULTS temp_altres;
    NODEINFO *ptr;

    /* Build doclist. */
    if (NULL == (doc_list = Malloc(ip->nodelist.num_eids, RESULT_TUPLE *)) ||
	NULL == (temp_altres.results = Malloc(2*altres->num_results, 
					      RESULT_TUPLE)))
	return UNDEF;
    if(UNDEF == build_doclist(altres, &temp_altres, &doc_list, &num_docs, ip))
      return UNDEF;

    /* Do depth-first traversal. */
    curr_path_len = 1; ptr = ip->nodes;
    while (ptr < ip->nodes + ip->nodelist.num_eids && 
	   curr_path_len < ip->path.num_eids) {
	RESULT_TUPLE **temp_doc_list;
	if (NULL == (temp_doc_list = Malloc(ip->nodelist.num_eids, RESULT_TUPLE *)))
	    return UNDEF;
	Bcopy(doc_list, temp_doc_list, ip->nodelist.num_eids*sizeof(RESULT_TUPLE *));
	while (curr_path_len < ip->path.num_eids) {
	    EID just_inserted;
	    just_inserted = ip->path.eid[curr_path_len - 1];
	    /* find this doc */
	    for ( i = 0; i < num_docs; i++ )
		if (temp_doc_list[i] != NULL &&
		    just_inserted.id == temp_doc_list[i]->qid.id)
		    break;
	    if (i == num_docs) break;

	    /* get forward edges for this node */
	    res_ptr = temp_doc_list[i];
	    while (eid_greater(&just_inserted, &temp_doc_list[i]->qid))
		temp_doc_list[i]++;
	    while (EID_EQUAL(temp_doc_list[i]->qid, just_inserted) &&
		   (already_present(ip->path.eid, curr_path_len,
				   temp_doc_list[i]->did) ||
		   !chronological(temp_doc_list, temp_doc_list[i]->did, 
				  num_docs)))
		temp_doc_list[i]++;
	    if (res_ptr->qid.id != temp_doc_list[i]->qid.id)
		temp_doc_list[i] = NULL;
	    if (temp_doc_list[i] != NULL && 
		EID_EQUAL(temp_doc_list[i]->qid, just_inserted)) {
		/* found an edge */
		ip->path.eid[curr_path_len++] = temp_doc_list[i]->did;
		while (EID_EQUAL(temp_doc_list[i]->qid, just_inserted))
		    temp_doc_list[i]++;
		if (res_ptr->qid.id != temp_doc_list[i]->qid.id)
		    temp_doc_list[i] = NULL;
	    }
	    else break;
	}

	/* get next starting node */
	while (ptr < ip->nodes + ip->nodelist.num_eids &&
	       already_present(ip->path.eid, ip->path.num_eids, ptr->node_id))
	    ptr++;
	if (ptr < ip->nodes + ip->nodelist.num_eids && 
	    curr_path_len < ip->path.num_eids) {
	    ip->path.eid[curr_path_len++] = ptr->node_id;
	}
	Free(temp_doc_list);    
    }
    Free(temp_altres.results);
    Free(doc_list);

    ip->path.num_eids = curr_path_len;
    return 1;
}


/**********************************************************************
 * Bushy path within a segment
 **********************************************************************/
static int
seg_bushy( seg_begin, seg_end, length, path )
NODEINFO *seg_begin;
NODEINFO *seg_end;
int length;
EID_LIST *path;
{
    int i;

    for (i = 0; i < length; i++)
	path->eid[path->num_eids++] = seg_begin[i].node_id;

    return 1;
}


/**********************************************************************
 * Depth-first path within a segment
 **********************************************************************/
static int
seg_dfp( seg_begin, seg_end, length, path, altres )
NODEINFO *seg_begin;
NODEINFO *seg_end;
int length;
EID_LIST *path;
ALT_RESULTS *altres;
{
    int num_so_far;
    RESULT_TUPLE *rtup;

    path->eid[path->num_eids++] = seg_begin->node_id;
    num_so_far = 1;
    /* Do depth-first traversal within this segment. */
    while (seg_begin < seg_end && num_so_far < length) {
	EID just_inserted = path->eid[path->num_eids - 1];
	
	/* get forward edges for this node */
	rtup = altres->results; 
	while (rtup < altres->results + altres->num_results &&
	       !EID_EQUAL(just_inserted, rtup->qid)) rtup++;
	while (EID_EQUAL(just_inserted, rtup->qid) && 
	       already_present(path->eid, path->num_eids, rtup->did))
	    rtup++;
	if (EID_EQUAL(just_inserted, rtup->qid)) { /* found an edge */
	    path->eid[path->num_eids++] = rtup->did;
	    num_so_far++;
	}
	else { /* get next starting node */
	    while (seg_begin < seg_end && 
		   already_present(path->eid, path->num_eids, 
				   seg_begin->node_id))
		seg_begin++;
	    if (seg_begin < seg_end) {
		path->eid[path->num_eids++] = seg_begin->node_id;
		num_so_far++;
	    }
	}
    }

    return 1;
}


static int
compare_degree(n1, n2)
NODEINFO *n1;
NODEINFO *n2;
{
    if (n1->degree < n2->degree)
	return 1;
    if (n1->degree > n2->degree)
	return -1;
    return 0;
}
