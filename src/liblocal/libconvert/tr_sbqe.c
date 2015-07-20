#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/tr_sbqe.c,v 11.2 1993/02/03 16:51:11 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "docindex.h"
#include "vector.h"
#include "trace.h"
#include "context.h"
#include "inst.h"
#include "docdesc.h"
#include "buf.h"
#include "local_eid.h"

static int compare_rtup_sim(RESULT_TUPLE *r1, RESULT_TUPLE *r2);
static int compare_vec(VEC *, VEC *);

static PROC_TAB *index_pp, *vecs_vecs;
static long num_ctypes;
static long num_conwt_threshold;
static int print_vec_inst;

static SPEC_PARAM spec_args[] = {
    PARAM_FUNC("tr_sbqe.index_pp", &index_pp),
    PARAM_FUNC("tr_sbqe.vecs_vecs", &vecs_vecs),
    {"tr_sbqe.num_ctypes",      getspec_long, (char *) &num_ctypes},
    {"tr_sbqe.num_conwt_threshold",      getspec_long, (char *) &num_conwt_threshold},
    TRACE_PARAM ("convert.tr_sbqe.trace")
};
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    PROC_TAB *index_pp;	
	int textdoc_inst;
	int index_pp_inst;
	int vv_inst;

	long *ctype_len_buff;
	int  buff_size;

} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_tr_sbqe (spec, prefix)
SPEC *spec;
char *prefix;
{
    int new_inst;
    STATIC_INFO *ip;
    CONTEXT old_context;
	char new_prefix[PATH_LEN];

    old_context = get_context();
    set_context (CTXT_DOC);

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_tr_sbqe");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];
    ip->index_pp = index_pp;

    if (UNDEF == (ip->textdoc_inst = init_get_textdoc(spec, NULL)))
		return UNDEF;

    ip->valid_info = 1;

    add_context (CTXT_SENT);
    sprintf(new_prefix, "%ssent.", (prefix == NULL) ? "" : prefix);

    if( UNDEF == (ip->index_pp_inst = ip->index_pp->init_proc(spec, new_prefix)) ||
    	UNDEF == (ip->vv_inst = vecs_vecs->init_proc(spec, NULL)))			
		return UNDEF;

    del_context (CTXT_SENT);
    set_context (old_context);

    if (sm_trace >= 4 &&
        UNDEF == (print_vec_inst = init_print_vec_dict(spec, NULL)))
        return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving init_tr_sbqe");
    return (new_inst);
}

int
tr_sbqe (eid, qvec, expqvec, m, inst)
EID eid;
VEC *qvec;
VEC *expqvec;
int m;			/* Number of sentences to add */
int inst;
{
    int sent_count, i, j = 0, k = 0, s; 
    SM_INDEX_TEXTDOC pp_vec, pp_svec;
	VEC_LIST sveclist, qveclist;	// list of sentence vectors for this document
	VEC *sent_vecs;
	VEC *svec_ptr;
	VEC_LIST_PAIR pair;
	VEC merged_vec, prev_merged_vec;
	ALT_RESULTS sent_qry_sims;
	VEC_PAIR vec_pair;
	long ctype;
	static char msg[1024];

    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering tr_sbqe");
	merged_vec.id_num.id = UNDEF;
	prev_merged_vec.id_num.id = UNDEF;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "tr_sbqe");
        return (UNDEF);
    }
    ip  = &info[inst];

	eid.ext.type = P_SENT;  // We intend to decompose the document into sentences

    if (UNDEF == get_textdoc(eid.id, &pp_vec, ip->textdoc_inst))
		return(UNDEF);
    pp_vec.mem_doc.id_num = eid;

	sent_count = 0;
	for (i = 0; i < pp_vec.mem_doc.num_sections; i++) {
	    for (j = 0; j < pp_vec.mem_doc.sections[i].num_subsects; j++) {
			sent_count += pp_vec.mem_doc.sections[i].subsect[j].num_subsects;
		}
	}

    if (NULL == (sent_vecs = Malloc(sent_count, VEC)))
	    return(UNDEF);

	Bcopy(&pp_vec, &pp_svec, sizeof(SM_INDEX_TEXTDOC));

	sent_count = 0;
	for (i = 0; i < pp_vec.mem_doc.num_sections; i++) {
	    for (j = 0; j < pp_vec.mem_doc.sections[i].num_subsects; j++) {
	        pp_svec.mem_doc.sections = &(pp_vec.mem_doc.sections[i].subsect[j]);
			// The sentences are obtained under one more level. 
			if (pp_vec.mem_doc.sections[i].subsect[j].num_subsects <= 1)
				continue;
			for (s = 0; s < pp_vec.mem_doc.sections[i].subsect[j].num_subsects; s++) {
	        	pp_svec.mem_doc.sections = &(pp_vec.mem_doc.sections[i].subsect[j].subsect[s]);
				pp_svec.mem_doc.num_sections = 1;
    			if (UNDEF == ip->index_pp->proc(&pp_svec, &sent_vecs[k], ip->index_pp_inst))
	    			return UNDEF;
				// Efficient relocation of this vector after sorting by the similarity values
				if (UNDEF == save_vec(&sent_vecs[k]))
					return(UNDEF);

				snprintf(msg, sizeof(msg), "sentence[%d]:", k);
		    	PRINT_TRACE (4, print_string, msg);
				strncpy(msg, &pp_svec.doc_text[pp_svec.mem_doc.sections[0].begin_section],
						MIN(sizeof(msg)-1,
						pp_svec.mem_doc.sections[0].end_section - pp_svec.mem_doc.sections[0].begin_section));
		    	PRINT_TRACE (4, print_string, msg);

				snprintf(msg, sizeof(msg), "sentence vector[%d]:", k);
	    		PRINT_TRACE (4, print_string, msg);
        		if (sm_trace >= 4) {
	        		if (UNDEF == print_vec_dict(&sent_vecs[k], NULL, print_vec_inst))
	            		return UNDEF;
				}

				// Skip very long sentences.... (might happen when the document is not in sentential form
				if (sent_vecs[k].num_conwt > num_conwt_threshold)
					sent_vecs[k].num_conwt = 0;
				k++;
				sent_count++;
			}
		}
	}

	// Filter out the null (and too long) vectors from sent_vecs array
    qsort(sent_vecs, sent_count, sizeof(VEC), compare_vec);
	for (i = 0; i < sent_count; i++) {
		if (sent_vecs[i].num_conwt == 0)
			break;
		sent_vecs[i].id_num.id = i;
		snprintf(msg, sizeof(msg), "sentence vector[%d]: ", i);
	    PRINT_TRACE (4, print_string, msg);
      	if (sm_trace >= 4) {
	   		if (UNDEF == print_vec_dict(&sent_vecs[i], NULL, print_vec_inst))
	   			return UNDEF;
		}
	}
	sent_count = i;

	/* Compute similarity of each sentence to every other para */
	sveclist.num_vec = sent_count; sveclist.vec = sent_vecs;
	qveclist.num_vec = 1; qveclist.vec = qvec;
	pair.vec_list1 = &qveclist;
	pair.vec_list2 = &sveclist;
	if (UNDEF == vecs_vecs->proc(&pair, &sent_qry_sims, ip->vv_inst))
		return UNDEF;

	if (sent_qry_sims.num_results == 0) {
		copy_vec(expqvec, qvec);
		return 1;
	}

    qsort(sent_qry_sims.results, sent_qry_sims.num_results, sizeof(RESULT_TUPLE), compare_rtup_sim);

	// Merge all the sentence vectors
	if (sent_qry_sims.num_results == 1) { // Only one vec with nonzero sim

		snprintf(msg, sizeof(msg), "Merging 1 sentence from document %d...", eid.id);
    	PRINT_TRACE (4, print_string, msg);

		merge_vec(qvec, &sent_vecs[sent_qry_sims.results[0].did.id], expqvec);

	    PRINT_TRACE (4, print_string, "expanded query vec:");
        if (sm_trace >= 4) {
	        if (UNDEF == print_vec_dict(expqvec, NULL, print_vec_inst))
	            return UNDEF;
		}

	    PRINT_TRACE (2, print_string, "Trace: leaving tr_sbqe");
    	return (1);
	}
	merge_vec(&sent_vecs[sent_qry_sims.results[0].did.id], &sent_vecs[sent_qry_sims.results[1].did.id], &prev_merged_vec);

	snprintf(msg, sizeof(msg), "Merging %d sentences from document %d...", MIN(sent_qry_sims.num_results, m), eid.id);
    PRINT_TRACE (4, print_string, msg);

	for (i = 2; i < MIN(sent_qry_sims.num_results, m); i++) {
		if (sent_qry_sims.results[i].sim > 0) {
			merge_vec(&prev_merged_vec, &sent_vecs[sent_qry_sims.results[2].did.id], &merged_vec);

			// Save the intermediate merged vec
			free_vec(&prev_merged_vec);
			copy_vec(&prev_merged_vec, &merged_vec);  // prev_merged_vec <- merged_vec
		}
	}

    for (j = 0; j < k; j++)
		free_vec(&(sent_vecs[j]));
	Free(sent_vecs);

	if (merged_vec.id_num.id == UNDEF) {
		merge_vec(qvec, &prev_merged_vec, expqvec);
		free_vec(&prev_merged_vec);	
	}
	else {
		// Merge the merged sentence vectors with the query vector
		merge_vec(qvec, &merged_vec, expqvec);
		free_vec(&merged_vec);
		free_vec(&prev_merged_vec);	
	}

    PRINT_TRACE (4, print_string, "expanded query vec:");
    if (sm_trace >= 4) {
	    if (UNDEF == print_vec_dict(expqvec, NULL, print_vec_inst))
		    return UNDEF;
	}

    PRINT_TRACE (2, print_string, "Trace: leaving tr_sbqe");
    return (1);
}

static int
compare_rtup_sim( r1, r2 )
		RESULT_TUPLE *r1;
		RESULT_TUPLE *r2;
{
	if (r1->sim < r2->sim)  /* descending order */
		return 1;
	if (r1->sim > r2->sim)
	    return -1;
	return 0;
}

static int
compare_vec( v1, v2 )
		VEC *v1;
		VEC *v2;
{
	if (v1->num_conwt < v2->num_conwt)  /* descending order */
		return 1;
	if (v1->num_conwt > v2->num_conwt)
	    return -1;
	return 0;
}
int
close_tr_sbqe (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_tr_sbqe");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "skel");
        return (UNDEF);
    }
    ip  = &info[inst];
    ip->valid_info--;

    if (ip->valid_info == 0) {
	    if (UNDEF == ip->index_pp->close_proc(ip->index_pp_inst))
    	    return UNDEF;			
	    if (UNDEF == close_get_textdoc(ip->textdoc_inst))
    		return UNDEF;
    	if (UNDEF == vecs_vecs->close_proc(ip->vv_inst))
			return UNDEF;
    	if (sm_trace >= 4 &&
        	UNDEF == close_print_vec_dict(print_vec_inst))
	        return UNDEF;
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_tr_sbqe");
    return (0);
}



