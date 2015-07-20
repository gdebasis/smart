#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/weights_idf.c,v 11.2 1993/02/03 16:49:38 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by multiplying weight by normal idf factor
 *1 convert.wt_idf.t
 *1 convert.wt_idf.i
 *2 idfwt_idf (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;
 *4 init_inv_idf_weight (spec, param_prefix)
 *5   "convert.weight.trace"
 *5   "doc.textloc_file"
 *5   "*.inv_file"
 *5   "*.inv_file.rmode"

 *7 For each term in vector, multiply its existing weight by log (N / n).
 *7 N is the number of docs in the collection, determined from the number
 *7 of entries in "textloc_file".  n is the number of docs in which the term
 *7 occurs, determined by the number of entries in the inverted list for
 *7 the term (from "inv_file").
 *7 Note that "inv_file" is a ctype dependant parameter, and is found by
 *7 looking up the value of the parameter formed by concatenating 
 *7 param_prefix and "inv_file".
 *8 The idf values for a particular inv_file are cached to avoid excessive
 *8 references to the inverted file.
 *9 Need to generalize.
 *9 Should replace references to inverted file with a ctype-dependant
 *9 procedure.
***********************************************************************/


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by multiplying weight by normal idf factor sqared
 *1 convert.wt_idf.s
 *2 idfwt_s_idf (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;
 *4 init_inv_idf_weight (spec, param_prefix)
 *5   "convert.weight.trace"
 *5   "doc.textloc_file"
 *5   "*.inv_file"
 *5   "*.inv_file.rmode"

 *7 For each term in vector, multiply its existing weight by (log(N/n)) ** 2.
 *7 N is the number of docs in the collection, determined from the number
 *7 of entries in "textloc_file".  n is the number of docs in which the term
 *7 occurs, determined by the number of entries in the inverted list for
 *7 the term (from "inv_file").
 *7 Note that "inv_file" is a ctype dependant parameter, and is found by
 *7 looking up the value of the parameter formed by concatenating 
 *7 param_prefix and "inv_file".
 *8 The idf values for a particular inv_file are cached to avoid excessive
 *8 references to the inverted file.
 *9 Need to generalize.
 *9 Should replace references to inverted file with a ctype-dependant
 *9 procedure.
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by dividing weight by collection freq
 *1 convert.wt_idf.f
 *2 idfwt_freq (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;
 *4 init_inv_idf_weight (spec, param_prefix)
 *5   "convert.weight.trace"
 *5   "doc.textloc_file"
 *5   "*.inv_file"
 *5   "*.inv_file.rmode"

 *7 For each term in vector, multiply its existing weight by 1 / n.
 *7 n is the number of docs in which the term
 *7 occurs, determined by the number of entries in the inverted list for
 *7 the term (from "inv_file").
 *7 Note that "inv_file" is a ctype dependant parameter, and is found by
 *7 looking up the value of the parameter formed by concatenating 
 *7 param_prefix and "inv_file".
 *9 Need to generalize.
 *9 Should replace references to inverted file with a ctype-dependant
 *9 procedure.
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight phrase vector by multiplying weight by average of term idf factors
 *1 convert.wt_idf.P
 *2 idfwt_avphrase (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;
 *4 init_inv_idf_weight (spec, param_prefix)
 *5   "convert.weight.trace"
 *5   "doc.textloc_file"
 *5   "*.inv_file"
 *5   "*.inv_file.rmode"

 *7 For each term in vector (assumed to be a phrase), multiply its 
 *7 existing weight by the average of log (N / ni) for its component terms.
 *7 N is the number of docs in the collection, determined from the number
 *7 of entries in "textloc_file".  ni is the number of docs in which each term
 *7 occurs, determined by the number of entries in the inverted list for
 *7 the term (from "inv_file").
 *7 Note that "inv_file" is a ctype dependant parameter, and is found by
 *7 looking up the value of the parameter formed by concatenating 
 *7 param_prefix and "inv_file".
 *8 The idf values for a particular inv_file are cached to avoid excessive
 *8 references to the inverted file.
 *9 Need to generalize.
 *9 Should replace references to inverted file with a ctype-dependant
 *9 procedure.
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweighting procedure no-op for idf weights.
 *1 convert.wt_idf.x
 *1 convert.wt_idf.n

 *7 Do nothing.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "docindex.h"
#include "vector.h"
#include "inv.h"
#include "inst.h"

/* Routines for converting a term-freq weighted subvector into a subvector
* with the specified weighting scheme. There are three possible conversion
* that can be performed on each subvector:
*      2. Alter the doc weight, possibly based on collection freq info.
*                  Note that this is done individually on each term

* Weighting schemes and the desired weight_type parameter
* Parameters are specified by the first character of the incoming string
*      2.  "none"     : new_wt = new_tf
*                       No conversion is to be done
*          "tfidf"    : new_wt = new_tf * log (num_docs/coll_freq_of_term)
*                       Usual tfidf weight (Note: Pure idf if new_tf = 1)
*          "prob"     : new_wt = new_tf * log ((num_docs - coll_freq)
*                                               / coll_freq))
*                       Straight probabilistic weighting scheme
*          "freq"     : new_wt = new_tf / n
*          "squared"  : new_wt = new_tf * log (num_docs/coll_freq_of_term)**2
*
*/

int
init_idfwt_idf (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    return (init_con_cw_idf (spec, param_prefix));
}

int
idfwt_idf (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    float idf;
    CON_WT *conwt = vec->con_wtp;

    for (i = 0; i < vec->num_conwt; i++) {
        if (UNDEF == con_cw_idf (&conwt->con, &idf, inst))
            return (UNDEF);
        conwt->wt *= idf;
        conwt++;
    }
    return (1);
}

int
close_idfwt_idf (inst)
int inst;
{
    return (close_con_cw_idf (inst));
}


int
init_idfwt_freq (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    return (init_con_cw_cf (spec, param_prefix));
}

int
idfwt_freq (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    float freq;
    CON_WT *conwt = vec->con_wtp;

    for (i = 0; i < vec->num_conwt; i++) {
        if (UNDEF == con_cw_cf (&conwt->con, &freq, inst))
            return (UNDEF);
        conwt->wt /= freq;
        conwt++;
    }
    return (1);
}

int
close_idfwt_freq (inst)
int inst;
{
    return (close_con_cw_cf (inst));
}


int
init_idfwt_s_idf (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    return (init_con_cw_idf (spec, param_prefix));
}

int
idfwt_s_idf (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    float idf;
    CON_WT *conwt = vec->con_wtp;

    for (i = 0; i < vec->num_conwt; i++) {
        if (UNDEF == con_cw_idf (&conwt->con, &idf, inst))
            return (UNDEF);
        conwt->wt *= idf * idf;
        conwt++;
    }
    return (1);
}

int
close_idfwt_s_idf (inst)
int inst;
{
    return (close_con_cw_idf (inst));
}
