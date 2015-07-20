#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/tr_sbqr_incremental_o.c,v 11.0 1992/07/21 18:20:09 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.

   The difference with the non-incremental version is that we work on each document
   at a time. We remove an abolute number of sentences instead of a fraction and
   decrease this number as we go down the ranked list i.e. we remove the most from
   evidences from the top document and linearly decrease this number.

   Implement the Sentence Based Query Reduction (SBQR).
   This is useful for queries which are too long like the patents.
   Input:  init retr. tr_file, query vector
   Output: a vector file representing the reduced query
   Outline: Read the top pseudo-relevant documents from the tr file, open each document text,
            get the sentences or pseudo-sentences (if fixed length word window mode is enabled),
			compute the similarities of each (pseudo)sentence with the query vector with the
			desired similarity and add the sentence to the query.

*/

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "retrieve.h"
#include "vector.h"
#include "docindex.h"
#include "inst.h"

static char *query_file;
static long query_file_mode;
static long tr_mode;
static long reduced_query_file_mode;
static char* doc_file ;
static long doc_mode;
static long max_del;
static PROC_TAB *index_pp, *vecs_vecs;
static int print_vec_inst;
static char msg[1024];

static SPEC_PARAM spec_args[] = {
    {"convert.tr_sbqr_incremental.query_file", getspec_dbfile, (char *) &query_file},
    {"convert.tr_sbqr_incremental.tr_file.rmode", getspec_filemode, (char *) &tr_mode},
    {"convert.tr_sbqr_incremental.query_file.rmode", getspec_filemode, (char *) &query_file_mode},
    {"convert.tr_sbqr_incremental.reduced_query_file.rwcmode", getspec_filemode, (char *) &reduced_query_file_mode},
    {"convert.tr_sbqr_incremental.doc_file",     getspec_dbfile,    (char *) &doc_file},
    {"convert.tr_sbqr_incremental.doc_file.rmode", getspec_filemode,(char *) &doc_mode},
    {"convert.tr_sbqr_incremental.max_sentences_to_remove", getspec_long,(char *) &max_del},
    PARAM_FUNC("convert.tr_sbqr_incremental.index_pp", &index_pp),
    PARAM_FUNC("convert.tr_sbqr_incremental.vecs_vecs", &vecs_vecs),
    TRACE_PARAM ("convert.tr_sbqr_incremental.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
	int doc_fd;
	int qvec_fd; 
	int index_pp_inst;
	int vv_inst;
	int textdoc_inst;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int init_tr_sbqr_incr_obj(SPEC* spec, char* prefix)
{
    int new_inst;
    STATIC_INFO *ip;
    CONTEXT old_context;
	char new_prefix[PATH_LEN];

    PRINT_TRACE (2, print_string, "Trace: entering init_tr_sbqr_incr_obj");
    
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];
    if (!VALID_FILE (query_file) || !VALID_FILE (doc_file)) {
		return UNDEF;
	}

    if (UNDEF == (ip->textdoc_inst = init_get_textqry(spec, NULL)))
		return UNDEF;

    /* Open document file */
   	if (UNDEF == (ip->doc_fd = open_vector (doc_file, doc_mode)))
       	return (UNDEF);
	// Open the output expanded query vector for writing out the expanded query
    if (UNDEF == (ip->qvec_fd = open_vector (query_file, query_file_mode)))
        return (UNDEF);

    old_context = get_context();
    add_context (CTXT_SENT);
    sprintf(new_prefix, "%ssent.", (prefix == NULL) ? "" : prefix);
    if( UNDEF == (ip->index_pp_inst = index_pp->init_proc(spec, new_prefix)) ||
    	UNDEF == (ip->vv_inst = vecs_vecs->init_proc(spec, NULL)))			
		return UNDEF;

    del_context (CTXT_SENT);
    set_context (old_context);

    if (sm_trace >= 8 &&
        UNDEF == (print_vec_inst = init_print_vec_dict(spec, NULL)))
        return UNDEF;

    ip->valid_info = 1;
             
    PRINT_TRACE (2, print_string, "Trace: leaving init_tr_sbqr_incr_obj");
    
    return (new_inst);
}

static int decompose_query(STATIC_INFO* ip, VEC* qvec, VEC** sent_vecs)
{
	int i, j, s;
	int sent_count;
	VEC *svec;
	SM_INDEX_TEXTDOC pp_vec, pp_svec;

	if (UNDEF == get_textqry(qvec->id_num.id, &pp_vec, ip->textdoc_inst))
		return(UNDEF);
    pp_vec.mem_doc.id_num = qvec->id_num;

	sent_count = 0;
	for (i = 0; i < pp_vec.mem_doc.num_sections; i++) {
	    for (j = 0; j < pp_vec.mem_doc.sections[i].num_subsects; j++) {
			sent_count += pp_vec.mem_doc.sections[i].subsect[j].num_subsects;
		}
	}

    if (NULL == (*sent_vecs = Malloc(sent_count, VEC)))
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
				svec = *sent_vecs + sent_count;

    			if (UNDEF == index_pp->proc(&pp_svec, svec, ip->index_pp_inst))
	    			return UNDEF;
				if (UNDEF == save_vec(svec))
					return(UNDEF);

				snprintf(msg, sizeof(msg), "sentence[%d]:", sent_count);
		    	PRINT_TRACE (8, print_string, msg);
				strncpy(msg, &pp_svec.doc_text[pp_svec.mem_doc.sections[0].begin_section],
						MIN(sizeof(msg)-1,
						pp_svec.mem_doc.sections[0].end_section - pp_svec.mem_doc.sections[0].begin_section));
		    	PRINT_TRACE (8, print_string, msg);

				snprintf(msg, sizeof(msg), "sentence vector[%d]:", sent_count);
	    		PRINT_TRACE (8, print_string, msg);
        		if (sm_trace >= 8) {
	        		if (UNDEF == print_vec_dict(svec, NULL, print_vec_inst))
	            		return UNDEF;
				}

				sent_count++;
			}
		}
	}

	for (i = 0; i < sent_count; i++) {
		svec = *sent_vecs + i;
		svec->id_num.id = i;
		snprintf(msg, sizeof(msg), "sentence vector[%d]: ", i);
	    PRINT_TRACE (8, print_string, msg);
      	if (sm_trace >= 8) {
	   		if (UNDEF == print_vec_dict(svec, NULL, print_vec_inst))
	   			return UNDEF;
		}
	}

	return sent_count;
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

int
tr_sbqr_incr_obj (tr_file, outfile, inst)
char *tr_file;
char *outfile;
int inst;
{
    STATIC_INFO *ip;
    VEC  reduced_qvec, qvec, doc, tmp;
	VEC* qsent_vecs;
    long i;
	TR_VEC tr_vec;
	int reduced_vec_fd;
	int tr_fd;
	EID eid;
	int  qsent_count, ulimit, j, k;
	ALT_RESULTS   sent_thisdoc_sim;
	int numToDelForThisDoc;
	VEC_LIST_PAIR pair;
	VEC_LIST      sveclist, dveclist;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "tr_sbqr_incr_obj");
        return (UNDEF);
    }
    ip  = &info[inst];

	PRINT_TRACE (2, print_string, "Trace: entering tr_sbqr_incr_obj");

    if (UNDEF == (tr_fd = open_tr_vec (tr_file, tr_mode)))
		return UNDEF;
    if (UNDEF == (reduced_vec_fd = open_vector (outfile, reduced_query_file_mode)))
        return (UNDEF);


	/* Iterate over all tr_vec records and read the current query */
    while (1 == read_tr_vec (tr_fd, &tr_vec)) {
		// Read the query vector
		qvec.id_num.id = tr_vec.qid;

	    if (UNDEF == seek_vector(ip->qvec_fd, &qvec) ||
		    UNDEF == read_vector(ip->qvec_fd, &qvec))
			return(UNDEF);

		qsent_count = decompose_query(ip, &qvec, &qsent_vecs);
		if (qsent_count == UNDEF)
			return UNDEF;

	    for (i = 0; i < tr_vec.num_tr; i++) {
	    	doc.id_num.id = tr_vec.tr[i].did;
    		if (UNDEF == seek_vector (ip->doc_fd, &doc) ||
	    		UNDEF == read_vector (ip->doc_fd, &doc)) {
				return UNDEF ;
			}

			dveclist.num_vec = 1; dveclist.vec = &doc;
			sveclist.num_vec = qsent_count; sveclist.vec = qsent_vecs;

			pair.vec_list1 = &dveclist;
			pair.vec_list2 = &sveclist;

			if ( UNDEF == vecs_vecs->proc(&pair, &sent_thisdoc_sim, ip->vv_inst))
				return UNDEF;
			PRINT_TRACE(8, print_long, sent_thisdoc_sim.num_results);

			// Eliminate as much as you can from the top ranked document and then go on decreasing
			// the fraction as you move down the ranked list
			numToDelForThisDoc = max_del - max_del/(float)tr_vec.num_tr * (tr_vec.tr[i].rank-1);
			snprintf(msg, sizeof(msg), "Number of sentences to eliminate from %d-th doc = %d [max_delete = %d]",
						tr_vec.tr[i].rank, numToDelForThisDoc, max_del);
		    PRINT_TRACE (4, print_string, msg);

			ulimit = qsent_count - numToDelForThisDoc;
			if (ulimit < 1 || ulimit >= sent_thisdoc_sim.num_results)
				ulimit = sent_thisdoc_sim.num_results;
			ulimit = MIN(ulimit, sent_thisdoc_sim.num_results);

			// Sort the results
    		qsort(sent_thisdoc_sim.results, sent_thisdoc_sim.num_results, sizeof(RESULT_TUPLE), compare_rtup_sim);
			// Rearrange the array of qsent_vecs in the same order
			for (j = 0; j < ulimit; j++) {
				copy_vec(&tmp, &qsent_vecs[j]);
				free_vec(&qsent_vecs[j]);
				k = sent_thisdoc_sim.results[j].did.id;
				copy_vec(&qsent_vecs[j], &qsent_vecs[k]);
				free_vec(&qsent_vecs[k]);
				copy_vec(&qsent_vecs[k], &tmp);
				free_vec(&tmp);
				qsent_vecs[j].id_num.id = j;
				qsent_vecs[k].id_num.id = k;
			}
			qsent_count = ulimit;
		}

		copy_vec(&reduced_qvec, &qsent_vecs[0]);
		for (i = 1; i < qsent_count ; i++) {
			if (qsent_vecs[i].num_conwt == 0)
				continue;
			if (UNDEF == merge_vec(&reduced_qvec, &qsent_vecs[i], &tmp))
				return UNDEF;
			if (UNDEF == free_vec(&reduced_qvec))
				return UNDEF;
			if (UNDEF == copy_vec(&reduced_qvec, &tmp))
				return UNDEF;
			if (UNDEF == free_vec(&tmp))
				return UNDEF;
			if (UNDEF == free_vec(&(qsent_vecs[i])))
				return UNDEF;
		}

		reduced_qvec.id_num.id = qvec.id_num.id;
		if (UNDEF == seek_vector (reduced_vec_fd, &reduced_qvec) ||
			UNDEF == write_vector (reduced_vec_fd, &reduced_qvec))
			return (UNDEF);

		Free(qsent_vecs);
		free_vec(&reduced_qvec);
	}


    if (UNDEF == close_vector (reduced_vec_fd))
        return (UNDEF);
    if (UNDEF == close_tr_vec (tr_fd))
		return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving tr_sbqr_incr_obj");
    return (1);
}

int
close_tr_sbqr_incr_obj(inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_tr_sbqr_incr_obj");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_tr_sbqr_incr_obj");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info = 0;

	if (UNDEF == index_pp->close_proc(ip->index_pp_inst))
    	return UNDEF;			
   	if (UNDEF == vecs_vecs->close_proc(ip->vv_inst))
		return UNDEF;
	if ( UNDEF == close_vector(ip->doc_fd) )
		return UNDEF ;
    if (UNDEF == close_vector (ip->qvec_fd))
        return (UNDEF);
	if (UNDEF == close_get_textqry(ip->textdoc_inst))
   		return UNDEF;
   	if (sm_trace >= 8 &&
       	UNDEF == close_print_vec_dict(print_vec_inst))
        return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving close_tr_sbqr_incr_obj");
    return (0);
}

