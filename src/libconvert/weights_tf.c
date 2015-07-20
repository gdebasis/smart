#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/weights_tf.c,v 11.2 1993/02/03 16:49:38 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweighting procedure no-op for term-freq weighting.
 *1 convert.wt_tf.x
 *1 convert.wt_tf.n

 *7 Do nothing, leaving the weight as it was (normally tf)
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by using binary term weights
 *1 convert.wt_tf.b
 *2 tfwt_binary (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, set its weight to 1.0
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by dividing by max weight in vector
 *1 convert.wt_tf.m
 *2 tfwt_max (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, divide weight by the max weight in the vector
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by augmented tf
 *1 convert.wt_tf.a
 *2 tfwt_aug (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, set tf weight to
 *7   0.5 + 0.5 * (tf / max_tf)
 *7 where tf is the incoming weight, and max_tf is the maximum incoming
 *7 weight of the vector.
 *7 This distributes the tf contribution along the interval 0.5 to 1.0.
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by squaring each weight
 *1 convert.wt_tf.s
 *2 tfwt_square (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, square the present weight.
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by logrithmic term freq
 *1 convert.wt_tf.l
 *2 tfwt_log (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, set the new weight to 
 *7   ln (weight) + 1.0
 *7 An attempt to reduce the importance of tf in those collections with
 *7 wildly varying document length.
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by augmented log tf
 *1 convert.wt_tf.A
 *2 tfwt_aug_log (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, set the new weight to 
 *7   0.5 + 0.5 * (ln (weight) + 1.0)/(ln(max_tf) + 1.0)
 *7 A normalization attempts to further de-emphasize the tf facor
***********************************************************************/

#include <math.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "vector.h"

/* Routines for converting a term-freq weighted subvector into a subvector
* with the specified weighting scheme. There are three possible conversion
* that can be performed on each subvector:
*      1. Normalize term_freq component - most often the tf component is
*                  altered by dividing by the max tf in the subvector

* Weighting schemes and the desired weight_type parameter
* Parameters are specified by the first character of the incoming string
*      1.  "none"     : new_tf = tf
*                       No conversion to be done  1 <= new_tf
*          "binary"   : new_tf = 1
*          "max_norm" : new_tf = tf / max_tf
*                       divide each term by max in vector  0 < new_tf < 1.0
*          "aug_norm" : new_tf = 0.5 + 0.5 * (tf / max_tf)
*                       augmented normalized tf.  0.5 < new_tf <= 1.0
*          "square"   : new_tf = tf * tf
*          "log"      : new_tf = ln (tf) + 1.0
*          "log_log"  : new_tf = ln (ln (tf) + 1.0) + 1.0
*          "aug_log"   : new_tf = 0.5 + 0.5*(1.0+ln (tf))/(1.0+ln (max_tf))
*

*/
/* Declared in proc_convert.c ... 
 * static int tfwt_binary(), tfwt_max(), tfwt_aug(), tfwt_square(), tfwt_log();
 *
 * PROC_TAB tf_weight[] = {
 *  "x",               EMPTY,         EMPTY,         EMPTY,
 *  "n",               EMPTY,         EMPTY,         EMPTY,
 *  "b",               EMPTY,         tfwt_binary,   EMPTY,
 *  "m",               EMPTY,         tfwt_max,      EMPTY,
 *  "a",               EMPTY,         tfwt_aug,      EMPTY,
 *  "s",               EMPTY,         tfwt_square,   EMPTY,
 *  "l",               EMPTY,         tfwt_log,      EMPTY,
 *  "L",               EMPTY,         tfwt_log_log,  EMPTY,
 *  "A",               EMPTY,         tfwt_aug_log,  EMPTY,
 * };
 * int num_tf_weight = sizeof (tf_weight) / sizeof (tf_weight[0]);
*/

int
tfwt_binary (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;

    for (i = 0; i < vec->num_conwt; i++) {
        conwt->wt = 1.0;
        conwt++;
    }
    return (1);
}

int
tfwt_max (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;

    CON_WT *ptr = conwt;
    float max = 0.0;

    for (i = 0; i < vec->num_conwt; i++) {
        if (ptr->wt > max)
            max = ptr->wt;
        ptr++;
    }
    max += .00001;
    ptr = conwt;
    for (i = 0; i < vec->num_conwt; i++) {
        ptr->wt /= max;
        ptr++;
    }
    return (1);
}

int
tfwt_aug (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;

    CON_WT *ptr = conwt;
    float max = 0.0;

    for (i = 0; i < vec->num_conwt; i++) {
        if (ptr->wt > max)
            max = ptr->wt;
        ptr++;
    }
    max += .00001;
    ptr = conwt;
    for (i = 0; i < vec->num_conwt; i++) {
        ptr->wt  = 0.5 + 0.5 * ptr->wt / max;
        ptr++;
    }
    return (1);
}

int
tfwt_square (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;

    for (i = 0; i < vec->num_conwt; i++) {
        conwt->wt *= conwt->wt;
        conwt++;
    }
    return (1);
}

int
tfwt_log (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;

    for (i = 0; i < vec->num_conwt; i++) {
        conwt->wt = log ((double)conwt->wt) + 1.0;
        conwt++;
    }
    return (1);
}


int
tfwt_log_log (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;

    for (i = 0; i < vec->num_conwt; i++) {
        conwt->wt = log (log ((double)conwt->wt) + 1.0) + 1.0;
        conwt++;
    }
    return (1);
}


int
tfwt_aug_log (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;
    CON_WT *ptr = conwt;
    float max = 0.0;

    for (i = 0; i < vec->num_conwt; i++) {
        if (ptr->wt > max)
            max = ptr->wt;
        ptr++;
    }
    max += .00001;
    ptr = conwt;
    for (i = 0; i < vec->num_conwt; i++) {
        ptr->wt  = 0.5 + 0.5 * ((1.0 + log((double)ptr->wt)) /
				(1.0 + log((double)max)));
        ptr++;
    }
    return (1);
}
