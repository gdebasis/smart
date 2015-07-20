/* Compute the LM scores with log transform on the actual probabilities resulting in -ve sims */
#include <math.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "vector.h"
#include "dir_array.h"
#include "io.h"
#include "inst.h"
#include "collstat.h"
#include "trace.h"
#include "docdesc.h"

static char *prefix;
static char *collstat_file;
static char *collstat_mode;
static float lambda;

// The following parameters are to be read from the spec file
// for obtaining collection information.
static SPEC_PARAM_PREFIX spec_prefix_args[] = {
    {&prefix, "collstat_file",   getspec_dbfile,    (char *) &collstat_file},
    {&prefix, "collstat.rwmode", getspec_filemode,  (char *) &collstat_mode},
    {&prefix, "retrieve.lm.lambda", getspec_float, (char *) &lambda},  // this parameter is the initial lambda of LM
} ;

static int num_spec_prefix_args = sizeof (spec_prefix_args) / sizeof (spec_prefix_args[0]);

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("convert.weight.lm.trace")
} ;
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

// Collection information
struct coll_info {
    float *freq;		// frequency of the ith term in the collection
    long  num_freq;	// total number of terms in collection
} ;

static struct coll_info* collstats_info ;	// array of collection stat frequencies
static int* collstats_fd;  						// an array of fds one for each concept type
static long* totalDocFreq;

CON_WT *conwt_buf;
long num_conwt_buf;
static SM_INDEX_DOCDESC doc_desc ;

static float getTotalDocFreq(int ctype);

// Read from collstat file the collection frequency in the global
// variable collstats_info.
int init_lang_model_wt_cf_lm_nsim(SPEC* spec, char* unused)
{
    DIR_ARRAY dir_array;
	long ctype ;
	char conceptName[256];
	long i;

    PRINT_TRACE (2, print_string, "Trace: entering init_lang_model_wt_cf_lm_nsim");

	// Intialize buffer to copy invec's term weights into outvec's ones
    num_conwt_buf = 8096;

	if ( NULL == (conwt_buf = (CON_WT *)
                 malloc (num_conwt_buf * sizeof (CON_WT))) )
        return (UNDEF);

	// Get the number of concepts
    if (UNDEF == lookup_spec_docdesc (spec, &doc_desc))
        return (UNDEF);

	// Initialize the array of fds for opening the collection files
	collstats_fd = (int*) malloc (sizeof(int) * doc_desc.num_ctypes) ;
	if ( collstats_fd == NULL )
		return UNDEF ;

	collstats_info = (struct coll_info*) malloc (sizeof(struct coll_info) * doc_desc.num_ctypes) ;
	if ( collstats_info == NULL )
		return UNDEF ;

    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

	// For each concept, collect the collection frequency
	for (ctype = 0; ctype < doc_desc.num_ctypes; ctype++) {

		snprintf(conceptName, sizeof(conceptName), "ctype.%ld.", ctype) ;
		prefix = conceptName ;
    	if (UNDEF == lookup_spec_prefix (spec, spec_prefix_args, num_spec_prefix_args))
	        return (UNDEF);

	    if (! VALID_FILE (collstat_file)) {
			collstats_info[ctype].freq = NULL ;
        	collstats_info[ctype].num_freq = 0;
	    }
	    else {
   			if (UNDEF == (collstats_fd[ctype] = open_dir_array (collstat_file, collstat_mode)))
				return (UNDEF);

			// Get the frequency mode to use
			dir_array.id_num = 	COLLSTAT_TOTWT ;
 
			// Get the collection frequency list from the file
			if (1 != seek_dir_array (collstats_fd[ctype], &dir_array) ||
			    1 != read_dir_array (collstats_fd[ctype], &dir_array)) {
		    	collstats_info[ctype].freq = NULL;
			    collstats_info[ctype].num_freq = 0;
			}
			else {
				// Read from file successful. Allocate 'freq' array and dump the
				// contents of the file in this list
			    if (NULL == (collstats_info[ctype].freq = (float *)
			                 malloc ((unsigned) dir_array.num_list)))
			        return (UNDEF);
		    	(void) bcopy (dir_array.list,
		                  (char *) collstats_info[ctype].freq,
		                  dir_array.num_list);
			    collstats_info[ctype].num_freq = dir_array.num_list / sizeof (float);
			}

			if (UNDEF == close_dir_array (collstats_fd[ctype]))
				return (UNDEF);
		}
	}

	totalDocFreq = (float*) malloc (sizeof(float) * doc_desc.num_ctypes);

	// Compute the total Document Frequency only once since it is a constant.
	for (i = 0; i < doc_desc.num_ctypes; i++) {
		totalDocFreq[i] = getTotalDocFreq(i);
	}

    PRINT_TRACE (2, print_string, "Trace: leaving init_lang_model_wt_cf_lm_nsim");
    return (1);
}

static float getTotalDocFreq(int ctype)
{
	long i;
	float totalDocFreq = 0 ;
	for (i = 0; i < collstats_info[ctype].num_freq; i++) {
		totalDocFreq += collstats_info[ctype].freq[i] ;
	}
	return totalDocFreq ;
}

static int getDocumentLength(VEC* vec, int ctype)
{
	int i, length = 0 ;
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

/* 	This procedure assignes a score based on the language model.
	score(d) = product over all terms (
			lamda(i). P(d). P(t|d) +
			(1-lambda(i).P(t|Collection))
	where lambda(i) is the chance that the ith term is important in the query.
*/
int lang_model_wt_cf_lm_nsim(VEC* invec, VEC* outvec, int inst)
{
    CON_WT *invec_conwt_ptr, *outvec_conwt_ptr, *startInVec, *startOutVec ;
	float ratio ;
	float docLength ;
	long  ctype ; 

    PRINT_TRACE (2, print_string, "Trace: entering lang_model_wt_cf_lm_nsim");

   /* If no outvec is passed, we'll reweight the invec in place */
    if (outvec == NULL)
    	outvec = invec;
    else {
    	if (invec->num_conwt > num_conwt_buf) {
			if (NULL == (conwt_buf = Realloc( conwt_buf,
                          invec->num_conwt, CON_WT )))
        	return (UNDEF);
        	num_conwt_buf = invec->num_conwt;
		}
    }
    bcopy ((char *) invec->con_wtp,
           (char *) conwt_buf,
           (int) invec->num_conwt * sizeof (CON_WT));

    outvec->id_num = invec->id_num;
    outvec->num_ctype = invec->num_ctype;
    outvec->num_conwt = invec->num_conwt;
    outvec->con_wtp   = conwt_buf;
    outvec->ctype_len = invec->ctype_len ;
                                                                                   
	/* 	ptr->wt is the term frequency of the ith term.
		This number is divided by the total number of words in
		the document to get the probability of occurence of this term
		i.e. P(t|d).
		calculate: summation over ( log (1 + (0.5*tf(i)*totalDocFreq / (0.5*cf(i)*doc_len))) )  
	*/
	for ( 	ctype = 0, startInVec = invec->con_wtp, startOutVec = outvec->con_wtp;
			ctype < invec->num_ctype;
			startInVec += invec->ctype_len[ctype], startOutVec += outvec->ctype_len[ctype], ctype++) {

		docLength = (float)getDocumentLength(invec, ctype);

	    for (	invec_conwt_ptr = startInVec, outvec_conwt_ptr = startOutVec;
				invec_conwt_ptr - startInVec < invec->ctype_len[ctype];
				invec_conwt_ptr++, outvec_conwt_ptr++) {

			ratio = lambda*invec_conwt_ptr->wt/(float)docLength +
					(1-lambda)*collstats_info[ctype].freq[invec_conwt_ptr->con]/(float)totalDocFreq[ctype];
			outvec_conwt_ptr->con = invec_conwt_ptr->con ; 
			outvec_conwt_ptr->wt = log(ratio) ;
    	}
	}

    PRINT_TRACE (2, print_string, "Trace: leaving lang_model_wt_cf_lm_nsim");
    return 1 ;
}

int close_lang_model_wt_cf_lm_nsim(int inst)
{
	long ctype ;

    PRINT_TRACE (2, print_string, "Trace: entering close_lang_model_wt_cf_lm_nsim");

	if ( collstats_info != NULL ) {
		for (ctype = 0; ctype < doc_desc.num_ctypes; ctype++) {
		    if (NULL != collstats_info[ctype].freq)
    			free(collstats_info[ctype].freq);
		}

		free(collstats_info) ;
	}

	if (NULL != conwt_buf)
		free(conwt_buf) ;

	if ( collstats_fd != NULL )
		free(collstats_fd) ;

	if (totalDocFreq)
		free(totalDocFreq);

    PRINT_TRACE (2, print_string, "Trace: leaving close_lang_model_wt_cf_lm_nsim");
    return (1);
}
