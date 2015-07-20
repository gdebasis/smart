#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/weights_idf.c,v 11.2 1993/02/03 16:49:38 smart Exp $";
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
#include "inst.h"

static float idf_power;
static SPEC_PARAM spec_args[] = {
    {"convert.weight_idf.idf_power", getspec_float, (char *) &idf_power},
    TRACE_PARAM ("convert.weight.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static float idf_slope;
static float idf_half;
static float idf_max;
static SPEC_PARAM bspec_args[] = {
    {"convert.weight_idf.idf_slope", getspec_float, (char *) &idf_slope},
    {"convert.weight_idf.idf_half", getspec_float, (char *) &idf_half},
    {"convert.weight_idf.idf_max", getspec_float, (char *) &idf_max},
    TRACE_PARAM ("convert.weight.trace")
    };
static int num_bspec_args = sizeof (bspec_args) / sizeof (bspec_args[0]);

static char *textloc_file;
static char *doc_file;
static SPEC_PARAM nspec_args[] = {
    {"doc.textloc_file",	getspec_dbfile,	(char *) &textloc_file},
    {"doc.doc_file",		getspec_dbfile,	(char *) &doc_file},
    TRACE_PARAM ("convert.weight.trace")
};
static int num_nspec_args = sizeof (nspec_args) / sizeof (nspec_args[0]);

static double log_num_docs;


int
init_idfwt_IDF (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    return (init_con_cw_idf (spec, param_prefix));
}

int
idfwt_IDF (vec, unused, inst)
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
        conwt->wt *= (float) pow((double) idf, (double) idf_power);
        conwt++;
    }
    return (1);
}

int
close_idfwt_IDF (inst)
int inst;
{
    return (close_con_cw_idf (inst));
}

/******************************************************************/

int
init_idfwt_BIDF (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &bspec_args[0],
                              num_bspec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string,
                 "Trace: entering/leaving init_idfwt_BIDF");

    return(init_con_cw_cf (spec, param_prefix));
}

int
idfwt_BIDF (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    float freq, idf;
    CON_WT *conwt = vec->con_wtp;

    PRINT_TRACE (2, print_string, "Trace: entering idfwt_BIDF");

    for (i = 0; i < vec->num_conwt; i++) {
        if (UNDEF == con_cw_cf (&conwt->con, &freq, inst))
            return (UNDEF);
        idf = (float) (idf_max-atan((double) idf_slope*(freq-idf_half))/M_PI);
        conwt->wt *= idf;
        conwt++;
    }

    PRINT_TRACE (2, print_string, "Trace: leaving idfwt_BIDF");

    return (1);
}

int
close_idfwt_BIDF (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering/leaving close_idfwt_BIDF");

    return (close_con_cw_cf (inst));
}


int
init_idfwt_ictf (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    return (init_con_cw_ictf (spec, param_prefix));
}

int
idfwt_ictf (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    float idf;
    CON_WT *conwt = vec->con_wtp;

    for (i = 0; i < vec->num_conwt; i++) {
        if (UNDEF == con_cw_ictf (&conwt->con, &idf, inst))
            return (UNDEF);
        conwt->wt *= idf;
        conwt++;
    }
    return (1);
}

int
close_idfwt_ictf (inst)
int inst;
{
    return (close_con_cw_ictf (inst));
}


/*
 * idfwt_normidf: idf factor = log(N/df) / log(N).
 * 
 * Using this formula ensures that the idf factor is in [0,1].
 * Used in conjunction with tf factor M and normalization factor U.
 * Using this combination (MNU) retains the goodness of L(.)u schemes
 * while ensuring that all term weights are in [0,1].
 */
int
init_idfwt_normidf(spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    REL_HEADER *rh;

    if (UNDEF == lookup_spec(spec, &nspec_args[0], num_nspec_args))
        return (UNDEF);

    if (VALID_FILE (textloc_file)) {
        if (NULL == (rh = get_rel_header (textloc_file))) {
            set_error (SM_ILLPA_ERR, textloc_file, "init_idfwt_normidf");
            return (UNDEF);
        }
    }
    else if (NULL == (rh = get_rel_header (doc_file))) {
        set_error (SM_ILLPA_ERR, textloc_file, "init_idfwt_normidf");
        return (UNDEF);
    }
    log_num_docs = log((double) (rh->num_entries + 1));
    return (init_con_cw_idf (spec, param_prefix));
}

int
idfwt_normidf(vec, unused, inst)
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
        conwt->wt *= idf/log_num_docs;
        conwt++;
    }
    return (1);
}

int
close_idfwt_normidf(inst)
int inst;
{
    return (close_con_cw_idf (inst));
}

/******************************************************************/

int
idfwt_sqrtidf(vec, unused, inst)
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
        conwt->wt *= (float) sqrt((double) idf);
        conwt++;
    }
    return (1);
}

/******************************************************************
 * Used for weighting syntactic phrases. 
 * A phrase's idf weight is the maximum of the idf weights of its
 * constituent single terms.
 ******************************************************************/

/* static PROC_TAB *tokcon; */
/* static SPEC_PARAM mspec_args[] = { */
/*     {"convert.weight_idf.token_to_con", getspec_func, (char *) &tokcon}, */
/* }; */
/* static int num_mspec_args = sizeof (mspec_args) / sizeof (mspec_args[0]); */

/* static int t_inst, sp_inst; */

/* int */
/* init_idfwt_max(spec, param_prefix) */
/* SPEC *spec; */
/* char *param_prefix; */
/* { */
/*     int inst; */

/*     if (UNDEF == lookup_spec (spec, &mspec_args[0], num_mspec_args)) */
/*         return (UNDEF); */

/*     PRINT_TRACE (2, print_string, "Trace: entering init_idfwt_max"); */

/*     if (UNDEF == (t_inst = tokcon->init_proc(spec, "word.")) || */
/* 	UNDEF == (sp_inst = init_tokcon_dict_niss(spec, "synphr."))) */
/* 	return(UNDEF); */
/*     inst = init_con_cw_idf (spec, param_prefix); */

/*     PRINT_TRACE (2, print_string, "Trace: leaving init_idfwt_max"); */
/*     return inst; */
/* } */

/* int */
/* idfwt_max(vec, unused, inst) */
/* VEC *vec; */
/* char *unused; */
/* int inst; */
/* { */
/*     char  */
/*     long i; */

/*     for (i = 0; i < vec->num_conwt; i++) { */
/* 	get token for this phrase */
/* 	get concept number for each token */
/*         get idf for each token */
/*         if (UNDEF == con_cw_idf (&conwt->con, &idf, inst)) */
/*             return (UNDEF); */
/*         conwt->wt *= (float) pow((double) idf, (double) idf_power); */
/*         conwt++; */
/*     } */
/* } */

/* int */
/* close_idfwt_max (inst) */
/* int inst; */
/* { */
/*     if (UNDEF == tokcon->close_proc(t_inst) || */
/* 	UNDEF == close_tokcon_dict_niss(sp_inst)) */
/* 	return(UNDEF); */
/*     return (close_con_cw_idf (inst)); */
/* } */
