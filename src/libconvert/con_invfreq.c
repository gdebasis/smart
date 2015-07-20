#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/con_invfreq.c,v 11.2 1993/02/03 16:49:44 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Given a concept, return the coll freq for it (derived from inverted file)
 *1 convert.con_collinfo.invfreq
 *2 con_invfreq (con, freq, inst)
 *3   long *con;
 *3   float *freq;
 *3   int inst;
 *4 init_con_invfreq (spec, param_prefix)
 *5   "convert.con_collinfo.trace"
 *5   "doc.textloc_file"
 *5   "*.inv_file"
 *5   "*.inv_file.rmode"
 *4 close_con_invfreq (inst)
 *7 Return the collection frequency value for con, ie number of 
 *7 docs in which the term occurs, determined by the number of 
 *7 entries in the inverted list for the term (from "inv_file").
 *7 Note that "inv_file" is a ctype dependant parameter, and is found by
 *7 looking up the value of the parameter formed by concatenating 
 *7 param_prefix and "inv_file".
***********************************************************************/
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "docindex.h"
#include "trace.h"
#include "inv.h"
#include "inst.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("convert.con_collinfo.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *prefix;
static char *inv_file;
static long inv_mode;
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix,  "inv_file",     getspec_dbfile,    (char *) &inv_file},
    {&prefix,  "inv_file.rmode", getspec_filemode,(char *) &inv_mode},
    };
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int inv_fd;
    char file_name[PATH_LEN];
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_con_invfreq (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    STATIC_INFO *ip;
    int new_inst;
    long i;

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }
    prefix = param_prefix;
    if (UNDEF == lookup_spec_prefix (spec, &pspec_args[0], num_pspec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_inv_idf_weight");
 
    if (! VALID_FILE (inv_file))
        return (UNDEF);

   /* Check to see if this file_name has already been initialized.  If
       so, that instantiation will be used. */
    for (i = 0; i < max_inst; i++) {
        if (info[i].valid_info && 0 == strcmp (inv_file, info[i].file_name)) {
            info[i].valid_info++;
            return (i);
        }
    }
    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    /* Get occurrence information from inverted file.  For efficiency,
       should only read fixed part of inv_file, but for now read entire
       thing */
    if (UNDEF == (ip->inv_fd = open_inv (inv_file, inv_mode))) {
        return (UNDEF);
    }

    (void) strncpy (ip->file_name, inv_file, PATH_LEN);
    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_con_invfreq");
    return (new_inst);
}

int
con_invfreq (con, freq, inst)
long *con;
float *freq;
int inst;
{
    STATIC_INFO *ip;
    INV inv;
    
    PRINT_TRACE (2, print_string, "Trace: entering con_invfreq");
    PRINT_TRACE (4, print_long, con);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "con_invfreq");
        return (UNDEF);
    }
    ip = &info[inst];

    inv.id_num = *con;
    if (1 == seek_inv (ip->inv_fd, &inv) &&
        1 == read_inv (ip->inv_fd, &inv))
        *freq = (float) inv.num_listwt;
    else
        *freq = 0.0;

    PRINT_TRACE (4, print_float, freq);
    PRINT_TRACE (2, print_string, "Trace: leaving con_invfreq");
    return (1);
}
    

int
close_con_invfreq (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_con_invfreq");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_con_invfreq");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;
    /* Free buffers if this was last close of this inst */
    if (ip->valid_info == 0) {
        if (UNDEF == close_inv (ip->inv_fd))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_con_invfreqf");
    return (0);
}
