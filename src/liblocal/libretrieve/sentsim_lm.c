#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/sentsim_lm.c,v 11.2 1993/02/03 16:51:11 smart Exp $";
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

static PROC_TAB *vec_vec_ptab;
static PROC_INST vec_vec;
static int vec_vec_inst;
static int lm_prior;

static SPEC_PARAM spec_args[] = {
    {"sentsim_lm.vecs_vecs.vec_vec", getspec_func, (char *) &vec_vec_ptab},
    {"sentsim_lm.lm_prior", getspec_int, (char *) &lm_prior},
    TRACE_PARAM ("convert.sentsim_lm.trace")
};
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static float getDocumentLength(VEC* vec) {
	int i;
	float length = 0 ;
	CON_WT* start = vec->con_wtp ;

	// Read ctype_len[ctype] weights and add them up.
	for (i = 0; i < vec->num_conwt; i++) {
		length += start[i].wt ;
	}

	return length ;
}

int
init_sentsim_lm (spec, prefix)
SPEC *spec;
char *prefix;
{
    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    PRINT_TRACE (2, print_string, "Trace: entering init_sentsim_lm");

    if (UNDEF == (vec_vec_inst = vec_vec_ptab->init_proc (spec, NULL)))
	    return (UNDEF);

	// initalize the LM conversion module
	if (UNDEF == init_lang_model_wt_lm(spec, NULL))
		return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving init_sentsim_lm");
    return 0;
}

int sentsim_lm (Sentence* sentences, int numSentences, VEC* qvec, TR_VEC* unused)
{
    int i, j;
	VEC* vec, lmvec;
	VEC_PAIR  vec_pair;

    PRINT_TRACE (2, print_string, "Trace: entering sentsim_lm");

	// Fill up the LM similarity values.
	for (i = 0; i < numSentences; i++) {
		vec = &(sentences[i].svec);

		/* Convert the raw term frequency vectors to LM weights */
		if (UNDEF == lang_model_wt_lm(vec, &lmvec, 0))
			return UNDEF;
		PRINT_TRACE(4, print_vector, vec); 
		PRINT_TRACE(4, print_vector, &lmvec); 
		vec_pair.vec1 = &lmvec;
		vec_pair.vec2 = qvec;
        if (UNDEF == vec_vec_ptab->proc (&vec_pair, &(sentences[i].sim), vec_vec_inst))
	        return (UNDEF);
		sentences[i].sim += lm_prior * log(getDocumentLength(vec));
	}

    PRINT_TRACE (2, print_string, "Trace: leaving sentsim_lm");
    return (1);
}

int
close_sentsim_lm ()
{
    PRINT_TRACE (2, print_string, "Trace: entering close_sentsim_lm");

   	if (UNDEF == close_lang_model_wt_lm(0))
		return UNDEF;
    if (UNDEF == vec_vec_ptab->close_proc (vec_vec_inst))
	    return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_sentsim_lm");
    return (0);
}

