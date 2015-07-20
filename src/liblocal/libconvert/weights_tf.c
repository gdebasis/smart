#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/weights_tf.c,v 11.2 1993/02/03 16:49:38 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by square for f(tf)/f(avg-tf) weighting.
 *1 convert.wt_tf.S
 *2 tfwt_SQUARE (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, square(weight) / square(average weight).
 *7 d(weight)/d(tf) = tf
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweighting procedure no-op for f(tf)/f(avg-tf) weighting.
 *1 convert.wt_tf.X
 *1 convert.wt_tf.N
 *2 tfwt_NONE (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, weight / average weight
 *7 d(weight)/d(tf) = 1
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by logrithmic f(tf)/f(avg-tf) weighting.
 *1 convert.wt_tf.L
 *2 tfwt_LOG (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, set the new weight to 
 *7   ln (weight) + 1.0
 *7 An attempt to reduce the importance of tf in those collections with
 *7 wildly varying document length.
 *7 d(weight)/d(tf) = 1/tf
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by hyperbolic f(tf)/f(avg-tf) weighting.
 *1 convert.wt_tf.H
 *2 tfwt_HYP (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, set the new weight to 
 *7   (2.0 - 1.0/weight)/(2.0 - 1.0/average-weight)
 *7 An stronger attempt to reduce the importance of tf in collections
 *7 with wildly varying document length.
 *7 d(weight)/d(tf) = 1/tf^2
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by okapi's f(tf)/f(avg-tf) weighting.
 *1 convert.wt_tf.O
 *2 tfwt_OKAPI (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, set the new weight to 
 *7   (weight/(2.0+weight))/(average-weight/(2.0+average-weight))
 *7 Another attempt to reduce the importance of tf in collections with
 *7 wildly varying document length.
 *7 d(weight)/d(tf) = 2/(tf^2+4*tf+4)
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by extremely-hyperbolic f(tf)/f(avg-tf) weighting.
 *1 convert.wt_tf.E
 *2 tfwt_EX_HYP (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, set the new weight to 
 *7   (2.0 - 1.0/weight^2)/(2.0 - 1.0/average-weight^2)
 *7 An extremely strong attempt to reduce the importance of tf in
 *7 collections with wildly varying document length.
 *7 d(weight)/d(tf) = 1/tf^3
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by hyperbolic tf weighting.
 *1 convert.wt_tf.h
 *2 tfwt_hyp (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, set the new weight to 
 *7   2.0 - 1.0/weight
 *7 An stronger attempt to reduce the importance of tf in collections
 *7 with wildly varying document length.
 *7 d(weight)/d(tf) = 1/tf^2
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by okapi's tf weighting.
 *1 convert.wt_tf.o
 *2 tfwt_okapi (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, set the new weight to 
 *7   weight/(2.0+weight)
 *7 Another attempt to reduce the importance of tf in collections with
 *7 wildly varying document length.
 *7 d(weight)/d(tf) = 2/(tf^2+4*tf+4)
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by extremely-hyperbolic tf weighting.
 *1 convert.wt_tf.e
 *2 tfwt_ex_hyp (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, set the new weight to 
 *7   2.0 - 1.0/weight^2
 *7 An extremely strong attempt to reduce the importance of tf in
 *7 collections with wildly varying document length.
 *7 d(weight)/d(tf) = 1/tf^3
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "vector.h"

/*
 * Routines for converting a term-freq weighted subvector into a subvector
 * with the specified weighting scheme. Weighting schemes and the desired
 * weight_type parameter.
 * Parameters are specified by the first character of the incoming string
 *          "SQUARE"   : new_tf = tf * tf / (avg-tf * avg-tf)
 *          "NONE"     : new_tf = tf/avg-tf
 *          "LOG"      : new_tf = (ln (tf) + 1.0) / (ln (avg-tf) + 1.0)
 *          "HYP"      : new_tf = (2.0 - 1.0/tf) / (2.0 - 1.0/avg-tf)
 *          "OKAPI"    : new_tf = (tf/(2.0+tf)) / (avg-tf/(2.0+avg-tf))
 *          "EX_HYP"   : new_tf = (2.0-1.0/(tf*tf))/(2.0-1.0/(avg-tf*avg-tf))
 *
 *          "hyp"      : new_tf = 2.0 - 1.0/tf
 *          "okapi"    : new_tf = tf/(2.0+tf)
 *          "ex_hyp"   : new_tf = 2.0-1.0/(tf*tf)
*/

/* Declared in proc_convert.c ... 

*/

int
tfwt_SQUARE (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;
    float avg = 0.0;

    for (i = 0; i < vec->num_conwt; i++) {
        avg += conwt->wt;
        conwt++;
    }
    avg += .00001;
    avg /= vec->num_conwt;
    avg = 1.0 - 0.5/sqrt((double) avg);
/*
    avg *= avg;
*/
    conwt = vec->con_wtp;
    for (i = 0; i < vec->num_conwt; i++) {
	conwt->wt = 1.0 - 0.5/sqrt((double) conwt->wt);
/*
        conwt->wt *= conwt->wt;
*/
        conwt->wt /= avg;
        conwt++;
    }
    return (1);
}

int
tfwt_NONE (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;
    float avg = 0.0;

    for (i = 0; i < vec->num_conwt; i++) {
        avg += conwt->wt;
        conwt++;
    }
    avg += .00001;
    avg /= vec->num_conwt;

    conwt = vec->con_wtp;
    for (i = 0; i < vec->num_conwt; i++) {
        conwt->wt /= avg;
        conwt++;
    }
    return (1);
}

int
tfwt_LOG (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;
    float avg = 0.0;

    for (i = 0; i < vec->num_conwt; i++) {
        avg += conwt->wt;
        conwt++;
    }
    avg += .00001;
    avg /= vec->num_conwt;
    avg = log ((double)avg) + 1.0;

    conwt = vec->con_wtp;
    for (i = 0; i < vec->num_conwt; i++) {
        conwt->wt = log ((double)conwt->wt) + 1.0;
        conwt->wt /= avg;
        conwt++;
    }
    return (1);
}

int
tfwt_HYP (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;
    float avg = 0.0;

    for (i = 0; i < vec->num_conwt; i++) {
        avg += conwt->wt;
        conwt++;
    }
    avg += .00001;
    avg /= vec->num_conwt;
    avg = 2.0 - 1.0/avg;

    conwt = vec->con_wtp;
    for (i = 0; i < vec->num_conwt; i++) {
	conwt->wt = 2.0 - 1.0/conwt->wt;
        conwt->wt /= avg;
        conwt++;
    }
    return (1);
}

int
tfwt_OKAPI (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;
    float avg = 0.0;

    for (i = 0; i < vec->num_conwt; i++) {
        avg += conwt->wt;
        conwt++;
    }
    avg += .00001;
    avg /= vec->num_conwt;
    avg /= 2.0 + avg;

    conwt = vec->con_wtp;
    for (i = 0; i < vec->num_conwt; i++) {
	conwt->wt /= 2.0 + conwt->wt;
        conwt->wt /= avg;
        conwt++;
    }
    return (1);
}

int
tfwt_EX_HYP (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;
    float avg = 0.0;

    for (i = 0; i < vec->num_conwt; i++) {
        avg += conwt->wt;
        conwt++;
    }
    avg += .00001;
    avg /= vec->num_conwt;
    avg = 2.0 - 1.0/(avg*avg);

    conwt = vec->con_wtp;
    for (i = 0; i < vec->num_conwt; i++) {
	conwt->wt = 2.0 - 1.0/(conwt->wt*conwt->wt);
        conwt->wt /= avg;
        conwt++;
    }
    return (1);
}

int
tfwt_hyp (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;

    for (i = 0; i < vec->num_conwt; i++) {
	conwt->wt = 2.0 - 1.0/conwt->wt;
        conwt++;
    }
    return (1);
}
/*
int
tfwt_okapi (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;

    for (i = 0; i < vec->num_conwt; i++) {
	conwt->wt /= 2.0 + conwt->wt;
        conwt++;
    }
    return (1);
}
*/
int
tfwt_ex_hyp (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;

    for (i = 0; i < vec->num_conwt; i++) {
	conwt->wt = 2.0 - 1.0/(conwt->wt*conwt->wt);
        conwt++;
    }
    return (1);
}

int
tfwt_AUG (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;
    float max = 0.0;

    for (i = 0; i < vec->num_conwt; i++) {
	if (conwt->wt > max)
	    max = conwt->wt;
        conwt++;
    }
    max += .00001;
    max = log ((double)max) + 1.0;

    conwt = vec->con_wtp;
    for (i = 0; i < vec->num_conwt; i++) {
        conwt->wt = 0.40 + 0.60*(log ((double)conwt->wt) + 1.0)/max;
        conwt++;
    }
    return (1);
}

int
tfwt_ALOG (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;
    float avg = 0.0;

    for (i = 0; i < vec->num_conwt; i++) {
        conwt->wt = log ((double)conwt->wt) + 1.0;
	avg += conwt->wt;
        conwt++;
    }
    avg += .00001;
    avg /= vec->num_conwt;

    conwt = vec->con_wtp;
    for (i = 0; i < vec->num_conwt; i++) {
        conwt->wt /= avg;
        conwt++;
    }

    return (1);
}


/*
 * tfwt_norm
 * 
 * Raw tf values are capped by max_tf i.e. tf is set to MIN(tf, max_tf).
 * If a word occurs 10,500 times in a text, this is not significantly
 * different from occurring max_tf (= 10,000 say) times.
 * tf factor = (1 + log(tf)) / (1 + log(avg(tf)) * (1 + log(max_tf))
 * Using this formula ensures that the tf factor is in [0,1].
 * Used in conjunction with idf factor N and normalization factor U.
 * Using this combination (MNU) retains the goodness of L(.)u schemes
 * while ensuring that all term weights are in [0,1].
 */

static long max_tf;
static SPEC_PARAM spec_args[] = {
    {"convert.weight_tf.max_tf", getspec_long, (char *) &max_tf},
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

int
init_tfwt_norm(spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    if (UNDEF == lookup_spec(spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    return 0;
}

int
tfwt_norm(vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;
    float avg = 0.0;

    for (i = 0; i < vec->num_conwt; i++) {
	conwt->wt = MIN(conwt->wt, max_tf);
        avg += conwt->wt;
        conwt++;
    }
    avg += .00001;
    avg /= vec->num_conwt;
    avg = (log((double)avg) + 1.0) * (log((double)max_tf) + 1.0);

    conwt = vec->con_wtp;
    for (i = 0; i < vec->num_conwt; i++) {
        conwt->wt = log ((double)conwt->wt) + 1.0;
        conwt->wt /= avg;
        conwt++;
    }
    return (1);
}
