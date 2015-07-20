#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/feedback.c,v 11.2 1993/02/03 16:49:59 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "vector.h"
#include "feedback.h"
#include "proc.h"
#include "trace.h"
#include "spec.h"
#include "retrieve.h"

static PROC_INST expand,
    occ_info,
    weight,
    form;

static SPEC_PARAM spec_args[] = {
    {"feedback.expand",          getspec_func, (char *) &expand.ptab},
    {"feedback.occ_info",        getspec_func, (char *) &occ_info.ptab},
    {"feedback.weight",          getspec_func, (char *) &weight.ptab},
    {"feedback.form",            getspec_func, (char *) &form.ptab},
    TRACE_PARAM ("feedback.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static FEEDBACK_INFO fdbk_info;

int
init_feedback (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_feedback");

    /* Call all initialization procedures */
    if (UNDEF == (expand.inst = expand.ptab->init_proc (spec, NULL)) ||
        UNDEF == (occ_info.inst = occ_info.ptab->init_proc (spec, NULL)) ||
        UNDEF == (weight.inst = weight.ptab->init_proc (spec, NULL)) ||
        UNDEF == (form.inst = form.ptab->init_proc (spec, NULL))) {
        return (UNDEF);
    }

    fdbk_info.num_occ = 0;
    fdbk_info.max_occ = 0;
    fdbk_info.occ = NULL;

    PRINT_TRACE (2, print_string, "Trace: leaving init_feedback");
    return (0);
}

/* Construct feedback query corresponding to each input feedback query, */
/* using the original query plus information about a sample of relevant */
/* and non-relevant documents (plus collection information) */
    
int 
feedback (retrieval, new_query, inst)
RETRIEVAL *retrieval;
QUERY_VECTOR *new_query;
int inst;
{
#ifdef DEBUG2
    if (0 == malloc_verify()) {
        abort();
    }
    else {
        printf ("feedback: malloc heap OK upon entering\n");
        fflush (stdout);
    }
#endif // DEBUG2

    PRINT_TRACE (2, print_string, "Trace: entering feedback");

    fdbk_info.orig_query = (VEC *) retrieval->query->vector;
    fdbk_info.tr = retrieval->tr;
    new_query->qid = fdbk_info.orig_query->id_num.id;
    if (fdbk_info.tr->num_tr == 0) {
        /* original query retrieved no docs, just return it as new query */
        new_query->vector = (char *) fdbk_info.orig_query;
        return (0);
    }
    
    /* Construct a list of terms occurring in the query and/or the
     * relevant documents. If rel docs are gotten, then the following 
     * fields of occ are set: con, ctype, query, rel_ret, orig_weight,
     * rel_weight (set to sum of weights in rel docs, may be changed later).
     * All other fields are set to 0.
     * Sort by ctype,con
     */
    if (UNDEF == expand.ptab->proc (retrieval, &fdbk_info, expand.inst))
        return (UNDEF);

#ifdef DEBUG2
    if (0 == malloc_verify()) {
        abort();
    }
    else {
        printf ("feedback: malloc heap OK upon expansion\n");
        fflush (stdout);
    }
#endif // DEBUG2
    /* Get other occurrence information for all terms.  This may or may not */
    /* include info about occurrence in retrieved relevant docs, */
    /* retrieved non-rel docs, non-retrieved docs, etc. */
    if (UNDEF == occ_info.ptab->proc (NULL, &fdbk_info, occ_info.inst))
        return (UNDEF);

    /* compute a weight for each term on the list.  This weight will */
    /* depend on occurrence info and may depend on whether term occurs */
    /* in the query. */
    if (UNDEF == weight.ptab->proc (NULL, &fdbk_info, weight.inst))
        return (UNDEF);

    /* Form a new query from the old query and new query weights */
    /* Possibly delete terms from list if they have "insufficient" weight */
    if (UNDEF == form.ptab->proc (&fdbk_info, new_query, form.inst))
        return (UNDEF);

#ifdef DEBUG2
    if (0 == malloc_verify()) {
        abort();
    }
    else {
        printf ("feedback: malloc heap OK upon leaving\n");
        fflush (stdout);
    }
#endif // DEBUG2

    PRINT_TRACE (4, print_vector, (VEC *) new_query->vector);
    PRINT_TRACE (2, print_string, "Trace: leaving feedback");
    return (1);
}

int
close_feedback (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_feedback");

    /* Call all close procedures */
    if (UNDEF == expand.ptab->close_proc (expand.inst) ||
        UNDEF == occ_info.ptab->close_proc (occ_info.inst) ||
        UNDEF == weight.ptab->close_proc (weight.inst) ||
        UNDEF == form.ptab->close_proc (form.inst)) {
        return (UNDEF);
    }
    
    if (fdbk_info.max_occ > 0)
        free ((char *) fdbk_info.occ);

    fdbk_info.num_occ = 0;
    fdbk_info.max_occ = 0;
    fdbk_info.occ = NULL;

    PRINT_TRACE (2, print_string, "Trace: leaving close_feedback");
    return (0);
}
