#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/con_inv.c,v 11.2 1993/02/03 16:49:18 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Get inverted file entry for con

 *2 con_inv (con, inv, inst)
 *3   long *con;
 *3   INV *inv;
 *3   int inst;

 *4 init_con_inv (spec, param_prefix)
 *5   "index.con_inv.trace"
 *5   "*.inv_file"
 *5   "*.inv_file.rmode"
 *4 close_con_inv (inst)

 *7 Lookup con in the inverted file specified by parameter given by 
 *7 the concatenation of param_prefix and "inv_file".  
 *7 Normally, param_prefix will have some value like "ctype.1." in order
 *7 to obtain the ctype dependant inverted file.  Ie. "ctype.1.inv_file"
 *7 will be looked up in the specifications to find the correct inverted
 *7 file to use.
 *7 UNDEF returned if error, else 0.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "inv.h"
#include "io.h"
#include "docindex.h"
#include "trace.h"
#include "context.h"
#include "inst.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.con_inv.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

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
    /* procedure dependant info */
    int fd;
    char file_name[PATH_LEN];
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_con_inv (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    STATIC_INFO *ip;
    long i;
    int new_inst;

    /* Lookup the values of the relevant global parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    prefix = param_prefix;
    if (UNDEF == lookup_spec_prefix (spec, &pspec_args[0], num_pspec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_con_inv");

    if (! VALID_FILE (inv_file))
        return (UNDEF);

    /* If this inv_file has already been opened for inv con, just use
       that instantiation */
    for (i = 0; i < max_inst; i++) {
        if (info[i].valid_info && 0 == strcmp (inv_file, info[i].file_name)) {
            info[i].valid_info++;
            PRINT_TRACE (2, print_string, "Trace: leaving init_con_inv");
            return (i);
        }
    }

    /* Use new instantiation */
    NEW_INST (new_inst);
    if (new_inst == UNDEF)
        return (UNDEF);

    ip = &info[new_inst];
            
    (void) strncpy (ip->file_name, inv_file, PATH_LEN);

    /* Open inverted file */
    if (UNDEF == (ip->fd = open_inv (inv_file, inv_mode))) {
        return (UNDEF);
    }

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_con_inv");

    return (new_inst);
}

/* Lookup concept in the inverted file, return inv list corresponding to it */
int
con_inv (con, inv, inst)
long *con;
INV *inv;
int inst;
{
    int status;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering con_inv");
    PRINT_TRACE (6, print_long, con);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "con_inv");
        return (UNDEF);
    }
    ip  = &info[inst];

    inv->id_num = *con;
    if (UNDEF == (status = seek_inv(ip->fd, inv))) {
        return (UNDEF);
    }
    if (status == 0) {
        /* con is not in inverted file */
        inv->id_num = UNDEF;
    }
    else {
        if (1 != read_inv (ip->fd, inv))
            inv->id_num = UNDEF;
    }

    PRINT_TRACE (4, print_inv, inv);
    PRINT_TRACE (2, print_string, "Trace: leaving con_inv");

    return (0);
}

int
close_con_inv (inst)
int inst;
{
    STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_con_inv");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_con_inv");
        return (UNDEF);
    }
    ip  = &info[inst];
    ip->valid_info--;

    if (0 == ip->valid_info) {
        if (UNDEF == close_inv (ip->fd))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_con_inv");

    return (0);
}



