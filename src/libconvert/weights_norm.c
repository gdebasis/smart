#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/weights_norm.c,v 11.2 1993/02/03 16:49:20 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweighting procedure no-op for normalization of weights.
 *1 convert.wt_norm.x
 *1 convert.wt_norm.n

 *7 Do nothing.
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by cosine normalization
 *1 convert.wt_norm.c
 *2 normwt_cos (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, divide its existing weight by
 *7 the length of the vector.  Length is defined as
 *7 sqrt (sum of (existing weights squared)).
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by normalization with sum of weights
 *1 convert.wt_norm.s
 *2 normwt_sum (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, divide its existing weight by
 *7 the length of the vector.  Length is defined as
 *7 sum of existing weights.
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by cosine normalization and store length
 *1 convert.wt_norm.C
 *2 normwt_cos_store (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, divide its existing weight by
 *7 the length of the vector.  Length is defined as
 *7 sqrt (sum of (existing weights squared)).
 *7 Quick hack to enable normalization of one ctype by the length of
 *7 another (eg want phrase weights in one ctype to be roughly equal
 *7 to normal word weights in another, but don't want the phrase weights
 *7 to play any role in normal word normalization.)
 *9 Should be replaced by explicit mechanism.
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by cosine normalization of last stored length
 *1 convert.wt_norm.P
 *2 normwt_cos_use (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;

 *7 For each term in vector, divide its existing weight by
 *7 the last stored vector length. 
 *7 Quick hack to enable normalization of one ctype by the length of
 *7 another (eg want phrase weights in one ctype to be roughly equal
 *7 to normal word weights in another, but don't want the phrase weights
 *7 to play any role in normal word normalization.)
 *7 Assumes that the previous stored vector length comes from an
 *7 earlier ctype of this vector,
 *9 Should be replaced by explicit mechanism.
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
*      3. Normalize the entire subvector - most often to "sum of squares" of
*                  terms = 1. Alternative is sum of terms = 1

* Weighting schemes and the desired weight_type parameter
* Parameters are specified by the first character of the incoming string
*      3.  "none"     : norm_weight = new_wt
*                       No normalization done
*          "sum"      : divide each new_wt by sum of new_wts in vector
*          "cosine"   : divide each new_wt by sqrt (sum of (new_wts squared) )
*                       This is the usual cosine normalization (I.e. an
*                       inner product function of two cosine normalized
*                       vectors will yield the same results as a cosine
*                       function on vectors (either normalized or not))
*          "fourth"   : divide each new_wt by sum of (new_wts ** 4)
*          "max"      : divide each new_wt by max new_wt in vector
*
*/

/* Declared in proc_convert.c 
 * PROC_TAB norm_weight[] = {
 *  "x",               EMPTY,         EMPTY,         EMPTY,
 *  "n",               EMPTY,         EMPTY,         EMPTY,
 *  "c",               EMPTY,         normwt_cos,    EMPTY,
 *  "s",               EMPTY,         normwt_sum,    EMPTY,
 * * C is same as c, except stores norm factor *
 *  "C",               EMPTY,         normwt_cos_store, EMPTY,
 ** P is same as c, except uses stored norm factor from last C ctype *
 ** eg. if want phrase ctype with same norm factor as regular tokens *
 ** Assumes phrase ctype is > regular ctype, so that C is called first *
 *  "P",               EMPTY,         normwt_cos_use, EMPTY,
 *};
 *int num_norm_weight = sizeof (norm_weight) / sizeof (norm_weight[0]);
*/

int
normwt_cos (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;
    CON_WT *ptr = conwt;
    double sum = 0.0;

    for (i = 0; i < vec->num_conwt; i++) {
        sum += ptr->wt * ptr->wt;
        ptr++;
    }
    sum = sqrt (sum);
    ptr = conwt;
    for (i = 0; i < vec->num_conwt; i++) {
        ptr->wt /= sum;
        ptr++;
    }
    return (1);
}

int wt_lm (VEC* vec, float lambda) {
    long i;
    CON_WT *conwt = vec->con_wtp;
    CON_WT *ptr = conwt;
	float f = lambda/(1-lambda);

    for (i = 0; i < vec->num_conwt; i++) {
        ptr->wt = log(1 + f * ptr->wt);
        ptr++;
    }
    return (1);
}

int
normwt_sum (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;
    CON_WT *ptr = conwt;
    double sum = 0.0;

    for (i = 0; i < vec->num_conwt; i++) {
        sum += ptr->wt;
        ptr++;
    }
    ptr = conwt;
    for (i = 0; i < vec->num_conwt; i++) {
        ptr->wt /= sum;
        ptr++;
    }
    return (1);
}

static double saved_factor = 1.0;

int
normwt_cos_store (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;
    CON_WT *ptr = conwt;
    double sum = 0.0;

    for (i = 0; i < vec->num_conwt; i++) {
        sum += ptr->wt * ptr->wt;
        ptr++;
    }
    sum = sqrt (sum);
    ptr = conwt;
    for (i = 0; i < vec->num_conwt; i++) {
        ptr->wt /= sum;
        ptr++;
    }
    saved_factor = sum;
    return (1);
}

int
normwt_cos_use (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;
    CON_WT *ptr = conwt;

    for (i = 0; i < vec->num_conwt; i++) {
        ptr->wt /= saved_factor;
        ptr++;
    }
    return (1);
}
