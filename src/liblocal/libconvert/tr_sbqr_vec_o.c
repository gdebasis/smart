#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/tr_sbqr_o.c,v 11.0 1992/07/21 18:20:09 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.

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
static float retention_frac;
static PROC_TAB *index_pp, *vecs_vecs;
static char* reductionMethodName;
static int print_vec_inst;
static char msg[1024];

static SPEC_PARAM spec_args[] = {
    {"convert.tr_sbqr.query_file", getspec_dbfile, (char *) &query_file},
    {"convert.tr_sbqr.tr_file.rmode", getspec_filemode, (char *) &tr_mode},
    {"convert.tr_sbqr.query_file.rmode", getspec_filemode, (char *) &query_file_mode},
    {"convert.tr_sbqr.reduced_query_file.rwcmode", getspec_filemode, (char *) &reduced_query_file_mode},
    {"convert.tr_sbqr.doc_file",     getspec_dbfile,    (char *) &doc_file},
    {"convert.tr_sbqr.doc_file.rmode", getspec_filemode,(char *) &doc_mode},
    {"convert.tr_sbqr.retention_fraction", getspec_float,(char *) &retention_frac},
    {"convert.tr_sbqr.reduction_method", getspec_string,(char *) &reductionMethodName},
    PARAM_FUNC("convert.tr_sbqr.index_pp", &index_pp),
    PARAM_FUNC("convert.tr_sbqr.vecs_vecs", &vecs_vecs),
    TRACE_PARAM ("convert.tr_sbqr.trace")
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

int init_tr_sbqr_obj(SPEC* spec, char* prefix)
{
    int new_inst;
    STATIC_INFO *ip;
    CONTEXT old_context;
	char new_prefix[PATH_LEN];

    PRINT_TRACE (2, print_string, "Trace: entering init_tr_sbqr_obj");
    
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
             
    PRINT_TRACE (2, print_string, "Trace: leaving init_tr_sbqr_obj");
    
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
				// Efficient relocation of this vector after sorting by the similarity values
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

/* Compute the similarity of each sentence with the document vector.
 * The query vector is in nnn.
 * If doc_file is lm weighted then we would compute the probability
 * of generating each query sentence from a top ranked document
 * vector. */
static int compute_qry_sentence_sims(STATIC_INFO* ip, VEC* sent_vecs, int sent_count, VEC* dvec, ALT_RESULTS* sent_alldoc_sims)
{
	VEC_LIST_PAIR pair;
	VEC_LIST      sveclist, dveclist;
	ALT_RESULTS   sent_thisdoc_sim;
	int           i;

	dveclist.num_vec = 1; dveclist.vec = dvec;
	sveclist.num_vec = sent_count; sveclist.vec = sent_vecs;

	pair.vec_list1 = &dveclist;
	pair.vec_list2 = &sveclist;

	if ( UNDEF == vecs_vecs->proc(&pair, &sent_thisdoc_sim, ip->vv_inst))
		return UNDEF;
	PRINT_TRACE(8, print_long, &sent_thisdoc_sim.num_results);
	if (sent_thisdoc_sim.num_results == 0)
		return 0;
	if (sent_alldoc_sims->results == NULL) {
		// This is the first invokation for a new query. Copy this doc sims
		// into all doc sims.
		sent_alldoc_sims->results = Malloc(sent_count, RESULT_TUPLE);
		Bcopy(sent_thisdoc_sim.results, sent_alldoc_sims->results, sizeof(RESULT_TUPLE)*sent_thisdoc_sim.num_results);
		sent_alldoc_sims->num_results = sent_thisdoc_sim.num_results;
	}
	else {
		// Merge the similarities
		for (i = 0; i < sent_thisdoc_sim.num_results; i++) {
			sent_alldoc_sims->results[i].sim += sent_thisdoc_sim.results[i].sim; 
		}
	}
	return 1;
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

// Arrange in descending order of similarity
// Retain a fraction of the top most similar query sentences.
static int constructReducedQuery(VEC* qvec, VEC* qsent_vecs, ALT_RESULTS* qrysentence_doc_sims, VEC* outvec, float f)
{
	int i, ulimit = (int)qrysentence_doc_sims->num_results * f;
	VEC merged_vec;

	// check sanity of the configuration parameter
	if (ulimit < 1 || ulimit >= qrysentence_doc_sims->num_results)
		ulimit = qrysentence_doc_sims->num_results/2;

	if (qrysentence_doc_sims->num_results == 0)
		return 0;

    qsort(qrysentence_doc_sims->results, qrysentence_doc_sims->num_results, sizeof(RESULT_TUPLE), compare_rtup_sim);
	if (sm_trace >= 4) {
		for (i = 0; i < qrysentence_doc_sims->num_results; i++) {
			snprintf(msg, sizeof(msg), "sim: %d %d %f",
							qvec->id_num.id,
						    qrysentence_doc_sims->results[i].did.id,
							qrysentence_doc_sims->results[i].sim);
			PRINT_TRACE(4, print_string, msg);
		}
	}

	copy_vec(outvec, &qsent_vecs[ qrysentence_doc_sims->results[0].did.id ]);
	for (i = 1; i < ulimit ; i++) {
		if (UNDEF == merge_vec(outvec, &qsent_vecs[qrysentence_doc_sims->results[i].did.id], &merged_vec))
			return UNDEF;
		if (UNDEF == free_vec(outvec))
			return UNDEF;
		if (UNDEF == copy_vec(outvec, &merged_vec))
			return UNDEF;
		if (UNDEF == free_vec(&merged_vec))
			return UNDEF;
	}
	Free(qrysentence_doc_sims->results);
	return 1; 
}

static int findCutOff(ALT_RESULTS* qrysentence_doc_sims, float f) {
	// assumption: qrysentence_doc_sims is already sorted
	float area = 0, leftArea = 0, maxsim;
	long  i;
	RESULT_TUPLE* normalized_qrysentence_doc_sims;

	normalized_qrysentence_doc_sims = (RESULT_TUPLE*) Malloc(qrysentence_doc_sims->num_results, RESULT_TUPLE);
	memcpy(normalized_qrysentence_doc_sims, qrysentence_doc_sims->results, qrysentence_doc_sims->num_results * sizeof(RESULT_TUPLE));

	// Find the normalized area of the curve (Maximum value is the first point)
	maxsim = normalized_qrysentence_doc_sims[0].sim;
	for (i=0; i < qrysentence_doc_sims->num_results; i++) {
		normalized_qrysentence_doc_sims[i].sim = normalized_qrysentence_doc_sims[i].sim / maxsim;
		area += normalized_qrysentence_doc_sims[i].sim;
	}

	for (i=0; i < qrysentence_doc_sims->num_results; i++) {
		leftArea += normalized_qrysentence_doc_sims[i].sim;
		if (leftArea >= f*area)
			break;
	}
	return (int)i;
}

// Arrange in descending order of similarity
// Retain a fraction of the area of the curve under the similarity points.
// Thus, the number of sentences we retain in the query is proportional to
// the area of the curve.
static int constructReducedQueryByROC(VEC* qvec, VEC* qsent_vecs, ALT_RESULTS* qrysentence_doc_sims, VEC* outvec, float f)
{
	int i, j, ulimit;
	VEC merged_vec;

	if (qrysentence_doc_sims->num_results == 0)
		return 0;

    qsort(qrysentence_doc_sims->results, qrysentence_doc_sims->num_results, sizeof(RESULT_TUPLE), compare_rtup_sim);

	if (sm_trace >= 4) {
		// Only for printing
		for (i = 0; i < qrysentence_doc_sims->num_results; i++) {
			snprintf(msg, sizeof(msg), "sim: %d %d %f",
							qvec->id_num.id,
						    qrysentence_doc_sims->results[i].did.id,
							qrysentence_doc_sims->results[i].sim);
			PRINT_TRACE(4, print_string, msg);
		}
	}

	ulimit = findCutOff(qrysentence_doc_sims, f);
	// check sanity of the computed cut-off point
	if (ulimit < 1 || ulimit >= qrysentence_doc_sims->num_results)
		ulimit = qrysentence_doc_sims->num_results/2;

	if (sm_trace >= 4) {
		// Only for printing
		for (j = 0; j < ulimit; j++) {
			snprintf(msg, sizeof(msg), "rsim: %d %d %f",
							qvec->id_num.id,
						    qrysentence_doc_sims->results[j].did.id,
							qrysentence_doc_sims->results[j].sim);
			PRINT_TRACE(4, print_string, msg);
		}
	}

	copy_vec(outvec, &qsent_vecs[ qrysentence_doc_sims->results[0].did.id ]);
	for (i = 1; i < ulimit ; i++) {
		if (UNDEF == merge_vec(outvec, &qsent_vecs[qrysentence_doc_sims->results[i].did.id], &merged_vec))
			return UNDEF;
		if (UNDEF == free_vec(outvec))
			return UNDEF;
		if (UNDEF == copy_vec(outvec, &merged_vec))
			return UNDEF;
		if (UNDEF == free_vec(&merged_vec))
			return UNDEF;
	}
	Free(qrysentence_doc_sims->results);
	return 1; 
}

int
tr_sbqr_obj (tr_file, outfile, inst)
char *tr_file;
char *outfile;
int inst;
{
    STATIC_INFO *ip;
    VEC  reduced_qvec, qvec, doc;
	VEC* qsent_vecs;
    long i;
	TR_VEC tr_vec;
	int reduced_vec_fd;
	int tr_fd;
	EID eid;
	int  qsent_count;
	ALT_RESULTS qrysentence_doc_sims;
	float f;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "tr_sbqr_obj");
        return (UNDEF);
    }
    ip  = &info[inst];

	PRINT_TRACE (2, print_string, "Trace: entering tr_sbqr_obj");

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
		qrysentence_doc_sims.num_results = 0;
		qrysentence_doc_sims.results = NULL;

	    for (i = 0; i < tr_vec.num_tr; i++) {
	    	doc.id_num.id = tr_vec.tr[i].did;
    		if (UNDEF == seek_vector (ip->doc_fd, &doc) ||
	    		UNDEF == read_vector (ip->doc_fd, &doc)) {
				return UNDEF ;
			}

			if (UNDEF == compute_qry_sentence_sims(ip, qsent_vecs, qsent_count, &doc, &qrysentence_doc_sims))
				return (UNDEF);
		}

		// Eliminate as much as you can from the top ranked document and then go on decreasing
		// the fraction as you move down the ranked list
#if 0
		f = retention_frac + (1-retention_frac)*(tr_vec.tr[i].rank-1)/(float)tr_vec.num_tr;
		snprintf(msg, sizeof(msg), "Retention fraction for %d-th doc = %f [fmax = %f]",
						tr_vec.tr[i].rank, f, retention_frac);
	    PRINT_TRACE (4, print_string, msg);
#endif
		if (!strcmp(reductionMethodName, "roc")) {
			if (0 == constructReducedQueryByROC(&qvec, qsent_vecs, &qrysentence_doc_sims, &reduced_qvec, retention_frac))
				copy_vec(&reduced_qvec, &qvec); 
		}
		else {
			if (0 == constructReducedQuery(&qvec, qsent_vecs, &qrysentence_doc_sims, &reduced_qvec, retention_frac))
				copy_vec(&reduced_qvec, &qvec); 
		}

		reduced_qvec.id_num.id = qvec.id_num.id;
		if (UNDEF == seek_vector (reduced_vec_fd, &reduced_qvec) ||
			UNDEF == write_vector (reduced_vec_fd, &reduced_qvec))
			return (UNDEF);

    	for (i = 0; i < qsent_count; i++)
			free_vec(&(qsent_vecs[i]));
		Free(qsent_vecs);
		free_vec(&reduced_qvec);
	}


    if (UNDEF == close_vector (reduced_vec_fd))
        return (UNDEF);
    if (UNDEF == close_tr_vec (tr_fd))
		return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving tr_sbqr_obj");
    return (1);
}

int
close_tr_sbqr_obj(inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_tr_sbqr_obj");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_tr_sbqr_obj");
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

    PRINT_TRACE (2, print_string, "Trace: leaving close_tr_sbqr_obj");
    return (0);
}

