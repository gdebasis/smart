#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/con_cw_idf.c,v 11.2 1993/02/03 16:49:42 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Given a concept, return the idf value for it
 *1 convert.con_collinfo.idf
 *2 con_cw_idf (con, freq, inst)
 *3   long *con;
 *3   float *freq;
 *3   int inst;
 *4 init_con_cw_idf (spec, param_prefix)
 *5   "convert.con_collinfo.trace"
 *5   "*.idf.collstat_file"
 *5   "*.idf.collstat_file.rmode"
 *4 close_con_cw_idf (inst)
 *7 Return the natural idf value for con, ie log (N/n) where n is the number of
 *7 docs in which the term occurs, determined by the entry for that term
 *7 in the collstat_file.  If collstat_file does not exist, or does not
 *7 have collection frequency info in it, then the value is the number of 
 *7 entries in the inverted list for the term (from con_invidf()).
 *7 Note that "collstat_file" is a ctype dependant parameter, and is found by
 *7 looking up the value of the parameter formed by concatenating 
 *7 param_prefix and "idf.collstat_file".
***********************************************************************/
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "docindex.h"
#include "trace.h"
#include "dir_array.h"
#include "inst.h"
#include "collstat.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("convert.con_collinfo.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *prefix;
static char *collstat_file;
static long collstat_rmode;
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix,"idf.collstat_file",     getspec_dbfile,    (char *) &collstat_file},
    {&prefix,"idf.collstat_file.rmode", getspec_filemode,(char *) &collstat_rmode},
    };
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int dir_array_fd;
    char collstat_file[PATH_LEN];
    float *idf;
    long num_idf;
    int inv_inst;            /* If not UNDEF, then we are using inverted
                                file for info instead of collstat_file */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_con_cw_idf (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    STATIC_INFO *ip;
    int new_inst;
    long i;
    DIR_ARRAY dir_array;

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }
    prefix = param_prefix;
    if (UNDEF == lookup_spec_prefix (spec, &pspec_args[0], num_pspec_args))
        return (UNDEF);
 
    PRINT_TRACE (2, print_string, "Trace: entering init_con_cw_idf");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    if (VALID_FILE (collstat_file)) {
        /* Check to see if this file_name has already been initialized.  If
           so, that instantiation will be used. */
        for (i = 0; i < max_inst; i++) {
            if (info[i].valid_info &&
                0 == strcmp (collstat_file, info[i].collstat_file) &&
                info[i].inv_inst == UNDEF) {
                info[i].valid_info++;
                return (i);
            }
        }
    }
    else {
        /* Must use inverted file for info */
        if (UNDEF == (ip->inv_inst = init_con_invidf (spec, param_prefix)))
            return (UNDEF);
        ip->valid_info = 1;
        return (new_inst);
    }

    (void) strncpy (ip->collstat_file, collstat_file, PATH_LEN);

    dir_array.id_num = COLLSTAT_IDF;

    if (UNDEF == (ip->dir_array_fd = open_dir_array (collstat_file,
                                                   collstat_rmode)))
        return (UNDEF);

    if (1 != (seek_dir_array (ip->dir_array_fd, &dir_array))) {
        /* Information we want isn't present.  Use inverted file */
        if (UNDEF == (ip->inv_inst = init_con_invidf (spec, param_prefix)))
            return (UNDEF);
        ip->valid_info = 1;
        return (new_inst);
    }
    
    if (1 != (read_dir_array (ip->dir_array_fd, &dir_array)))
        return (UNDEF);

    ip->inv_inst = UNDEF;
    ip->idf = (float *) dir_array.list;
    ip->num_idf = dir_array.num_list / sizeof (float);

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_con_cw_idf");
    return (new_inst);
}

int
con_cw_idf (con, freq, inst)
long *con;
float *freq;
int inst;
{
    STATIC_INFO *ip;
    
    PRINT_TRACE (2, print_string, "Trace: entering con_cw_idf");
    PRINT_TRACE (4, print_long, con);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "con_cw_idf");
        return (UNDEF);
    }
    ip = &info[inst];

    if (ip->inv_inst != UNDEF) {
        /* we are getting our info from the inverted file */
        if (UNDEF == con_invidf (con, freq, ip->inv_inst))
            return (UNDEF);
    }
    else {
        if (*con >= 0 && *con < ip->num_idf)
            *freq = (float) ip->idf[*con];
        else
            *freq = 0.0;
    }

    PRINT_TRACE (4, print_float, freq);
    PRINT_TRACE (2, print_string, "Trace: leaving con_cw_idf");
    return (1);
}
    

int
close_con_cw_idf (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_con_cw_idf");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_con_cw_idf");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;

    if (ip->valid_info == 0) {
        if (ip->inv_inst != UNDEF) {
            /* we are getting our info from the inverted file */
            if (UNDEF == close_con_invidf (ip->inv_inst))
                return (UNDEF);
        }
        else {
            if (UNDEF == close_dir_array (ip->dir_array_fd))
                return (UNDEF);
        }
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_con_cw_idf");
    return (0);
}
