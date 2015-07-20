#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/sentsim_trlm.c,v 11.2 1993/02/03 16:51:11 smart Exp $";
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
#include "lda.h"

static int doc_fd;
static char* dvec_file ;
static long dvec_file_mode;
static char* collstat_file ;
static int collstats_fd;
static long collstat_mode ;
static float mixing_lambda ;
static long totalDocFreq;
static float *collstats_freq;		// frequency of the ith term in the collection
static long collstats_num_freq;		// total number of terms in collection
static long collstats_numdocs;		// Total number of documents in the collection
static float *p_w_Q;				// conditional prob. of generating w from the current query Q
static LDAModel ldamodel;
static int multifaceted;

static SPEC_PARAM spec_args[] = {
    {"sentsim_trlm.nnn_doc_file",        getspec_dbfile, (char *) &dvec_file},
    {"sentsim_trlm.doc_file.rmode",  getspec_filemode, (char *) &dvec_file_mode},
    {"sentsim_trlm.collstat_file",        getspec_dbfile, (char *) &collstat_file},
    {"sentsim_trlm.collstat_file.rmode",  getspec_filemode, (char *) &collstat_mode},
    {"sentsim_trlm.mixing_lambda", getspec_float, (char*)&mixing_lambda},
    {"sentsim_trlm.multifaceted", getspec_bool, (char *) &multifaceted},
    TRACE_PARAM ("convert.sentsim_trlm.trace")
};
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static long getDocumentLength(VEC* vec);
static float getTotalDocumentFreq();

int init_sentsim_trlm (SPEC* spec, char* prefix)
{
	DIR_ARRAY dir_array;

    PRINT_TRACE (2, print_string, "Trace: entering init_sentsim_trlm");

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

   	if (UNDEF == (doc_fd = open_vector (dvec_file, dvec_file_mode)))
       	return (UNDEF);

	// initalize the LM conversion module
	if (UNDEF == init_lang_model_wt_lm(spec, NULL))
		return UNDEF;

	if (! VALID_FILE (collstat_file)) {
		return UNDEF;
    }
    else {
		if (UNDEF == (collstats_fd = open_dir_array (collstat_file, collstat_mode)))
			return (UNDEF);

		// Read the total number of documents
        dir_array.id_num = COLLSTAT_NUMDOC; // Get the collection frequency list from the file
        if (1 != seek_dir_array (collstats_fd, &dir_array) ||
            1 != read_dir_array (collstats_fd, &dir_array)) {
			return UNDEF;
        }
        else {
			memcpy(&collstats_numdocs, dir_array.list, sizeof(long));
        }

		// Read in collection frequencies
        dir_array.id_num = COLLSTAT_TOTWT; // COLLSTAT_COLLFREQ; // ; // Get the collection frequency list from the file
        if (1 != seek_dir_array (collstats_fd, &dir_array) ||
            1 != read_dir_array (collstats_fd, &dir_array)) {
            collstats_freq = NULL;
            collstats_num_freq = 0;
			return UNDEF;
        }
        else {
            // Read from file successful. Allocate 'freq' array and dump the
            // contents of the file in this list
			collstats_freq = (float*) dir_array.list;
            collstats_num_freq = dir_array.num_list / sizeof (float);
			p_w_Q = (float*) malloc (collstats_num_freq * sizeof(float));
			if (p_w_Q == NULL)
				return UNDEF;
        }

    }
	totalDocFreq = getTotalDocumentFreq();

	if (UNDEF == init_lda_est(&ldamodel, spec)) {
		return UNDEF;
	}

    PRINT_TRACE (2, print_string, "Trace: leaving init_sentsim_trlm");
    return 0;
}

static float getTotalDocumentFreq() {
	long i;
	float totalDocFreq = 0 ;
	for (i = 0; i < collstats_num_freq; i++) {
		totalDocFreq += collstats_freq[i] ;
	}
	return totalDocFreq ;
}

static long getDocumentLength(VEC* vec)
{
	long i, length = 0 ;
	CON_WT* start = vec->con_wtp ;

	// Read ctype_len[ctype] weights and add them up.
	for (i = 0; i < vec->ctype_len[0]; i++) {
		length += start[i].wt ;
	}
	return length ;
}

int sentsim_trlm (Sentence* sentences, int numSentences, VEC* qvec, TR_VEC* tr)
{
	TR_TUP  *tr_tup;
	long    tr_ind, con, i;
	VEC     dvec, *svec;
	CON_WT  *conwtp; 
	float   p_w_D, p_D_Q, docLen;
	Sentence* s;

    PRINT_TRACE (2, print_string, "Trace: entering sentsim_trlm");

	memset(p_w_Q, 0, collstats_num_freq * sizeof(float));
	/* Call the LDA estimator to estimate P(w|z) and P(z|d)s */
	// For the multi-faceted model the query is considered
	// as a document in the LDA space.
	if (UNDEF == init_lda_est_inst(&ldamodel, doc_fd, tr, qvec, multifaceted)) {
		fprintf(stderr, "LDA reranking failed for query %d\n", qvec->id_num.id);
		return 1;
	}
	lda_est(&ldamodel);

	// Estimate P(w|R) for each term in the top ranked passages and then aggregate
	// the scores over sentences...
	for (tr_ind = 0; tr_ind < tr->num_tr; tr_ind++) {
		tr_tup = &(tr->tr[tr_ind]);
		
		/* Open the document vector and search for this given word
		 * in the con_wt array. The number stored in the LM vector
		 * is {tf.sum(df)}/{sum(tf).df}
		 */
	    dvec.id_num.id = tr->tr[tr_ind].did;
	    if (UNDEF == seek_vector (doc_fd, &dvec) ||
    	    UNDEF == read_vector (doc_fd, &dvec)) {
   	    	return UNDEF;
	    }

		docLen = (float)getDocumentLength(&dvec);
		// LM similarity as stored in the LM vec is an approximation of log(P(Q|D))
		// For the multi-faceted model obtain the marginalized probability...
		p_D_Q = !multifaceted? tr_tup->sim : getMarginalizedQueryGenEstimate(&ldamodel, tr_ind, collstats_freq, totalDocFreq);

		// Iterate for every word in this document
		for (conwtp = dvec.con_wtp; conwtp < &dvec.con_wtp[dvec.ctype_len[0]]; conwtp++) {
			 // P(w|Q) = \sum D \in R {P(w|D) * P(D|Q)}
			 // P(w|D) is to be computed as lambda *tf(w,D)/sum(tf, D) + (1-lambda)*df(w)/sum(df)
			 // tr->tr[tr_ind].sim contains the accumulated log P(D|Q )
			if (conwtp->con >= 0 && conwtp->con < collstats_num_freq) { // sanity check of array index access
				p_w_D = mixing_lambda*compute_marginalized_P_w_D(&ldamodel, conwtp, tr_ind) +
						(1-mixing_lambda)*collstats_freq[conwtp->con]/(float)totalDocFreq; 
				p_w_Q[conwtp->con] += p_w_D * p_D_Q;
			}
		}
	}

	// Aggregate the TRLM term scores over sentences
	for (s = sentences; s < &sentences[numSentences]; s++) {
		svec = &(s->svec);
		s->sim = 0;
		for (conwtp = svec->con_wtp; conwtp < &svec->con_wtp[svec->ctype_len[0]]; conwtp++) {
			if (conwtp->con >= 0 && conwtp->con < collstats_num_freq) { // sanity check of array index access
				if (p_w_Q[conwtp->con] > 0)
					s->sim += p_w_Q[conwtp->con];
			}
		}
	}

	close_lda_est_inst(&ldamodel);
    PRINT_TRACE (2, print_string, "Trace: leaving sentsim_trlm");
    return (1);
}

int close_sentsim_trlm ()
{
    PRINT_TRACE (2, print_string, "Trace: entering close_sentsim_trlm");

   	if (UNDEF == close_lang_model_wt_lm(0))
		return UNDEF;
    if (UNDEF == close_dir_array (collstats_fd))
	    return (UNDEF);
	if ( UNDEF == close_vector(doc_fd) )
		return UNDEF;
	close_lda_est(&ldamodel);
	FREE(p_w_Q);

    PRINT_TRACE (2, print_string, "Trace: leaving close_sentsim_trlm");
    return (0);
}


