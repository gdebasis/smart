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
#include "pnorm_vector.h"
#include "local_functions.h"

static char *qvec_file;
static long read_mode;
static long docid_width;
static SPEC_PARAM spec_args[] = {
    {"pnorm.query_file", getspec_dbfile, (char *) &qvec_file},
    {"inter.query_file.rmode", getspec_filemode, (char *) &read_mode},
    {"inter.docid_width",  getspec_long, (char *) &docid_width},
    TRACE_PARAM ("inter.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


/* Static info to be kept for each instantiation of this proc */
typedef struct {
    int valid_info;
    SPEC *saved_spec;
    long docid_width;
    int qfile;
    int vv_inst, util_inst;
} STATIC_INFO;
static STATIC_INFO *info;
static int max_inst = 0;

int init_sim_pvec_pvec(), sim_pvec_pvec(), close_sim_pvec_pvec();


int
init_pnorm_sim_vec (spec, unused)
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

    PRINT_TRACE (2, print_string, "Trace: entering init_pnorm_sim_vec");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    ip->saved_spec = spec;

    /* WARNING.  docid_width should really get set using is->orig_spec so */
    /* it doesn't change upon an Euse command */
    ip->docid_width = docid_width;
    if (UNDEF == (ip->qfile = open_pnorm(qvec_file, read_mode)))
	return(UNDEF);
    if (UNDEF == (ip->vv_inst = init_sim_pvec_pvec(spec, NULL)) ||
	UNDEF == (ip->util_inst = init_inter_util (spec, (char *) NULL)))
        return (UNDEF);

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: entering init_pnorm_sim_vec");
    return (new_inst);
}

int
close_pnorm_sim_vec (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_pnorm_sim_vec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.pnorm_sim_q_doc");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Check to see if still have valid instantiations using this data */
    if (--ip->valid_info > 0)
        return (0);

    if (UNDEF == close_pnorm(ip->qfile))
	return(UNDEF);
    if (UNDEF == close_sim_pvec_pvec(ip->vv_inst) ||
	UNDEF == close_inter_util (ip->util_inst))
        return (UNDEF);

    ip->valid_info--;
    PRINT_TRACE (2, print_string, "Trace: leaving close_pnorm_sim_vec");
    return (1);
}

int
pnorm_sim_qvec (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    char temp_buf[PATH_LEN];
    float sim;
    VEC dvec;
    PNORM_VEC qvec;
    PVEC_VEC_PAIR vpair;
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
    qvec.id_num = ((VEC *)(is->retrieval.query->vector))->id_num;
    if (UNDEF == seek_pnorm(ip->qfile, &qvec) ||
	UNDEF == read_pnorm(ip->qfile, &qvec))
	return(UNDEF);

    if (UNDEF == inter_get_vec (is->command_line[1], &dvec, ip->util_inst)) {
        if (UNDEF == add_buf_string ("Cannot index doc", &is->err_buf))
            return (UNDEF);
        return (0);
    }

    vpair.qvec = &qvec; vpair.dvec = &dvec;
    status = sim_pvec_pvec(&vpair, &sim, ip->vv_inst);
    (void) sprintf (temp_buf,
                    "%*ld  %*ld  %6.4f\n",
                    (int) -ip->docid_width, qvec.id_num.id,
                    (int) -ip->docid_width, dvec.id_num.id,
                    sim);
    if (UNDEF == add_buf_string (temp_buf, &is->output_buf))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving sim_qvec");

    return (status);
}
