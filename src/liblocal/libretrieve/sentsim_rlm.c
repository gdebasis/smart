#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/sentsim_rlm.c,v 11.2 1993/02/03 16:51:11 smart Exp $";
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
#include "docdesc.h"
#include "buf.h"
#include "tr_vec.h"
#include "local_eid.h"
#include "collstat.h"
#include "dir_array.h"

// The following parameters are to be read from the spec file
// for obtaining collection information.
static char* prefix;
static char* collstat_file ;
static long collstat_mode ;
static SPEC_PARAM_PREFIX spec_prefix_args[] = {
    {&prefix, "collstat_file",   getspec_dbfile,    (char *) &collstat_file},
    {&prefix, "collstat.rwmode", getspec_filemode,  (char *) &collstat_mode},
} ;
static int num_spec_prefix_args = sizeof (spec_prefix_args) / sizeof (spec_prefix_args[0]);


static int doc_fd;
static char* dvec_file ;
static long dvec_file_mode;
static PROC_TAB *vec_vec_ptab;
static PROC_INST vec_vec;
static float mixing_lambda, rerank_lambda ;
static int num_ctypes;
static SM_INDEX_DOCDESC doc_desc;

// Collection information
typedef struct {
    long   *freq;		// frequency of the ith term in the collection
    long   num_freq;	// total number of terms in collection
	long   totalDocFreq;
	float  *p_w_Q;	// RLM estimated probabilities for this ctype
	CON_WT *p_w_R;	// RLM estimated probabilities for this ctype
	int    vsize
} CtypeCollInfo ;

static CtypeCollInfo* collstats_info ;	// array of collection stat frequencies
static int* collstats_fd;  						// an array of fds one for each concept type

static SPEC_PARAM spec_args[] = {
    {"sentsim_rlm.nnn_doc_file",        getspec_dbfile, (char *) &dvec_file},
    {"sentsim_rlm.doc_file.rmode",  getspec_filemode, (char *) &dvec_file_mode},
    {"sentsim_rlm.vecs_vecs.vec_vec", getspec_func, (char *) &vec_vec_ptab},
    {"sentsim_rlm.mixing_lambda", getspec_float, (char*)&mixing_lambda},
    {"sentsim_rlm.rerank_lambda", getspec_float, (char*)&rerank_lambda},
    TRACE_PARAM ("convert.sentsim_rlm.trace")
};
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static float getDocumentLength(VEC* vec, int);
static long getTotalDocFreq(int ctype);
static int compare_con(void* this, void* that);

int init_sentsim_rlm (SPEC* spec, char* unused)
{
	int ctype;
	DIR_ARRAY dir_array;
	char conceptName[64];

    PRINT_TRACE (2, print_string, "Trace: entering init_sentsim_rlm");

	prefix = conceptName;
	// Get the number of concepts
    if (UNDEF == lookup_spec_docdesc (spec, &doc_desc))
        return (UNDEF);
	num_ctypes = doc_desc.num_ctypes;

	// Initialize the array of fds for opening the collection files
	collstats_fd = (int*) malloc (sizeof(int) * num_ctypes) ;
	if (collstats_fd == NULL)
		return UNDEF ;

	collstats_info = (CtypeCollInfo*) malloc (sizeof(CtypeCollInfo) * num_ctypes) ;
	if ( collstats_info == NULL )
		return UNDEF;

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

   	if (UNDEF == (doc_fd = open_vector (dvec_file, dvec_file_mode)))
       	return (UNDEF);

	// For each concept, collect the collection frequency
	for (ctype = 0; ctype < num_ctypes; ctype++) {
		snprintf(conceptName, sizeof(conceptName), "ctype.%d.", ctype) ;
    	if (UNDEF == lookup_spec_prefix (spec, spec_prefix_args, num_spec_prefix_args))
	        return (UNDEF);
		if (! VALID_FILE (collstat_file)) {
			return UNDEF;
	    }
	    else {
			if (UNDEF == (collstats_fd[ctype] = open_dir_array (collstat_file, collstat_mode)))
				return (UNDEF);

			// Read in collection frequencies
        	dir_array.id_num = COLLSTAT_COLLFREQ;
	        if (1 != seek_dir_array (collstats_fd[ctype], &dir_array) ||
    	        1 != read_dir_array (collstats_fd[ctype], &dir_array)) {
		    	collstats_info[ctype].freq = NULL;
			    collstats_info[ctype].num_freq = 0;
				return UNDEF;
    	    }
        	else {
	            // Read from file successful. Allocate 'freq' array and dump the contents of the file in this list
			    if (NULL == (collstats_info[ctype].freq = (long *)malloc ((unsigned) dir_array.num_list)))
			        return (UNDEF);
		    	bcopy (dir_array.list, (char *) collstats_info[ctype].freq, dir_array.num_list);
			    collstats_info[ctype].num_freq = dir_array.num_list / sizeof (long);
			
				if (NULL == (collstats_info[ctype].p_w_Q = (float*) malloc (collstats_info[ctype].num_freq * sizeof(float))))
					return UNDEF;
	        }
		}
    }

	// Compute the total Document Frequency only once since it is a constant.
	for (ctype = 0; ctype < num_ctypes; ctype++) {
		collstats_info[ctype].totalDocFreq = getTotalDocFreq(ctype);
	}

    PRINT_TRACE (2, print_string, "Trace: leaving init_sentsim_rlm");
    return 0;
}

static long getTotalDocFreq(int ctype)
{
	long i;
	long totalDocFreq = 0 ;
	for (i = 0; i < collstats_info[ctype].num_freq; i++) {
		totalDocFreq += collstats_info[ctype].freq[i] ;
	}
	return totalDocFreq ;
}

static float getDocumentLength(VEC* vec, int ctype) {
	int i;
	float length = 0 ;
	CON_WT* start = vec->con_wtp ;

	// Jump to the appropriate part of the vector
	for (i = 0; i < ctype; i++) {
		start += vec->ctype_len[i] ;
	}

	// Read ctype_len[ctype] weights and add them up.
	for (i = 0; i < vec->ctype_len[ctype]; i++) {
		length += start[i].wt ;
	}

	return length ;
}

int sentsim_rlm (Sentence* sentences, int numSentences, VEC* qvec, TR_VEC* tr)
{
	TR_TUP  *tr_tup;
	int     ctype;
	long    tr_ind, con, i;
	VEC     dvec, *svec;
	CON_WT  *conwtp, *start, *this_conwt;
	float   p_w_D, p_D_Q, kl_div, docLen, norm_tf;
	Sentence* s;

    PRINT_TRACE (2, print_string, "Trace: entering sentsim_rlm");

	for (ctype = 0; ctype < num_ctypes; ctype++)
		memset(collstats_info[ctype].p_w_Q, 0, collstats_info[ctype].num_freq * sizeof(float));

	// Estimate P(w|R) for each term in the top ranked passages and then aggregate
	// the scores over sentences...
	for (tr_ind = 0; tr_ind < tr->num_tr; tr_ind++) {
		tr_tup = &tr->tr[tr_ind];
		
		/* Open the document vector and search for this given word
		 * in the con_wt array. The number stored in the LM vector
		 * is {tf.sum(df)}/{sum(tf).df}
		 */
	    dvec.id_num.id = tr->tr[tr_ind].did;
	    if (UNDEF == seek_vector (doc_fd, &dvec) ||
    	    UNDEF == read_vector (doc_fd, &dvec)) {
   	    	return UNDEF;
	    }

		// LM similarity as stored in the LM vec is an approximation of log(P(Q|D))
		p_D_Q = tr_tup->sim;

		// Iterate for every ctype in this document
		for (ctype = 0, start = dvec.con_wtp; ctype < dvec.num_ctype; ctype++) {
			docLen = getDocumentLength(&dvec, ctype);
			// Iterate for every concept of this ctype
			for (conwtp = start; conwtp < &start[dvec.ctype_len[ctype]]; conwtp++) {
				 // P(w|Q) = \sum D \in R {P(w|D) * P(D|Q)}
				 // P(w|D) is to be computed as lambda *tf(w,D)/sum(tf, D) + (1-lambda)*df(w)/sum(df)
				 // tr->tr[tr_ind].sim contains the accumulated log P(D|Q )
				if (conwtp->con >= 0 && conwtp->con < collstats_info[ctype].num_freq) { // sanity check of array index access
					p_w_D = mixing_lambda*conwtp->wt/docLen +
							(1-mixing_lambda)*collstats_info[ctype].freq[conwtp->con]/(float)collstats_info[ctype].totalDocFreq;
					collstats_info[ctype].p_w_Q[conwtp->con] += p_w_D * p_D_Q;
				}
			}
			start += dvec.ctype_len[ctype];
		}
	}
//#define AGGR
#ifdef AGGR 
	// Aggregate the RLM term scores over sentences
	for (s = sentences; s < &sentences[numSentences]; s++) {
		svec = &(s->svec);
		s->sim = 0;
		for (ctype = 0, start = svec->con_wtp; ctype < svec->num_ctype; ctype++) {
			docLen = getDocumentLength(svec, ctype);
			for (conwtp = start; conwtp < &start[svec->ctype_len[ctype]]; conwtp++) {
				if (conwtp->con >= 0 && conwtp->con < collstats_info[ctype].num_freq) { // sanity check of array index access
					s->sim += conwtp->wt/docLen * collstats_info[ctype].p_w_Q[conwtp->con];
				}
			}
			start += svec->ctype_len[ctype];
		}
	}
#endif

#define KLDIV
#ifdef KLDIV
	for (ctype = 0; ctype < num_ctypes; ctype++) {
		collstats_info[ctype].vsize = 0;
		for (con = 0; con < collstats_info[ctype].num_freq; con++) {
			if (collstats_info[ctype].p_w_Q[con] > 0 && collstats_info[ctype].freq[con] > 0) collstats_info[ctype].vsize++;
		}
		collstats_info[ctype].p_w_R = Malloc(collstats_info[ctype].vsize, CON_WT);

		for (i = 0, con = 0; con < collstats_info[ctype].num_freq; con++) {
			if (collstats_info[ctype].p_w_Q[con] > 0 && collstats_info[ctype].freq[con] > 0) {
				collstats_info[ctype].p_w_R[i].con = con;
				collstats_info[ctype].p_w_R[i].wt = collstats_info[ctype].p_w_Q[con];
				i++;
			}
		}
	}

	// Compute KL divergence of each sentence LM from the RLM
	for (s = sentences; s < &sentences[numSentences]; s++) {
		svec = &(s->svec);
		kl_div = 0;

		for (ctype = 0, start = svec->con_wtp; ctype < svec->num_ctype; ctype++) {
			docLen = getDocumentLength(svec, ctype);

			// We need to iterate over the entire set of terms in the retrieved result set.
			for (conwtp = collstats_info[ctype].p_w_R; conwtp < &(collstats_info[ctype].p_w_R[collstats_info[ctype].vsize]); conwtp++) {
				// This search gives the tf (t_i|d) i.e.
				// the term frequency of the current term in the current document.
				// It might be zero.
	        	this_conwt = (CON_WT*) bsearch(conwtp, start, svec->ctype_len[ctype], sizeof(CON_WT), compare_con);
				norm_tf = (this_conwt == NULL)? 0 : this_conwt->wt/docLen;
				p_w_D = rerank_lambda*norm_tf + (1-rerank_lambda)*collstats_info[ctype].freq[conwtp->con]/(float)collstats_info[ctype].totalDocFreq;
				kl_div += conwtp->wt * log(conwtp->wt/p_w_D);
			}

			start += svec->ctype_len[ctype];
		}
		s->sim = -kl_div;
	}
#endif

    PRINT_TRACE (2, print_string, "Trace: leaving sentsim_rlm");
    return (1);
}

int close_sentsim_rlm ()
{
	int ctype;
    PRINT_TRACE (2, print_string, "Trace: entering close_sentsim_rlm");

	for (ctype = 0; ctype < doc_desc.num_ctypes; ctype++) {
    	if (UNDEF == close_dir_array (collstats_fd[ctype]))
	    	return (UNDEF);
   		free(collstats_info[ctype].freq);
   		free(collstats_info[ctype].p_w_Q);
#ifdef KLDIV
		Free(collstats_info[ctype].p_w_R);
#endif
	}

	free(collstats_info);
	free(collstats_fd);

	if ( UNDEF == close_vector(doc_fd) )
		return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving close_sentsim_rlm");
    return (0);
}

static int compare_con(void* this, void* that) {
    CON_WT *this_conwt, *that_conwt;
    this_conwt = (CON_WT*) this;
    that_conwt = (CON_WT*) that;

    return this_conwt->con < that_conwt->con ? -1 : this_conwt->con == that_conwt->con ? 0 : 1;
}
