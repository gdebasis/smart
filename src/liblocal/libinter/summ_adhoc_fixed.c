#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libinter/summ_adhoc_fixed.c,v 11.2 1993/02/03 16:52:09 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Generate fixed-length query-oriented summaries for list of docs.
 *2 summ_adhoc_fixed (is, unused, inst)
 *3   INTER_STATE *is;
 *3   char *unused;
 *3   int inst;
 *4 init_summ_adhoc_fixed (spec, unused)
 *4 close_summ_adhoc_fixed (inst)
 *7 Note: 2 vecs_vecs instantiations are used. vecs_vecs is called to
 *7 compare the query with each para, and then to compare each sentence
 *7 to the whole document. Having two separate instantiations prevents
 *7 the altres from the two calls over-writing each other. Thus, we don't
 *7 have to save/copy the altres returned by vecs_vecs (this is a pointer
 *7 into vecs_vecs' internal data structures).
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
static PROC_TAB *vecs_vecs;
static SPEC_PARAM spec_args[] = {
    {"inter.summarize.doclist_file", getspec_dbfile, (char *) &doclist_file},
    {"inter.summarize.summary_dir", getspec_dbfile, (char *) &summary_dir},
    {"inter.summarize.default_percent", getspec_double, (char *)&percent},
    {"inter.summarize.vecs_vecs", getspec_func, (char *) &vecs_vecs},    
    TRACE_PARAM ("local.inter.path.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

typedef struct {
    int valid_info;	/* bookkeeping */
    int dvec_inst, qvec_inst, eid_inst, vv_para_inst, vv_sent_inst, show_inst; 

    int max_paras;
    VEC *para_vecs;
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

static int postproc_path(), compare_rtup_sim();


int
init_summ_adhoc_fixed (spec, unused)
SPEC *spec;
char *unused;
{
    int new_inst;
    STATIC_INFO *ip;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_summ_adhoc_fixed");
    NEW_INST( new_inst );
    if (UNDEF == new_inst)
        return UNDEF;
    ip = &info[new_inst];
    ip->valid_info = 1;

    if (UNDEF==(ip->dvec_inst = init_did_vec(spec, NULL)) ||
	UNDEF==(ip->qvec_inst = init_qid_vec(spec, NULL)) ||
	UNDEF==(ip->eid_inst = init_eid_util(spec, NULL)) ||
	UNDEF==(ip->vv_para_inst = vecs_vecs->init_proc(spec, NULL)) ||
	UNDEF==(ip->vv_sent_inst = vecs_vecs->init_proc(spec, NULL)) ||
	UNDEF==(ip->show_inst = init_show_doc_part(spec, NULL)))
	return(UNDEF);

    ip->max_paras = 0;
    ip->max_sent_vecs = 0;
    ip->max_sents = 64;
    if (NULL == (ip->sent_res.results = Malloc(ip->max_sents, RESULT_TUPLE)))
	return(UNDEF);
    ip->max_text_bufs = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving init_summ_adhoc_fixed");
    return new_inst;
}


int
close_summ_adhoc_fixed (inst)
int inst;
{
    int i;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_summ_adhoc_fixed");
    if (!VALID_INST(inst)) {
        set_error( SM_ILLPA_ERR, "Instantiation", "close_summ_adhoc_fixed");
        return UNDEF;
    }
    ip = &info[inst];

    if (UNDEF == close_did_vec(ip->dvec_inst) ||
	UNDEF == close_qid_vec(ip->qvec_inst) ||
	UNDEF == close_eid_util(ip->eid_inst) ||
	UNDEF == vecs_vecs->close_proc(ip->vv_para_inst) ||
	UNDEF == vecs_vecs->close_proc(ip->vv_sent_inst) ||
	UNDEF == close_show_doc_part(ip->show_inst))
	return(UNDEF);

    if (ip->max_paras) Free(ip->para_vecs);
    if (ip->max_sent_vecs) Free(ip->sent_vecs);
    if (ip->max_sents) Free(ip->sent_res.results);
    if (ip->max_text_bufs) {
	for (i = 0; i < ip->max_text_bufs; i++)
	    if (ip->text_bufs[i].size)
		Free(ip->text_bufs[i].buf);
	Free(ip->text_bufs);
    }

    ip->valid_info = 0;
    PRINT_TRACE (2, print_string, "Trace: leaving close_summ_adhoc_fixed");
    return (0);
}


int
summ_adhoc_fixed (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    char char_buf[PATH_LEN], *slist[1] = {&char_buf[0]};
    int i;
    int uid = getuid();
    long qid, docid, num_paras;
    double saved_percent;
    FILE *fp, *out_fp;
    STRING_LIST strlist = {1, &slist[0]};
    EID_LIST elist;
    VEC qvec;
    VEC_LIST qv_list, plist;
    VEC_LIST_PAIR vlpair;
    ALT_RESULTS para_altres;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering summ_adhoc_fixed");
    if (!VALID_INST(inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "summ_adhoc_fixed");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (is->num_command_line < 2) {
	if (UNDEF == add_buf_string ("No query specified\n", &is->err_buf))
            return (UNDEF);
        return (0);
    }
    /* Get query vector */
    qid = atol(is->command_line[1]);
    if (UNDEF == qid_vec(&qid, &qvec, ip->qvec_inst))
	return(UNDEF);

    if (is->num_command_line == 3) {
      saved_percent = percent;
      percent = atof(is->command_line[2]);
      sprintf(char_buf, "Getting %3.1f summaries\n", percent);
      if (UNDEF == add_buf_string (char_buf, &is->err_buf))
            return (UNDEF);
    }
    if (NULL == (fp = fopen(doclist_file, "r")))
	return(UNDEF);
    /* Process each document in turn */
    while (fscanf(fp, "%ld %d\n", &docid, &ip->document_len) != EOF) {
	sprintf(char_buf, "%s/%d.%ld", summary_dir, uid, docid);
        if (NULL == (out_fp = fopen(char_buf, "w"))) {
	    set_error (SM_ILLPA_ERR, "Summary file", "summ_adhoc_fixed");
	    return (UNDEF);
	}

	/* Get para vectors */
	sprintf(char_buf, "%ld.p", docid);
	if (UNDEF == stringlist_to_eidlist(&strlist, &elist, ip->eid_inst))
	    return(UNDEF);
	if (elist.num_eids > ip->max_paras) {
	    if (ip->max_paras) Free(ip->para_vecs);
	    ip->max_paras += 2 * elist.num_eids;
	    if (NULL == (ip->para_vecs = Malloc(ip->max_paras, VEC)))
		return(UNDEF);
	}
	for (i = 0; i < elist.num_eids; i++) {
	    if (UNDEF == eid_vec(elist.eid[i], &(ip->para_vecs[i]), ip->eid_inst) ||
		UNDEF == save_vec(&ip->para_vecs[i]))
		return(UNDEF);
	}

	/* Compare query to para vectors */
	qv_list.num_vec = 1; qv_list.vec = &qvec;
	plist.num_vec = elist.num_eids; plist.vec = ip->para_vecs;
	vlpair.vec_list1 = &qv_list; vlpair.vec_list2 = &plist;
	if (UNDEF == vecs_vecs->proc(&vlpair, &para_altres, ip->vv_para_inst))
	    return(UNDEF);

	for (i = 0; i < elist.num_eids; i++) 
	    free_vec(&ip->para_vecs[i]);

	qsort(para_altres.results, para_altres.num_results, 
	      sizeof(RESULT_TUPLE), compare_rtup_sim);
	num_paras = (long) floor((double)(elist.num_eids) * percent/100.0 + 0.5);
	if (para_altres.num_results > num_paras)
	    para_altres.num_results = num_paras;
	qsort(para_altres.results, para_altres.num_results, 
	      sizeof(RESULT_TUPLE), compare_rtups);

	/* Postprocess paths to select sentences.
	 * Texts for selected sentences are stores in ip->text_bufs.
	 * Sentences are ordered by "goodness" in ip->sent_res.
	 */
	if (UNDEF == postproc_path(&para_altres, ip, is))
	    return(UNDEF);

	/* Print out selected sentences */
	is->output_buf.end = 0;
	qsort(ip->sent_res.results, ip->sent_res.num_results, 
	      sizeof(RESULT_TUPLE), compare_rtups);
	for (i = 0; i < ip->sent_res.num_results; i++) {
	    int buf_index = ip->sent_res.results[i].did.id;
	    if (UNDEF != buf_index &&
		UNDEF == add_buf(&(ip->text_bufs[buf_index]), &is->output_buf))
		return(UNDEF);
	}
	if (is->output_buf.end != fwrite(is->output_buf.buf, 1, 
					 is->output_buf.end, out_fp))
	    return(UNDEF);
	is->output_buf.end = 0;
	fclose(out_fp);
    }

    fclose(fp);
    PRINT_TRACE (2, print_string, "Trace: leaving summ_adhoc_fixed"); 
    return(1);
}


static int 
postproc_path(para_altres, ip, is)
ALT_RESULTS *para_altres;
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
    docid = para_altres->results[0].did.id;
    if (UNDEF == did_vec(&docid, &dvec, ip->dvec_inst))
	return(UNDEF);

    ip->sent_res.num_results = 0;
    /* For each para */
    for (i = 0; i < para_altres->num_results; i++) { 
	if (para_altres->results[i].did.ext.type != P_PARA) {
	    set_error(SM_ILLPA_ERR, "Expected para summary", "postproc_path");
	    return 0;
	}
	/* Get sentence vectors */
	assert(docid == para_altres->results[i].did.id);
	sprintf(did_buf, "%ld.p%d.s", docid, para_altres->results[i].did.ext.num);
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
	if (UNDEF == vecs_vecs->proc(&pair, &curr_sent, ip->vv_sent_inst))
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
	ip->text_bufs[i].end = 0;
	if (summary_size + is->output_buf.end <= ip->document_len/FINAL_FRAC) {
	    summary_size += is->output_buf.end;
	    if (UNDEF == add_buf(&is->output_buf, &ip->text_bufs[i]))
		return(UNDEF);
	    ip->sent_res.results[i].did.id = i;
	}
	else ip->sent_res.results[i].did.id = UNDEF;
    }

    return 1;
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


