#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/weight_tri.c,v 11.2 1993/02/03 16:49:37 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector using three part weighting scheme
 *1 convert.wt_ctype.weight_tri
 *2 weight_tri (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;
 *4 init_weight_tri (spec, param_prefix)
 *5   "convert.weight.trace"
 *5   "*.query_weight"
 *5   "*.doc_weight"
 *5   "*.[query_]sect_weight"
 *5   "*.[query_]para_weight"
 *5   "*.[query_]sent_weight"
 *4 close_weight_tri (inst)

 *6 global_context used to distinguish if indexing query (CTXT_QUERY)
 *6 or document (CTXT_DOC) or part of a text (CTXT_SENT,CTXT_PARA,CTXT_SECT).
 *6 Note that CTXT_QUERY can be OR'd with the part values.

 *7 Weight vector according the value of the parameter given by 
 *7 the concatenation of param_prefix and either "doc_weight" or
 *7 "query weight".  
 *7 Normally, param_prefix will have some value like "ctype.1." in order
 *7 to obtain the ctype dependant weight.  Ie. "ctype.1.doc_weight"
 *7 will be looked up in the specifications to find the correct weight
 *7 to use for weighting docs.
 *7 The value of the parameter is a 3 character code (eg "atc"):
 *7     First char gives the term-freq procedure to be used
 *7     Second char gives the inverted-doc-freq procedure to be used.
 *7     Third char gives the normalization procedure to be used.
 *7 The entire vector is weighted according to the value; it is assumed
 *7 that the input vector is actually just one ctype (that corresponding to
 *7 param_prefix).
 *7 UNDEF returned if error, else 0.
 *7 
 *7 Sample Routines:
 *7 (These may or may not exist.  See the appropriate hierarchy table for
 *7 what is actually implemetnted at the moment.)
 *7  Sample routines for converting a term-freq weighted vector into a vector
 *7  with the specified weighting scheme. There are three possible conversion
 *7  that can be performed on each vector:
 *7       1. Normalize term_freq component - most often the tf component is
 *7                   altered by dividing by the max tf in the vector
 *7       2. Alter the doc weight, possibly based on collection freq info.
 *7                   Note that this is done individually on each term
 *7       3. Normalize the entire subvector - most often to "sum of squares" of
 *7                   terms = 1. Alternative is sum of terms = 1
 *7 
 *7  Weighting schemes and the desired weight_type parameter
 *7  Parameters are specified by the first character of the incoming string
 *7       1.  "none"     : new_tf = tf
 *7                        No conversion to be done  1 <= new_tf
 *7           "binary"   : new_tf = 1
 *7           "max_norm" : new_tf = tf / max_tf
 *7                        divide each term by max in vector  0 < new_tf < 1.0
 *7           "aug_norm" : new_tf = 0.5 + 0.5 * (tf / max_tf)
 *7                        augmented normalized tf.  0.5 < new_tf <= 1.0
 *7           "square"   : new_tf = tf * tf
 *7           "log"      : new_tf = ln (tf) + 1.0
 *7 
 *7       2.  "none"     : new_wt = new_tf
 *7                        No conversion is to be done
 *7           "tfidf"    : new_wt = new_tf * log (num_docs/coll_freq_of_term)
 *7                        Usual tfidf weight (Note: Pure idf if new_tf = 1)
 *7           "prob"     : new_wt = new_tf * log ((num_docs - coll_freq)
 *7                                                / coll_freq))
 *7                        Straight probabilistic weighting scheme
 *7           "freq"     : new_wt = new_tf / n
 *7           "squared"  : new_wt = new_tf * log(num_docs/coll_freq_of_term)**2
 *7 
 *7       3.  "none"     : norm_weight = new_wt
 *7                        No normalization done
 *7           "sum"      : divide each new_wt by sum of new_wts in vector
 *7           "cosine"   : divide each new_wt by sqrt (sum of(new_wts squared))
 *7                        This is the usual cosine normalization (I.e. an
 *7                        inner product function of two cosine normalized
 *7                        vectors will yield the same results as a cosine
 *7                        function on vectors (either normalized or not))
 *7           "fourth"   : divide each new_wt by sum of (new_wts ** 4)
 *7           "max"      : divide each new_wt by max new_wt in vector

 *8 Just calls the procedures associated with the given hierarchy names.
 *8 Eg. if weight is atc, then the procs called are those with hierarchy names
 *8    "convert.wt_tf.a"
 *8    "convert.wt_idf.t"
 *8    "convert.wt_norm.c"
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "docdesc.h"
#include "vector.h"
#include "docindex.h"
#include "trace.h"
#include "context.h"
#include "inst.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("convert.weight.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    PROC_INST tf;
    PROC_INST idf;
    PROC_INST norm;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_weight_tri (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    SPEC_PARAM weight_param;
    char *weight;
    int new_inst;
    STATIC_INFO *ip;
    char param[PATH_LEN];

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }
    PRINT_TRACE (2, print_string, "Trace: entering init_weight_tri");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    ip = &info[new_inst];

    /* Lookup the weight desired for this ctype */
    if (param_prefix != NULL) 
        (void) strcpy (param, param_prefix);
    else
        param[0] = '\0';

    if (check_context (CTXT_SENT)) {
	if (check_context (CTXT_QUERY))
	    (void) strcat (param, "query_sent_weight");
	else
	    (void) strcat (param, "sent_weight");
    }
    else if (check_context (CTXT_PARA)) {
	if (check_context (CTXT_QUERY))
	    (void) strcat (param, "query_para_weight");
	else
	    (void) strcat (param, "para_weight");
    }
    else if (check_context (CTXT_SECT)) {
	if (check_context (CTXT_QUERY))
	    (void) strcat (param, "query_sect_weight");
	else
	    (void) strcat (param, "sect_weight");
    }
    else if (check_context (CTXT_QUERY))
        (void) strcat (param, "query_weight");
    else if (check_context (CTXT_DOC))
        (void) strcat (param, "doc_weight");
    else
        (void) strcat (param, "doc_weight");
    weight_param.param = param;
    weight_param.convert = getspec_string;
    weight_param.result = (char *) &weight;
    if (UNDEF == lookup_spec (spec, &weight_param, 1))
        return (UNDEF);

    /* Establish which weighting methods will be used for each weight types */
    (void) sprintf (param, "convert.wt_tf.%c", weight[0]);
    if (UNDEF == (getspec_func (spec, (char *) NULL, param, &ip->tf.ptab))) {
        clr_err();
        (void) sprintf (param, "local.convert.wt_tf.%c", weight[0]);
        if (UNDEF == (getspec_func (spec, (char *) NULL,
                                    param, &ip->tf.ptab)))
            return (UNDEF);
    }
    (void) sprintf (param, "convert.wt_idf.%c", weight[1]);
    if (UNDEF == (getspec_func (spec, (char *) NULL, param, &ip->idf.ptab)))  {
        clr_err();
        (void) sprintf (param, "local.convert.wt_idf.%c", weight[1]);
        if (UNDEF == (getspec_func (spec, (char *) NULL,
                                    param, &ip->idf.ptab)))
            return (UNDEF);
    }
    (void) sprintf (param, "convert.wt_norm.%c", weight[2]);
    if (UNDEF == (getspec_func (spec, (char *) NULL, param, &ip->norm.ptab))) {
        clr_err();
        (void) sprintf (param, "local.convert.wt_norm.%c", weight[2]);
        if (UNDEF == (getspec_func (spec, (char *) NULL,
                                    param, &ip->norm.ptab)))
            return (UNDEF);
    }

    /* Initialize component weighting procedures */
    if (UNDEF == (ip->tf.inst = ip->tf.ptab->init_proc(spec,param_prefix)) ||
        UNDEF == (ip->idf.inst = ip->idf.ptab->init_proc(spec,param_prefix)) ||
        UNDEF == (ip->norm.inst=ip->norm.ptab->init_proc(spec,param_prefix))) {
        return (UNDEF);
    }
    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_weight_tri");

    return (new_inst);
}

int
weight_tri (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering weight_tri");
    PRINT_TRACE (6, print_vector, vec);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "weight_tri");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Call the component weighting method on vec */
    if (UNDEF == ip->tf.ptab->proc (vec, (char *) NULL, ip->tf.inst) ||
        UNDEF == ip->idf.ptab->proc (vec, (char *) NULL, ip->idf.inst) ||
        UNDEF == ip->norm.ptab->proc (vec, (char *) NULL, ip->norm.inst))
        return (UNDEF);

    PRINT_TRACE (4, print_vector, vec);
    PRINT_TRACE (2, print_string, "Trace: leaving weight_tri");
    return (1);
}


int
close_weight_tri (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_weight_tri");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_weight_tri");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Close weighting procs and free buffers if this was last close of inst */
    ip->valid_info--;
    if (ip->valid_info == 0) {
        if (UNDEF == ip->tf.ptab->close_proc (ip->tf.inst) ||
            UNDEF == ip->idf.ptab->close_proc (ip->idf.inst) ||
            UNDEF == ip->norm.ptab->close_proc (ip->norm.inst))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_weight_tri");

    return (0);
}



