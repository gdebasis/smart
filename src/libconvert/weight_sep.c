#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/weight_std.c,v 11.2 1993/02/03 16:49:36 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Weight an entire vector by calling separate wt_ctype procedures
 *1 convert.weight.weight_separate
 *2 separate_weight (invec, outvec, inst)
 *3   VEC *invec;
 *3   VEC *outvec;
 *3   int inst;
 *4 init_separate_weight (spec, prefix)
 *5   "convert.weight.trace"
 *5   "<prefix>*.weight_ctype"
 *4 close_separate_weight (inst)
 *7 If 'prefix' is not passed, "index.ctype." is used.
 *7 Copy the entire vector from invec to outvec (unless outvec is NULL,
 *7 in which case vector is weighted in situ).  For each ctype N subvector
 *7 of outvec, call the procedure given by <prefix>N.weight_ctype to reweight
 *7 that ctype.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "vector.h"
#include "docdesc.h"
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

    SM_INDEX_DOCDESC doc_desc;    /* New style doc description */
    int *ctype_inst;
    CON_WT *conwt_buf;
    long num_conwt_buf;
    long *ctype_len_buf;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_separate_weight (spec, prefix)
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
    PRINT_TRACE (2, print_string, "Trace: entering init_separate_weight");

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

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_separate_weight");

    return (new_inst);
}

int
separate_weight (invec, outvec, inst)
VEC *invec;
VEC *outvec;
int inst;
{
    long i;
    CON_WT *con_wtp;
    STATIC_INFO *ip;
    VEC ctype_vec;         /* Version of vec that only has one ctype */

    PRINT_TRACE (2, print_string, "Trace: entering separate_weight");
    PRINT_TRACE (6, print_vector, invec);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "separate_weight");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* If no outvec is passed, we'll reweight the invec in place */
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
	    set_error (SM_INCON_ERR, "Doc has too many ctypes", "separate_weight");
	    return (UNDEF);
	}

	bcopy ((char *) invec->con_wtp,
	       (char *) ip->conwt_buf,
	       (int) invec->num_conwt * sizeof (CON_WT));
	bcopy ((char *) invec->ctype_len,
	       (char *) ip->ctype_len_buf,
	       (int) invec->num_ctype * sizeof (long));

	outvec->id_num = invec->id_num;
	outvec->num_ctype = invec->num_ctype;
	outvec->num_conwt = invec->num_conwt;
	outvec->con_wtp   = ip->conwt_buf;
	outvec->ctype_len = ip->ctype_len_buf;
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

    PRINT_TRACE (4, print_vector, outvec);
    PRINT_TRACE (2, print_string, "Trace: leaving separate_weight");
    return (1);
}


int
close_separate_weight (inst)
int inst;
{
    long i;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_separate_weight");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_separate_weight");
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

    PRINT_TRACE (2, print_string, "Trace: leaving close_separate_weight");

    return (0);
}



