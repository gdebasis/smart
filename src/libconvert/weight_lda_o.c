#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/weight.c,v 11.2 1993/02/03 16:49:33 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Object level conversion routine to reweight a document vector object
 *1 convert.obj.weight_lda_doc

 *2 weight_lda_doc (in_file, out_file, inst)
 *2   char *in_file;
 *2   char *out_file;
 *2   int inst;

 *4 init_weight_lda_doc (spec, unused)
 *5   "convert.in.rmode"
 *5   "convert.out.rwmode"
 *5   "convert.weight.trace"
 *5   "convert.doc.weight"
 *4 close_weight_lda_doc (inst)

 *6 global_context set to indicate weighting document (CTXT_DOC)
 *7 For every document in in_file, calls weighting procedure given by 
 *7 doc.weight to reweight it, and then write it to out_file.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "io.h"
#include "functions.h"
#include "spec.h"
#include "docindex.h"
#include "docdesc.h"
#include "vector.h"
#include "trace.h"
#include "context.h"
#include "vector.h"
#include "tr_vec.h"
#include "lda.h"

/* Convert tf weighted vector file to specified weighting scheme
*/

static long in_mode;
static long out_mode;
static PROC_INST query_func;

//int init_lda_est_inst_coll (LDAModel* ip, int doc_fd);

static char *nnn_vec_file;
static long nnn_vec_file_mode;
//static int numDocs;

static SPEC_PARAM spec_args[] = {
    {"weight_lda.nnn_doc_file",    getspec_dbfile, (char *) &nnn_vec_file},
    {"weight_lda.nnn_doc_file.rmode",getspec_filemode, (char *) &nnn_vec_file_mode},
    {"weight_lda.in.rmode",     getspec_filemode,  (char *) &in_mode},
    {"weight_lda.out.rwmode",   getspec_filemode,  (char *) &out_mode},
   	//{"lda_est.num_docs",   getspec_int,  (char *) &numDocs},
    TRACE_PARAM ("weight_lda.trace")
    };
static int num_spec_args =
    sizeof (spec_args) / sizeof (spec_args[0]);

static SPEC *save_spec;

typedef struct {
	SPEC* spec;
    int nnn_vec_fd;
	LDAModel ldamodel;
} STATIC_INFO;

static STATIC_INFO ip;

int
init_weight_lda_doc (spec, unused)
SPEC *spec;
char *unused;
{
    PRINT_TRACE (2, print_string, "Trace: entering init_weight_lda_doc");
    
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    save_spec = spec;
	ip.spec = spec;

	if (UNDEF == init_lda_est(&ip.ldamodel, ip.spec)) {
		return UNDEF;
	}

    PRINT_TRACE (2, print_string, "Trace: leaving init_weight_lda_doc");
    return (0);
}        

int
weight_lda_doc (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    CONTEXT old_context;
    VEC invec, outvec;
    int orig_fd, new_fd;
	int numDocs = 0;

    PRINT_TRACE (2, print_string, "Trace: entering weight_lda_doc");

    old_context = get_context();
    set_context (CTXT_DOC);
    
    if (UNDEF == (ip.nnn_vec_fd = open_vector (nnn_vec_file, nnn_vec_file_mode)))
	    return (UNDEF);

    /* Call all initialization procedures */
    if (UNDEF == init_lang_model_wt_lm_lda(save_spec))
        return (UNDEF);
    if (UNDEF == (orig_fd = open_vector (in_file, in_mode)))
        return (UNDEF);
    if (UNDEF == (new_fd = open_vector (out_file,
                                        out_mode|SCREATE)))
        return (UNDEF);

    while (1 == read_vector (ip.nnn_vec_fd, &invec)) {
	    numDocs++;
    }
    // rewind
	invec.id_num.id = 1;
	if (UNDEF == seek_vector (ip.nnn_vec_fd, &invec))
		return UNDEF;
  
	// Add documents to LDA model
	// Note: We need the nnn doc file to calculate document lengths
	// WARNING: Program will crash if the vector lengths are unequal 
	if (UNDEF == init_lda_est_inst_coll(&(ip.ldamodel), ip.nnn_vec_fd, numDocs))
		return UNDEF;
	if (UNDEF == lda_est(&ip.ldamodel))
		return UNDEF;

    /* Get each document in turn */
    while (1 == read_vector (orig_fd, &invec)) {
        if (UNDEF == lang_model_wt_lm_lda (&invec, &outvec, &ip.ldamodel)) {
            return (UNDEF);
        }
        if (UNDEF == seek_vector (new_fd, &outvec) ||
            UNDEF == write_vector (new_fd, &outvec)) {
            return (UNDEF);
        }
    }

    /* Close weighting procs */
    if (UNDEF == close_lang_model_wt_lm_lda (new_fd /*unused*/) ||
        UNDEF == close_vector (orig_fd) ||
        UNDEF == close_vector (new_fd)) {
        return (UNDEF);
    }

    set_context (old_context);

    PRINT_TRACE (2, print_string, "Trace: leaving weight_lda_doc");

    return (1);
}

int
close_weight_lda_doc (inst)
int inst;
{
    PRINT_TRACE (2,print_string, "Trace: entering/leaving close_weight_lda_doc");

	close_lda_est(&ip.ldamodel);
    if (UNDEF == close_vector (ip.nnn_vec_fd))
		return (UNDEF);

    return (0);
}

