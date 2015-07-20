#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/vec_vec_coocc.c,v 11.2 1993/02/03 16:54:46 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Calculate similarity between pair of vectors
 *1 retrieve.vec_vec.vec_vec
 *2 vec_vec_coocc (vec_pair, sim_ptr, inst)
 *3   VEC_PAIR *vec_pair;
 *3   float *sim_ptr;
 *3   int inst;
 *4 init_vec_vec_coocc (spec, unused)
 *5   "retrieve.vec_vec.trace"
 *5   "index.ctype.*.seq_sim"
 *4 close_vec_vec_coocc (inst)
 *7 Calculate the similarity between the two vectors (given by vec_pair)
 *7 by calling a ctype dependant similarity function given by
 *7 index.ctype.N.seq_sim for each ctype N occurring in the vectors.

 *9 Note: Assumes similarity is 0.0 between ctypes of the vectors if
 *9 that ctype is not included in one or both of the vectors.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "retrieve.h"
#include "vector.h"
#include "docdesc.h"

static PROC_TAB *tokcon;
static float ctype_weight;
static SPEC_PARAM spec_args[] = {
    {"ctype.2.sim_ctype_weight", getspec_float, (char *) &ctype_weight},
    {"retrieve.ctype.0.token_to_con", getspec_func, (char *) &tokcon},
    TRACE_PARAM ("retrieve.vec_vec.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static SM_INDEX_DOCDESC doc_desc;

static int *ctype_inst;
static int contok_inst, tokcon_inst;

static int num_inst = 0;
int init_contok_dict_noinfo(), contok_dict_noinfo(), close_contok_dict_noinfo();

int
init_vec_vec_coocc (spec, unused)
SPEC *spec;
char *unused;
{
    long i;
    char param_prefix[PATH_LEN];

    if (num_inst++ > 0) {
        /* Assume all instantiations equal */
        PRINT_TRACE (2, print_string,
                     "Trace: entering/leaving init_vec_vec_coocc");
        return (0);
    }

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_vec_vec_coocc");

    if (UNDEF == lookup_spec_docdesc (spec, &doc_desc))
        return (UNDEF);

    /* Reserve space for inst id of each of the ctype retrieval procs */
    if (NULL == (ctype_inst = (int *)
                 malloc ((unsigned) doc_desc.num_ctypes * sizeof (int))))
        return (UNDEF);
    
    /* Call all initialization procedures */
    for (i = 0; i < doc_desc.num_ctypes; i++) {
        (void) sprintf (param_prefix, "ctype.%ld.", i);
        if (UNDEF == (ctype_inst[i] = doc_desc.ctypes[i].seq_sim->
                      init_proc (spec, param_prefix)))
            return (UNDEF);
    }
    
    /* Call all initialization procedures */
    if (UNDEF == (contok_inst = init_contok_dict_noinfo(spec, "ctype.2.")) ||
        UNDEF == (tokcon_inst = tokcon->init_proc(spec, "index.section.word.")))
	return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving init_vec_vec_coocc");
    return (0);
}

int
vec_vec_coocc (vec_pair, sim_ptr, inst)
VEC_PAIR *vec_pair;
float *sim_ptr;
int inst;
{
    VEC *qvec = vec_pair->vec1;
    VEC *dvec = vec_pair->vec2;
    long i, j;
    VEC ctype_query;
    VEC ctype_doc;
    VEC_PAIR ctype_vec_pair;
    CON_WT *query_con_wtp;
    CON_WT *doc_con_wtp;
    float ctype_sim;
    char term_buf[PATH_LEN], *term_pair, *term1, *term2;
    long con1, con2;

    PRINT_TRACE (2, print_string, "Trace: entering vec_vec_coocc");
    PRINT_TRACE (6, print_vector, qvec);
    PRINT_TRACE (6, print_vector, dvec);

    /* Perform retrieval, sequentially going through query by ctype */
    ctype_query.id_num = qvec->id_num;
    ctype_query.num_ctype = 1;
    query_con_wtp = qvec->con_wtp;
    ctype_doc.id_num = dvec->id_num;
    ctype_doc.num_ctype = 1;
    doc_con_wtp = dvec->con_wtp;
    ctype_vec_pair.vec1 = &ctype_query;
    ctype_vec_pair.vec2 = &ctype_doc;
    *sim_ptr = 0.0;
    
    for (i = 0; i < doc_desc.num_ctypes; i++) {
	if (qvec->num_ctype <= i || dvec->num_ctype <= i)
	    break;
	if (qvec->ctype_len[i] <= 0 || dvec->ctype_len[i] <= 0) {
	    query_con_wtp += qvec->ctype_len[i];
	    doc_con_wtp += dvec->ctype_len[i];
	    continue;
	}
            
	ctype_query.ctype_len = &qvec->ctype_len[i];
	ctype_query.num_conwt = qvec->ctype_len[i];
	ctype_query.con_wtp = query_con_wtp;
	query_con_wtp += qvec->ctype_len[i];
	ctype_doc.ctype_len = &dvec->ctype_len[i];
	ctype_doc.num_conwt = dvec->ctype_len[i];
	ctype_doc.con_wtp = doc_con_wtp;
	doc_con_wtp += dvec->ctype_len[i];
	if (UNDEF == doc_desc.ctypes[i].seq_sim->
	    proc (&ctype_vec_pair, &ctype_sim, ctype_inst[i]))
	    return (UNDEF);
	*sim_ptr += ctype_sim;
    }

    for (i = 0; i < qvec->ctype_len[2]; i++, query_con_wtp++) {
	CON_WT *ptr1 = NULL, *ptr2 = NULL;

	/* Get the term-pair corresponding to this ctype 2 term */
        if (UNDEF == contok_dict_noinfo(&(query_con_wtp->con), &term_pair, 
                                        contok_inst))
            return(UNDEF);
        strcpy(term_buf, term_pair);
        term2 = term1 = term_buf;
        while (*term2 != ' ') term2++;
        *term2 = '\0';
        term2++;
        if (UNDEF == tokcon->proc(term1, &con1, tokcon_inst) ||
            UNDEF == tokcon->proc(term2, &con2, tokcon_inst))
            return(UNDEF);

	for (j = 0; j < dvec->ctype_len[0]; j++) {
	    if (dvec->con_wtp[j].con == con1)
		ptr1 = &dvec->con_wtp[j];
	    if (dvec->con_wtp[j].con == con2)
		ptr2 = &dvec->con_wtp[j];
	}

	if (ptr1 != NULL && ptr2 != NULL)
	    *sim_ptr += query_con_wtp->wt * MIN(ptr1->wt, ptr2->wt)
		* ctype_weight;
    }

    PRINT_TRACE (4, print_float, sim_ptr);
    PRINT_TRACE (2, print_string, "Trace: leaving vec_vec_coocc");
    return (1);
}


int
close_vec_vec_coocc (inst)
int inst;
{
    long i;
 
    if (--num_inst) {
        PRINT_TRACE (2, print_string,
                     "Trace: entering/leaving close_vec_vec_coocc");
        return (0);
    }

    PRINT_TRACE (2, print_string, "Trace: entering close_vec_vec_coocc");

    for (i = 0; i < doc_desc.num_ctypes; i++) {
        if (UNDEF == (doc_desc.ctypes[i].seq_sim->close_proc (ctype_inst[i])))
            return (UNDEF);
        if (UNDEF == close_contok_dict_noinfo(contok_inst) ||
            UNDEF == tokcon->close_proc(tokcon_inst))
            return (UNDEF);
    }

    (void) free ((char *) ctype_inst);

    PRINT_TRACE (2, print_string, "Trace: leaving close_vec_vec_coocc");
    return (0);
}
