#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/rr_prob_o.c,v 11.2 1993/02/03 16:51:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take an entire smart rr file, and convert to probabilities
 *1 local.convert.obj.rr_prob
 *2 rr_prob_obj (rr_file, text_file, inst)
 *3 char *rr_file;
 *3 char *text_file;
 *3 int inst;
 *4 init_rr_prob_obj (spec, unused)
 *5   "rr_prob_obj.rr_file"
 *5   "rr_prob_obj.rr_file.rmode"
 *5   "rr_prob_obj.doc_len_file"
 *5   "rr_prob_obj.bin_size"
 *5   "rr_prob_obj.tolerance"
 *5   "rr_prob_obj.trace"
 *4 close_rr_prob_obj (inst)

 *7 Read RR_VEC tuples from rr_file and convert to relevance (length)
 *7 probabilities. Takes the list of documents, sorts them by
 *7 their length, and assigns relevant documents to their respective
 *7 bins. Finally prints
 *7
 *7	Median Bin Length	Relevant in Bin/Total Relevant
 *7
 *7 Since dividing documents into bins of some size can result in
 *7 last bin with very few documents, DECREMENT bin size until the
 *7 last bin has tolerance*bin_size documents. Tolerance is typically
 *7 80% or so.
 *7
 *7 Return UNDEF if error, else 1.

 *8 Procedure is to assume we have a document length file which has
 *8 entries of type DID LENGTH
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "io.h"
#include "functions.h"
#include "spec.h"
#include "trace.h"
#include "smart_error.h"
#include "rr_vec.h"
#include "dict.h"
#include "vector.h"
#include "eid.h"

static char *default_rr_file;
static long rr_mode;
static char *doc_len_file;
static long bin_size;
static float tolerance;
static long min_size;
static SPEC_PARAM spec_args[] = {
    {"rr_prob_obj.rr_file",      getspec_dbfile,   (char *) &default_rr_file},
    {"rr_prob_obj.rr_file.rmode", getspec_filemode,(char *) &rr_mode},
    {"rr_prob_obj.doc_len_file",  getspec_string, (char *) &doc_len_file},
    {"rr_prob_obj.bin_size",  getspec_long, (char *) &bin_size},
    {"rr_prob_obj.tolerance",  getspec_float, (char *) &tolerance},
    {"rr_prob_obj.min_size",  getspec_long, (char *) &min_size},
    TRACE_PARAM ("rr_prob_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

#define CHUNK 100000
typedef struct {
    long did;
    long len;
    long nqr;	/* num queries retrieved (relevant) */
} DOC_DATA;

static DOC_DATA *docs;
static long num_docs;
static long max_docs;

static int comp_len(), comp_did();

int
init_rr_prob_obj (spec, unused)
SPEC *spec;
char *unused;
{
    FILE *fp;
    long i, did, len;

    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string,
                 "Trace: entering init_rr_prob_obj");

    if (NULL == (fp = fopen(doc_len_file, "r"))) {
	set_error(SM_ILLPA_ERR, "Can't open doc_len_file", "rr_prob_o");
	return UNDEF;
    }

    if (NULL == (docs = (DOC_DATA *) malloc(CHUNK*sizeof(DOC_DATA)))) {
	set_error(SM_ILLPA_ERR, "Not enough memory", "rr_prob_o");
	return UNDEF;
    }

    max_docs = CHUNK;
    for (i = 0; fscanf(fp, "%ld%ld", &did, &len) != EOF; i++) {
        if (len < min_size) {
            i--;
            continue;
        }
        if (i == max_docs) {
            max_docs += CHUNK;
            if (NULL == (docs = realloc((char *) docs,
				       max_docs*sizeof(DOC_DATA)))) {
                set_error(SM_ILLPA_ERR, "Not enough memory", "tr_prob_o");
                return UNDEF;
            }
        }
        docs[i].did = did;
        docs[i].len = len;
        docs[i].nqr = 0;
    }
    num_docs = i;

    PRINT_TRACE (2, print_string,
                 "Trace: leaving init_rr_prob_obj");

    return (0);
}

int
rr_prob_obj (rr_file, text_file, inst)
char *rr_file;
char *text_file;
int inst;
{
    FILE *out_fd;
    RR_VEC rr_vec;
    int rr_fd;
    DOC_DATA *docs_ptr;
    long final_bin_size = bin_size;
    long num_ret = 0;
    long i, bin;

    PRINT_TRACE (2, print_string, "Trace: entering rr_prob_obj");

    /* Open text output file */
    if (VALID_FILE (text_file)) {
        if (NULL == (out_fd = fopen (text_file, "w")))
            return (UNDEF);
    }
    else
        out_fd = stdout;

    /* Open input doc and dict files */
    if (rr_file == (char *) NULL)
        rr_file = default_rr_file;
    if (UNDEF == (rr_fd = open_rr_vec (rr_file, rr_mode)))
        return (UNDEF);

    /* sort by did for binary search */
    qsort((char *)docs, num_docs, sizeof(DOC_DATA), comp_did);

    while (1 == read_rr_vec (rr_fd, &rr_vec)) {
        PRINT_TRACE (7, print_rr_vec, &rr_vec);
        for (i = 0; i < rr_vec.num_rr; i++) {
            /* Lookup did in list */
	    docs_ptr = (DOC_DATA *)bsearch((char *)(&rr_vec.rr[i].did),
					   (char *)docs, num_docs,
					   sizeof(DOC_DATA), comp_did);
	    if (docs_ptr == NULL) {
		fprintf(stderr,
			"Warning: length not supplied for document %ld\n",
			rr_vec.rr[i].did);
		continue;
	    }
	    docs_ptr->nqr++;
	    num_ret++;
	}
    }

    /* sort by length */
    qsort((char *)docs, num_docs, sizeof(DOC_DATA), comp_len);

    /* decrement bin size to get a decent sized last bin */
    while(num_docs%final_bin_size < (long) (tolerance*final_bin_size) &&
          num_docs%final_bin_size != 0)
        final_bin_size--;

    fprintf(stdout, "Bin Size = %ld\n", final_bin_size);

    for (bin = i = 0; i < num_docs; bin++) {
	long from = final_bin_size*bin;
	long to = final_bin_size*(bin+1);
	long sum = 0;

	for (i = from; i < to && i < num_docs; i++)
	    sum += docs[i].nqr;
	    
	fprintf(out_fd, "%ld\t%12.10f\n", docs[(i+from)/2].len,
		((float) sum*final_bin_size)/((i-from)*num_ret));
    }

    if (UNDEF == close_rr_vec (rr_fd))
        return (UNDEF);

    if (VALID_FILE (text_file))
        (void) fclose (out_fd);

    PRINT_TRACE (2, print_string, "Trace: leaving rr_prob_obj");
    return (1);
}

int
close_rr_prob_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string,
                 "Trace: entering close_rr_prob_obj");

    (void) free((char *) docs);

    PRINT_TRACE (2, print_string,
                 "Trace: leaving close_rr_prob_obj");
    return (0);
}

static int comp_len(d1, d2)
DOC_DATA *d1;
DOC_DATA *d2;
{
    return (d1->len - d2->len);
}


static int comp_did(d1, d2)
DOC_DATA *d1;
DOC_DATA *d2;
{
    return (d1->did - d2->did);
}
