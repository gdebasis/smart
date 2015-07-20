#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/weight_slope.c,v 11.2 1993/02/03 16:49:34 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Weight an entire vector with wt_ctype procedures, then cosine normalize
 *1 convert.weight.weight_slope
 *2 weight_slope (invec, outvec, inst)
 *3   VEC *invec;
 *3   VEC *outvec;
 *3   int inst;
 *4 init_weight_slope (spec, unused)
 *5   "ctype.*.weight_ctype"
 *5   "convert.weight_slope.major_ctype"
 *5   "convert.weight_slope.pivot"
 *5   "convert.weight_slope.slope"
 *5   "convert.weight.trace"
 *4 close_weight_slope (inst)
 *7 Copy the entire vector from invec to outvec (unless outvec is NULL
 *7 in which case weighting takes place in situ).  For each ctype N subvector
 *7 of outvec, call the procedure given by ctype.N.weight_ctype to reweight
 *7 that ctype.
 *7 
 *7 Finds a line of slope that intersect y=x at pivot.
 *7 Then normalize the entire vector by the
 *7     slope*cosine + (1-slope)*pivot
 *7 where cosine is the eucledian length of the subvector for ctype
 *7 major_ctype; ie,
 *7    sqrt (sum (square of weights in ctype major_ctype))
 *7 If major_ctype is -1, the weights of all ctypes in the vector are
 *7 used in the normalization.
 *7 This procedure is usually used in circumstances where the vector
 *7 includes optional extra information that should not affect the
 *7 normalization of the major information, but the extra information must be
 *7 made commensurate with the major information.
 *7 If the major_ctype has length 0, an empty vector is returned.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "docindex.h"
#include "docdesc.h"
#include "vector.h"
#include "trace.h"
#include "context.h"
#include "inst.h"

/* Same as std_weight, except there is a final cosine normalization of the
   entire vector (std_weight potentially provides cosine normalization over
   individual ctype sub-vectors only) */

static long major_ctype;
static float pivot;
static float slope;

static SPEC_PARAM spec_args[] = {
    {"convert.weight_slope.major_ctype", getspec_long, (char *) &major_ctype},
    {"convert.weight_slope.pivot",  getspec_float, (char *) &pivot},
    {"convert.weight_slope.slope", getspec_float, (char *) &slope},
    TRACE_PARAM ("convert.weight.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    SM_INDEX_DOCDESC doc_desc;    /* New style doc description */
    int *ctype_inst;
    CON_WT *conwt_buf;
    long num_conwt_buf;
    long *ctype_len_buf;
    long major_ctype;             /* Normalize vector at end by this ctype */
    float pivot;
    float slope;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_weight_slope (spec, prefix)
SPEC *spec;
char *prefix;
{
    long i;
    int new_inst;
    STATIC_INFO *ip;
    char param_prefix[PATH_LEN];

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }
    PRINT_TRACE (2, print_string, "Trace: entering init_weight_slope");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    if (UNDEF == lookup_spec_docdesc (spec, &ip->doc_desc)) {
        return (UNDEF);
    }

    if (NULL == (ip->conwt_buf = (CON_WT *)
                 malloc (1024 * sizeof (CON_WT))) ||
        NULL == (ip->ctype_inst = (int *) 
                 malloc ((unsigned) ip->doc_desc.num_ctypes * sizeof (int))) ||
        NULL == (ip->ctype_len_buf = (long *)
                 malloc ((unsigned) ip->doc_desc.num_ctypes * sizeof (long))))
        return (UNDEF);

    ip->num_conwt_buf = 1024;

    for (i = 0; i < ip->doc_desc.num_ctypes; i++) {
        (void) sprintf (param_prefix, "%s%ld.",
			prefix==NULL ? "index.ctype." : prefix,  i);
        if (UNDEF == (ip->ctype_inst[i] = ip->doc_desc.ctypes[i].weight_ctype->
                      init_proc (spec, param_prefix)))
            return (UNDEF);
    }

    ip->major_ctype = major_ctype;
    ip->pivot = pivot;
    ip->slope = slope;
    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_weight_slope");

    return (new_inst);
}

int
weight_slope (invec, outvec, inst)
VEC *invec;
VEC *outvec;
int inst;
{
    long i,j;
    CON_WT *con_wtp;
    STATIC_INFO *ip;
    VEC ctype_vec;         /* Version of vec that only has one ctype */
    long start_ctype;
    double sum;

    PRINT_TRACE (2, print_string, "Trace: entering weight_slope");
    PRINT_TRACE (6, print_vector, invec);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "weight_slope");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (outvec == NULL)
	outvec = invec;

    else {
	if (invec->num_conwt > ip->num_conwt_buf) {
	    if (NULL == (ip->conwt_buf = Realloc( ip->conwt_buf,
						  invec->num_conwt, CON_WT )))
		return (UNDEF);
	    ip->num_conwt_buf = invec->num_conwt;
	}
	if (invec->num_ctype > ip->doc_desc.num_ctypes) {
	    set_error (SM_INCON_ERR, "Doc has too many ctypes", "weight_slope");
	    return (UNDEF);
	}
	outvec->id_num = invec->id_num;
	outvec->num_ctype = invec->num_ctype;
	outvec->num_conwt = invec->num_conwt;
	outvec->con_wtp   = ip->conwt_buf;
	outvec->ctype_len = ip->ctype_len_buf;

	if (invec->num_conwt || invec->ctype_len) {
	    bcopy ((char *) invec->con_wtp,
		   (char *) ip->conwt_buf,
		   (int) invec->num_conwt * sizeof (CON_WT));
	    bcopy ((char *) invec->ctype_len,
		   (char *) ip->ctype_len_buf,
		   (int) invec->num_ctype * sizeof (long));
	}
    }    

    if (invec->num_conwt == 0 || invec->ctype_len == 0) {
        PRINT_TRACE (4, print_vector, outvec);
        PRINT_TRACE (2, print_string, "Trace: leaving weight_slope");
        return (0);
    }
    

    /* Call the component weighting method on each subvector of vec */
    ctype_vec.id_num = outvec->id_num;
    ctype_vec.num_ctype = 1;
    con_wtp = outvec->con_wtp;
    for (i = 0;
         i < outvec->num_ctype && 
             con_wtp < &outvec->con_wtp[outvec->num_conwt];
         i++) {
        if (outvec->ctype_len[i] <= 0)
            continue;
        ctype_vec.ctype_len = &outvec->ctype_len[i];
        ctype_vec.num_conwt = outvec->ctype_len[i];
        ctype_vec.con_wtp   = con_wtp;
        if (UNDEF == ip->doc_desc.ctypes[i].weight_ctype->
            proc (&ctype_vec, (char *) NULL, ip->ctype_inst[i]))
            return (UNDEF);
        con_wtp += ctype_vec.num_conwt;
    }

    /* Normalize the entire vector */
    sum = 0;
    if (-1 == ip->major_ctype) {
        /* Cosine normalization by the entire vector */
        for (i = 0; i < outvec->num_conwt; i++) {
            sum += outvec->con_wtp[i].wt * outvec->con_wtp[i].wt;
        }
    }
    else if (ip->major_ctype < outvec->num_ctype) {
        /* Cosine normalization by only major_ctype */
        start_ctype = 0;
        for (j = 0; j < ip->major_ctype; j++) {
            start_ctype += outvec->ctype_len[j];
        }
        for (i = start_ctype;
             i < start_ctype + outvec->ctype_len[ip->major_ctype];
             i++) {
            sum += outvec->con_wtp[i].wt * outvec->con_wtp[i].wt;
        }
    }
    else {
        set_error (SM_INCON_ERR, "Major_ctype is too large", "weight_slope");
        return (UNDEF);
    }
    sum = sqrt (sum);

    if (sum == 0.0) {
        /* Return an empty vector */
        outvec->num_ctype = 0;
        outvec->num_conwt = 0;
    }
    else
        for (i = 0; i < outvec->num_conwt; i++)
	    outvec->con_wtp[i].wt /= ip->slope*sum + (1.0 - ip->slope)*pivot;

    PRINT_TRACE (4, print_vector, outvec);
    PRINT_TRACE (2, print_string, "Trace: leaving weight_slope");
    return (1);
}


int
close_weight_slope (inst)
int inst;
{
    long i;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_weight_slope");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_weight_slope");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Close weighting procs and free buffers if this was last close of inst */
    ip->valid_info--;
    if (ip->valid_info == 0) {
        for (i = 0; i < ip->doc_desc.num_ctypes; i++) {
            if (UNDEF == ip->doc_desc.ctypes[i].weight_ctype->
                close_proc (ip->ctype_inst[i]))
                return (UNDEF);
        }
        (void) free ((char *) ip->ctype_inst);
        (void) free ((char *) ip->conwt_buf);
        (void) free ((char *) ip->ctype_len_buf);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_weight_slope");

    return (0);
}
