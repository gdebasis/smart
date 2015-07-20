#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/con_cw_ictf.c,v 11.2 1993/02/03 16:49:29 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Given a concept, return the coll freq for it
 *1 convert.con_collinfo.cf
 *2 con_cw_ictf (con, freq, inst)
 *3   long *con;
 *3   float *freq;
 *3   int inst;
 *4 init_con_cw_ictf (spec, param_prefix)
 *5   "convert.con_collinfo.trace"
 *5   "*.collstat_file"
 *5   "*.collstat_file.rmode"
 *4 close_con_cw_ictf (inst)
 *7 Return the collection frequency value for con, ie number of 
 *7 docs in which the term occurs, determined by the entry for that term
 *7 in the collstat_file.  If collstat_file does not exist, or does not
 *7 have collection frequency info in it, then the value is the number of 
 *7 entries in the inverted list for the term (from con_invfreq()).
 *7 Note that "collstat_file" is a ctype dependant parameter, and is found by
 *7 looking up the value of the parameter formed by concatenating 
 *7 param_prefix and "collstat_file".
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
static char *textloc_file;
static char *doc_file;
static char *collstat_file;
static long collstat_rmode;
static long ictf_num;
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix,"doc.textloc_file",  getspec_dbfile,(char *) &textloc_file},
    {&prefix,"doc.doc_file",      getspec_dbfile,(char *) &doc_file},
    {&prefix,"collstat_file",     getspec_dbfile,    (char *) &collstat_file},
    {&prefix,"collstat_file.rmode", getspec_filemode,(char *) &collstat_rmode},
    {&prefix,"ictf_num", getspec_long,(char *) &ictf_num},
    };
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int dir_array_fd;
    char collstat_file[PATH_LEN];
    float *freq;
    long num_freq;
    float *idf;
    long num_idf;
    float log_ictf_num;

    long num_docs;
    float log_num_docs;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_con_cw_ictf (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    REL_HEADER *rh;
    STATIC_INFO *ip;
    int new_inst;
    long i;
    DIR_ARRAY dir_array, da;

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }
    prefix = param_prefix;
    if (UNDEF == lookup_spec_prefix (spec, &pspec_args[0], num_pspec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_con_cw_ictf");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    /* Need to get the number of docs in the collection (to later be able
       to compute the proportion that have a term).  Check textloc_file. */
    if (VALID_FILE (textloc_file)) {
        if (NULL == (rh = get_rel_header (textloc_file))) {
            set_error (SM_ILLPA_ERR, textloc_file, "init_con_invidf");
            return (UNDEF);
        }
    }
    else if (NULL == (rh = get_rel_header (doc_file))) {
        set_error (SM_ILLPA_ERR, textloc_file, "init_con_invidf");
        return (UNDEF);
    }

    
    ip->num_docs = rh->num_entries + 1;
    ip->log_num_docs = (float) log ((double) ip->num_docs);

    if (VALID_FILE (collstat_file)) {
        /* Check to see if this file_name has already been initialized.  If
           so, that instantiation will be used. */
        for (i = 0; i < max_inst; i++) {
            if (info[i].valid_info &&
                0 == strcmp (collstat_file, info[i].collstat_file)) {
                info[i].valid_info++;
                return (i);
            }
        }
    }
    else
        return (UNDEF);

    (void) strncpy (ip->collstat_file, collstat_file, PATH_LEN);
    dir_array.id_num = COLLSTAT_TOTWT;

    if (UNDEF == (ip->dir_array_fd = open_dir_array (collstat_file,
                                                   collstat_rmode)))
        return (UNDEF);

    if (1 != (seek_dir_array (ip->dir_array_fd, &dir_array)))
            return (UNDEF);
    if (1 != (read_dir_array (ip->dir_array_fd, &dir_array)))
        return (UNDEF);
    ip->freq = (float *) dir_array.list;
    ip->num_freq = dir_array.num_list / sizeof (float);
    ip->log_ictf_num = (float) log ((double) ictf_num);

    da.id_num = COLLSTAT_IDF;
    if (1 != (seek_dir_array (ip->dir_array_fd, &da)))
            return (UNDEF);
    if (1 != (read_dir_array (ip->dir_array_fd, &da)))
        return (UNDEF);
    ip->idf = (float *) da.list;
    ip->num_idf = da.num_list / sizeof (float);

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_con_cw_ictf");
    return (new_inst);
}

int
con_cw_ictf (con, freq, inst)
long *con;
float *freq;
int inst;
{
    STATIC_INFO *ip;
    
    PRINT_TRACE (2, print_string, "Trace: entering con_cw_ictf");
    PRINT_TRACE (4, print_long, con);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "con_cw_ictf");
        return (UNDEF);
    }
    ip = &info[inst];

    if (*con >= 0 && *con < ip->num_freq && *con < ip->num_idf)
        *freq = ip->idf[*con]/ip->log_num_docs +
	    (1.0 - (float) log((double) ip->freq[*con])/ip->log_ictf_num);
    else
        *freq = 0.0;

    PRINT_TRACE (4, print_float, freq);
    PRINT_TRACE (2, print_string, "Trace: leaving con_cw_ictf");
    return (1);
}
    

int
close_con_cw_ictf (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_con_cw_ictf");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_con_cw_ictf");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;

    /* Free buffers if this was last close of this inst */
    if (ip->valid_info == 0) {
        if (UNDEF == close_dir_array (ip->dir_array_fd))
	    return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_con_cw_ictf");
    return (0);
}
