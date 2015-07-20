#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/makevec.c,v 11.2 1993/02/03 16:50:54 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Convert list of concepts in p_doc to a full vector
 *1 index.makevec.makevec
 *2 makevec (p_doc, vec, inst)
 *3   SM_CONDOC *p_doc;
 *3   SM_VECTOR *vec;
 *3   int inst;
 *4 init_makevec (spec, unused)
 *5   "index.num_ctypes"
      "index.makevec.trace"
 *4 close_makevec (inst)
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "io.h"
#include "docindex.h"
#include "trace.h"
#include "vector.h"
#include "inst.h"

static long num_ctypes;         /* Number of possible ctypes in collection */

static SPEC_PARAM spec_args[] = {
    {"index.num_ctypes",        getspec_long, (char *) &num_ctypes},
    TRACE_PARAM ("index.makevec.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int con_comp();    /* Comparison routine to sort cons by
                             increasing concept number */

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
    /* ctype info */
    long *ctype_len;         /* lengths of each subvector in vec */
    long num_ctype;          /* Number of sub-vectors space reserved for */
    /* conwt_info */
    CON_WT *con_wtp;         /* pointer to concepts,weights for vec */
    long num_conwt;          /* total number of concept,weight pairs
                                space reserved for */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_makevec (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: entering init_makevec");
    
    /* Reserve space for this new instantiation's static variables */
    NEW_INST (new_inst);
    if (new_inst == UNDEF)
        return (UNDEF);

    ip = &info[new_inst];
    
    /* Reserve space for ctype array */
    if (NULL == (ip->ctype_len = (long *)
                 malloc ((unsigned) num_ctypes * sizeof (long)))) {
        return (UNDEF);
    }
    ip->num_ctype = num_ctypes;
    ip->num_conwt = 0;
    ip->valid_info = 1;
    
    PRINT_TRACE (2, print_string, "Trace: leaving init_makevec");
    
    return (new_inst);
}


int
makevec (p_doc, vec, inst)
SM_CONDOC *p_doc;
SM_VECTOR *vec;
int inst;
{
    STATIC_INFO *ip;
    CON_WT *conwt_ptr;
    SM_CONLOC *conloc_ptr, *conloc_end;
    long *con_ptr, *con_end;
    long i,j;
    long num_orig_con;

    PRINT_TRACE (2, print_string, "Trace: entering makevec");
    PRINT_TRACE (6, print_sm_condoc, p_doc);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "makevec");
        return (UNDEF);
    }
    ip  = &info[inst];

    vec->id_num.id = p_doc->id_num;
    EXT_NONE(vec->id_num.ext);

    /* Find number of concepts in incoming sections */
    num_orig_con = 0;
    for (i = 0; i < p_doc->num_sections; i++)
        num_orig_con += p_doc->sections[i].num_concepts;

    if (num_orig_con == 0) {
        vec->num_ctype = 0;
        vec->num_conwt = 0;
        vec->ctype_len = NULL;
        vec->con_wtp = NULL;
        PRINT_TRACE (2, print_string, "Trace: leaving makevec");
        return (0);
    }

    /* Make sure enough space reserved for concept,weight pairs */
    if (num_orig_con > ip->num_conwt) {
        if (ip->num_conwt > 0)
            (void) free ((char *) ip->con_wtp);
        ip->num_conwt += num_orig_con;
        if (NULL == (ip->con_wtp = (CON_WT *)
                     malloc ((unsigned) ip->num_conwt * sizeof (CON_WT)))) {
            return (UNDEF);
        }
    }

    /* Handle each ctype of vector separately.  First put all cons (and just
       cons) of current ctype at END of CON_WT array (guaranteed not to
       overwrite anything).  Then sort cons.  Then move cons into correct
       location from beginning of CON_WT array, setting weight as appropriate*/
    con_end = (long *) &ip->con_wtp[num_orig_con];
    conwt_ptr = ip->con_wtp;
    for (i = 0; i < ip->num_ctype; i++) {
        ip->ctype_len[i] = 0;
        con_ptr =  con_end;
        for (j = 0; j < p_doc->num_sections; j++) {
            conloc_ptr = p_doc->sections[j].concepts;
            conloc_end = &p_doc->sections[j].
                concepts[p_doc->sections[j].num_concepts];
        
            while (conloc_ptr < conloc_end) {
                if (conloc_ptr->ctype == i) {
                    *(--con_ptr) = conloc_ptr->con;
                }
                conloc_ptr++;
            }
        }

        /* sort incoming concepts by concept (within ctype i) */
        qsort ((char *) con_ptr, con_end - con_ptr,
               sizeof (long), con_comp);
        
        /* Copy concepts into start of array */
        while (con_ptr < con_end) {
            conwt_ptr->con = *con_ptr;
            conwt_ptr->wt = 1.0;
            con_ptr++;
            while (con_ptr < con_end && *con_ptr == conwt_ptr->con) {
                conwt_ptr->wt += 1.0;
                con_ptr++;
            }
            ip->ctype_len[i]++;
            conwt_ptr++;
        }
    }

    vec->num_ctype = ip->num_ctype;
    vec->num_conwt = conwt_ptr - ip->con_wtp;
    vec->ctype_len = ip->ctype_len;
    vec->con_wtp = ip->con_wtp;

    PRINT_TRACE (4, print_vector, vec);
    PRINT_TRACE (2, print_string, "Trace: leaving makevec");

    return (1);
}

int
close_makevec (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_makevec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_makevec");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (ip->num_conwt > 0) 
        (void) free ((char *) ip->con_wtp);
    if (ip->num_ctype > 0) 
        (void) free ((char *) ip->ctype_len);

    ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_makevec");
    return (0);
}


static int
con_comp (con1, con2)
long *con1, *con2;
{
    if (*con1 < *con2)
        return (-1);
    if (*con1 > *con2)
        return (1);
    return (0);
}

