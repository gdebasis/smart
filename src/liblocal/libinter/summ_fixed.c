#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libinter/summ_fixed.c,v 11.2 1993/02/03 16:52:09 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Generate fixed-length generic summaries for list of docs.
 *2 summ_fixed (is, unused, inst)
 *3   INTER_STATE *is;
 *3   char *unused;
 *3   int inst;
 *4 init_summ_fixed (spec, unused)
 *4 close_summ_fixed (inst)
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

#define FINAL_FRAC 10

static char *doclist_file, *summary_dir;
static double percent;
static long min_num_paras;
static PROC_TAB *vecs_vecs;
static SPEC_PARAM spec_args[] = {
    {"inter.summarize.doclist_file", getspec_dbfile, (char *) &doclist_file},
    {"inter.summarize.summary_dir", getspec_dbfile, (char *) &summary_dir},
    {"inter.summarize.default_percent", getspec_double, (char *)&percent},
    {"inter.summarize.min_num_paras", getspec_long, (char *)&min_num_paras},
    {"inter.summarize.vecs_vecs", getspec_func, (char *) &vecs_vecs},    
    TRACE_PARAM ("local.inter.path.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

typedef struct {
    EID node_id;
    int degree;
    double sim_sum;
    char beg_seg, end_seg;
} NODEINFO;

typedef struct {
    int valid_info;	/* bookkeeping */

    int lcom_parse_inst, eid_inst, comp_inst, seg_inst, show_inst, dvec_inst,
	vv_inst;
    
    int max_pathlen;
    EID_LIST path;

    int max_nodeinfo, num_nodes;
    NODEINFO *nodes;

    int max_sent_vecs;
    VEC *sent_vecs;
    int max_sents;
    ALT_RESULTS sent_res;

    int document_len;
    int max_text_bufs;
    SM_BUF *text_bufs;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int seg_path_core(), postproc_path(), compare_degree(), compare_rtup_sim();


int
init_summ_fixed (spec, unused)
SPEC *spec;
char *unused;
{
    int new_inst;
    STATIC_INFO *ip;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_summ_fixed");
    NEW_INST( new_inst );
    if (UNDEF == new_inst)
        return UNDEF;
    ip = &info[new_inst];
    ip->valid_info = 1;

    if (UNDEF==(ip->lcom_parse_inst = init_local_comline_parse(spec, NULL)) ||
	UNDEF==(ip->eid_inst = init_eid_util(spec, NULL)) ||
	UNDEF==(ip->comp_inst = init_compare_core(spec, "local.inter.compare.")) ||
	UNDEF==(ip->seg_inst = init_segment_core(spec, NULL)) ||
	UNDEF==(ip->show_inst = init_show_doc_part(spec, NULL)) ||
	UNDEF==(ip->dvec_inst = init_did_vec(spec, NULL)) ||
	UNDEF==(ip->vv_inst = vecs_vecs->init_proc(spec, NULL)))
	return(UNDEF);

    ip->max_pathlen = 0;
    ip->max_nodeinfo = 0;
    ip->max_sent_vecs = 0;
    ip->max_sents = 64;
    if (NULL == (ip->sent_res.results = Malloc(ip->max_sents, RESULT_TUPLE)))
	return(UNDEF);
    ip->max_text_bufs = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving init_summ_fixed");
    return new_inst;
}


int
close_summ_fixed (inst)
int inst;
{
    int i;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_summ_fixed");
    if (!VALID_INST(inst)) {
        set_error( SM_ILLPA_ERR, "Instantiation", "close_summ_fixed");
        return UNDEF;
    }
    ip = &info[inst];

    if (UNDEF == close_local_comline_parse(ip->lcom_parse_inst) ||
	UNDEF == close_eid_util(ip->eid_inst) ||
	UNDEF == close_compare_core(ip->comp_inst) ||
	UNDEF == close_segment_core(ip->seg_inst) ||
	UNDEF == close_show_doc_part(ip->show_inst) ||
	UNDEF == close_did_vec(ip->dvec_inst) ||
	UNDEF == vecs_vecs->close_proc(ip->vv_inst))
	return(UNDEF);

    if (ip->max_pathlen) Free(ip->path.eid);
    if (ip->max_nodeinfo) Free(ip->nodes);
    if (ip->max_sent_vecs) Free(ip->sent_vecs);
    if (ip->max_sents) Free(ip->sent_res.results);
    if (ip->max_text_bufs) {
	for (i = 0; i < ip->max_text_bufs; i++)
	    if (ip->text_bufs[i].size)
		Free(ip->text_bufs[i].buf);
	Free(ip->text_bufs);
    }

    ip->valid_info = 0;
    PRINT_TRACE (2, print_string, "Trace: leaving close_summ_fixed");
    return (0);
}


int
summ_fixed (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    char char_buf[PATH_LEN];
    int status, i;
    int uid = getuid();
    long docid;
    FILE *fp, *out_fp;
    COMLINE_PARSE_RES parse_results;
    EID_LIST path;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering summ_fixed");
    if (!VALID_INST(inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "summ_fixed");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (NULL == (fp = fopen(doclist_file, "r")))
	return(UNDEF);
    /* Process each document in turn */
    while (fscanf(fp, "%ld %d\n", &docid, &ip->document_len) != EOF) {
	sprintf(char_buf, "%s/%d.%ld", summary_dir, uid, docid);
        if (NULL == (out_fp = fopen(char_buf, "w"))) {
	    set_error (0, "Summary file", "seg_path_core");
	    return (UNDEF);
	}

	/* This could possibly be done more cleanly. For now, we simply
	 * simulate user input for each document by mucking aroung with 
	 * is->command_line and is->num_command_line.
	 */
	is->num_command_line = 2;
	sprintf(char_buf, "%ld.p", docid);
	is->command_line[1] = &char_buf[0];
	if (1 != (status = local_comline_parse(is, &parse_results, 
					       ip->lcom_parse_inst)))
	    return(status);

	/* Generate segmented bushy paths */
	if (UNDEF == seg_path_core(&parse_results, &path, inst)) {
	    if (UNDEF == add_buf_string("Couldn't generate reading path\n",
					&is->err_buf))
		return UNDEF;
	    continue;
	}

	/* Postprocess paths to select sentences.
	 * Texts for selected sentences are stores in ip->text_bufs.
	 * Sentences are ordered by "goodness" in ip->sent_res.
	 */
	if (UNDEF == postproc_path(&path, ip, is))
	    return(UNDEF);

	/* Print out selected sentences */
	is->output_buf.end = 0;
	qsort(ip->sent_res.results, ip->sent_res.num_results, 
	      sizeof(RESULT_TUPLE), compare_rtups);
	for (i = 0; i < ip->sent_res.num_results; i++) {
	    int buf_index = ip->sent_res.results[i].did.id;
	    if (UNDEF == add_buf(&ip->text_bufs[buf_index], &is->output_buf))
		return(UNDEF);
	}
	if (is->output_buf.end != fwrite(is->output_buf.buf, 1, 
					 is->output_buf.end, out_fp))
	    return(UNDEF);
	is->output_buf.end = 0;
	fclose(out_fp);
    }

    fclose(fp);
    PRINT_TRACE (2, print_string, "Trace: leaving summ_fixed"); 
    return(1);
}


static int 
seg_path_core(parse_results, path, inst)
COMLINE_PARSE_RES *parse_results;
EID_LIST *path;
int inst;
{
    int path_len, eff_path_len, seg_path_len, seg_len, status, i, j;
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
	ip->nodes[i].sim_sum = 0.0;
    }
    ip->num_nodes = segres.num_nodes;

    for (rtup = altres.results; rtup < altres.results + altres.num_results;
	 rtup++) {
	for (i = 0; i < segres.num_nodes; i++) { 
	    if (EID_EQUAL(ip->nodes[i].node_id, rtup->qid)) {
		ip->nodes[i].degree++;
		ip->nodes[i].sim_sum += rtup->sim;
	    }
	    if (EID_EQUAL(ip->nodes[i].node_id, rtup->did)) {
		ip->nodes[i].degree++;
		ip->nodes[i].sim_sum += rtup->sim;
	    }
	}
    }

    /* Initialize path. */
    path_len = (int) ceil(percent * segres.num_nodes/100.0);
    path_len = MAX(path_len, min_num_paras);
    path_len = MIN(path_len, segres.num_nodes);
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
	for (j = 0; j < seg_path_len; j++)
	    ip->path.eid[ip->path.num_eids++] = beg[j].node_id;
	beg = end;
    }

    *path = ip->path;

    PRINT_TRACE (2, print_string, "Trace: leaving seg_path_core");
    return 1;
}



static int 
postproc_path(path, ip, is)
EID_LIST *path;
STATIC_INFO *ip;
INTER_STATE *is;
{
    char did_buf[PATH_LEN], *slist[1] = {&did_buf[0]};
    int summary_size, status, i, j;
    long docid;
    STRING_LIST strlist = {1, &slist[0]};
    EID_LIST sent_list;
    VEC dvec;
    VEC_LIST svec, ovec;
    VEC_LIST_PAIR pair;
    ALT_RESULTS curr_sent;

    /* Get vector for the entire document */
    docid = path->eid[0].id;
    if (UNDEF == did_vec(&docid, &dvec, ip->dvec_inst))
	return(UNDEF);

    ip->sent_res.num_results = 0;
    /* For each para */
    for (i = 0; i < path->num_eids; i++) {
	if (path->eid[i].ext.type != P_PARA) {
	    set_error(SM_ILLPA_ERR, "Expected para summary", "postproc_path");
	    return 0;
	}
	/* Get sentence vectors */
	assert(docid == path->eid[i].id);
	sprintf(did_buf, "%ld.p%d.s", docid, path->eid[i].ext.num);
	if (UNDEF == stringlist_to_eidlist(&strlist, &sent_list, ip->eid_inst))
	    return(UNDEF);
	if (sent_list.num_eids > ip->max_sent_vecs) {
            if (ip->max_sent_vecs) Free(ip->sent_vecs);
            ip->max_sent_vecs += 2 * sent_list.num_eids;
            if (NULL == (ip->sent_vecs = Malloc(ip->max_sent_vecs, VEC)))
                return(UNDEF);
        }
	for (j = 0; j < sent_list.num_eids; j++)
	    if (UNDEF == eid_vec(sent_list.eid[j], &(ip->sent_vecs[j]), 
				 ip->eid_inst) ||
		UNDEF == save_vec(&(ip->sent_vecs[j])))
		return(UNDEF);

	/* Compute similarity of each sentence to every other para */
	svec.num_vec = sent_list.num_eids; svec.vec = ip->sent_vecs;
	ovec.num_vec = 1; ovec.vec = &dvec;
	pair.vec_list1 = &svec; pair.vec_list2 = &ovec; 
	if (UNDEF == vecs_vecs->proc(&pair, &curr_sent, ip->vv_inst))
	    return UNDEF;

	for (j = 0; j < sent_list.num_eids; j++)
	    free_vec(&(ip->sent_vecs[j]));

	/* Accumulate results */
	if (ip->sent_res.num_results + curr_sent.num_results > ip->max_sents) {
	    ip->max_sents += 2 * sent_list.num_eids;
	    if (NULL == 
		(ip->sent_res.results = Realloc(ip->sent_res.results, 
						ip->max_sents, RESULT_TUPLE)))
		return(UNDEF);
	}
	Bcopy(curr_sent.results, 
	      &(ip->sent_res.results[ip->sent_res.num_results]),
	      curr_sent.num_results * sizeof(RESULT_TUPLE));	
	ip->sent_res.num_results += curr_sent.num_results;
    }

    qsort(ip->sent_res.results, ip->sent_res.num_results, sizeof(RESULT_TUPLE),
	  compare_rtup_sim);

    /* Get and save text for selected sentences */
    if (ip->sent_res.num_results > ip->max_text_bufs) {
	if (ip->max_text_bufs) {
	    for (i = 0; i < ip->max_text_bufs; i++)
		if (ip->text_bufs[i].size)
		    Free(ip->text_bufs[i].buf);
	    Free(ip->text_bufs);
	}
	ip->max_text_bufs += ip->sent_res.num_results;
	if (NULL == (ip->text_bufs = Malloc(ip->max_text_bufs, SM_BUF)))
	    return(UNDEF);
	bzero(ip->text_bufs, sizeof(SM_BUF)*ip->max_text_bufs);
    }
    for (summary_size = i = 0; i < ip->sent_res.num_results; i++) {
	is->output_buf.end = 0;
	if (1 != (status = show_named_eid(is, ip->sent_res.results[i].qid)))
	    return(status);
	/* HACK: save text_bufs index in did, so we can find them easily
	 * after sorting the rtups back in text order */
	ip->sent_res.results[i].did.id = i;
	ip->text_bufs[i].end = 0;
	if (summary_size + is->output_buf.end <= ip->document_len/FINAL_FRAC) {
	    summary_size += is->output_buf.end;
	    if (UNDEF == add_buf(&is->output_buf, &ip->text_bufs[i]))
		return(UNDEF);
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

     if (n1->sim_sum < n2->sim_sum)
       return 1;
     if (n1->sim_sum > n2->sim_sum)
       return -1;

    return 0;
}

static int
compare_rtup_sim( r1, r2 )
RESULT_TUPLE *r1;
RESULT_TUPLE *r2;
{
    if (r1->sim < r2->sim)	/* descending order */
	return 1;
    if (r1->sim > r2->sim)
	return -1;
    return 0;
}


