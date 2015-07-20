#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/sim_vecs.c,v 11.2 1993/02/03 16:51:32 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "io.h"
#include "spec.h"
#include "proc.h"
#include "tr_vec.h"
#include "retrieve.h"
#include "inter.h"
#include "inst.h"
#include "trace.h"
#include "smart_error.h"
#include "vector.h"

static PROC_TAB *vec_retrieve_ptab; /* Retrieval method for vectors */

static long vec_num_wanted;         /* number of results wanted */

static long docid_width;

static SPEC_PARAM spec_args[] = {
    {"inter.vec_vec",      getspec_func,    (char *) &vec_retrieve_ptab},
    {"inter.num_wanted",      getspec_long,    (char *) &vec_num_wanted},
    {"inter.docid_width",  getspec_long, (char *) &docid_width},
    TRACE_PARAM ("inter.trace")
};
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    SPEC *saved_spec;
    PROC_INST vec_retrieve;
    long vec_num_wanted;

    long docid_width;
    int util_inst;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int
init_sim_vec (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;
    long i;

    /* Check to see if this spec file is already initialized (only need
       one initialization per spec file) */
    for (i = 0; i <  max_inst; i++) {
        if (info[i].valid_info && spec == info[i].saved_spec) {
            info[i].valid_info++;
            return (i);
        }
    }

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_sim_vec");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    ip->saved_spec = spec;

    ip->vec_retrieve.ptab = vec_retrieve_ptab;
    /* WARNING.  docid_width should really get set using is->orig_spec so */
    /* it doesn't change upon an Euse command */
    ip->docid_width = docid_width;
    if (UNDEF == (ip->vec_retrieve.inst =
                  ip->vec_retrieve.ptab->init_proc (spec, NULL)))
        return (UNDEF);

    ip->vec_num_wanted = vec_num_wanted;

    if (UNDEF == (ip->util_inst = init_inter_util (spec, (char *) NULL)))
        return (UNDEF);

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: entering close_sim_vec");

    return (new_inst);
}


int
close_sim_vec (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_sim_vec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.close_sim_vec");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Check to see if still have valid instantiations using this data */
    if (--ip->valid_info > 0)
        return (0);

    if (UNDEF == ip->vec_retrieve.ptab->close_proc (ip->vec_retrieve.inst))
        return (UNDEF);

    if (UNDEF == close_inter_util (ip->util_inst))
        return (UNDEF);

    ip->valid_info--;
    PRINT_TRACE (2, print_string, "Trace: leaving close_sim_vec");

    return (1);
}

/* Given two vecs compute the similarity between them and report results */
static int
sim_retrieve (is, vec1, vec2, inst)
INTER_STATE *is;
VEC *vec1;
VEC *vec2;
int inst;
{
    VEC_PAIR vec_pair;
    float sim_result;
    char temp_buf[PATH_LEN];
    STATIC_INFO *ip;
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.sim_retrieve");
        return (UNDEF);
    }
    ip  = &info[inst];
    
    vec_pair.vec1 = vec1;
    vec_pair.vec2 = vec2;

    if (UNDEF == ip->vec_retrieve.ptab->proc (&vec_pair,
                                              &sim_result,
                                              ip->vec_retrieve.inst))
        return (UNDEF);

    (void) sprintf (temp_buf,
                    "%*ld  %*ld  %6.4f\n",
                    (int) -ip->docid_width, vec1->id_num.id,
                    (int) -ip->docid_width, vec2->id_num.id,
                    sim_result);
    if (UNDEF == add_buf_string (temp_buf, &is->output_buf))
        return (UNDEF);

    return (1);
}

int
sim_qvec (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    VEC dvec;
    STATIC_INFO *ip;
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering sim_qvec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.sim_qvec");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (is->num_command_line < 2) {
        if (UNDEF == add_buf_string ("Must specify a doc", &is->err_buf))
            return (UNDEF);
        return (0);
    }
    if (NULL == is->retrieval.query->vector) {
        if (UNDEF == add_buf_string ("No query specified", &is->err_buf))
            return (UNDEF);
        return (0);
    }

    if (UNDEF == inter_get_vec (is->command_line[1], &dvec, ip->util_inst)) {
        if (UNDEF == add_buf_string ("Cannot index doc", &is->err_buf))
            return (UNDEF);
        return (0);
    }

    status = sim_retrieve (is, (VEC *) is->retrieval.query->vector,
                          &dvec, inst);

    PRINT_TRACE (2, print_string, "Trace: leaving sim_qvec");

    return (status);
}


int
sim_dvec (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    int status;
    VEC vec1, vec2;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering sim_dvec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.sim_dvec");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (is->num_command_line < 3) {
        if (UNDEF == add_buf_string ("Must specify two docids", &is->err_buf))
            return (UNDEF);
        return (0);
    }


    if (UNDEF == inter_get_vec (is->command_line[1], &vec1, ip->util_inst)) {
        if (UNDEF == add_buf_string ("Cannot index first doc", &is->err_buf))
            return (UNDEF);
        return (0);
    }
    if (UNDEF == save_vec (&vec1))
        return (UNDEF);
    if (UNDEF == inter_get_vec (is->command_line[2], &vec2, ip->util_inst)) {
        if (UNDEF == add_buf_string ("Cannot index second doc", &is->err_buf))
            return (UNDEF);
        return (0);
    }

    status = sim_retrieve (is, &vec1, &vec2, inst);

    if (UNDEF == free_vec (&vec1))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving sim_dvec");

    return (status);
}

