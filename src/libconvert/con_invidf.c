#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/con_invidf.c,v 11.2 1993/02/03 16:49:44 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Given a concept, return idf value for it (derived from inverted file)
 *1 convert.con_collinfo.invidf
 *2 con_invidf (con, idf, inst)
 *3   long *con;
 *3   float *idf;
 *3   int inst;
 *4 init_con_invidf (spec, param_prefix)
 *5   "convert.con_collinfo.trace"
 *5   "doc.textloc_file"
 *5   "*.inv_file"
 *5   "*.inv_file.rmode"
 *4 close_con_invidf (inst)
 *7 Return the idf value for con, ie log (N / n).
 *7 N is the number of docs in the collection, determined from the number
 *7 of entries in "textloc_file".  n is the number of docs in which the term
 *7 occurs, determined by the number of entries in the inverted list for
 *7 the term (from "inv_file").
 *7 Note that "inv_file" is a ctype dependant parameter, and is found by
 *7 looking up the value of the parameter formed by concatenating 
 *7 param_prefix and "inv_file".
 *8 The idf values for a particular inv_file are cached to avoid excessive
 *8 references to the inverted file.
 *9 May want to have cache values stay around after close for efficiency.
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

static char *textloc_file;
static char *doc_file;
static SPEC_PARAM spec_args[] = {
    {"doc.textloc_file",           getspec_dbfile,(char *) &textloc_file},
    {"doc.doc_file",               getspec_dbfile,(char *) &doc_file},
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

    double num_docs;
    double log_num_docs;
    int inv_fd;
    float *idf_cache;
    long num_idf_cache;
    char file_name[PATH_LEN];
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_con_invidf (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    REL_HEADER *rh;
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

    PRINT_TRACE (2, print_string, "Trace: entering init_con_invidf");

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

    
    ip->num_docs = (double) (rh->num_entries + 1);
    ip->log_num_docs = log (ip->num_docs);

    /* Get occurrence information from inverted file.  For efficiency,
       should only read fixed part of inv_file, but for now read entire
       thing */
    if (UNDEF == (ip->inv_fd = open_inv (inv_file, inv_mode))) {
        return (UNDEF);
    }

    /* Reserve space for freq_cache */
    if (NULL == (rh = get_rel_header (inv_file))) {
        set_error (SM_ILLPA_ERR, inv_file, "init_con_invidf");
        return (UNDEF);
    }
    ip->num_idf_cache = rh->max_primary_value + 1;
    if (NULL == (ip->idf_cache = (float *)
                 calloc ((unsigned) ip->num_idf_cache, sizeof (float))))
        return (UNDEF);

    (void) strncpy (ip->file_name, inv_file, PATH_LEN);
    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_con_invidf");
    return (new_inst);
}

int
con_invidf (con, idf, inst)
long *con;
float *idf;
int inst;
{
    STATIC_INFO *ip;
    INV inv;
    float term_idf;
    
    PRINT_TRACE (2, print_string, "Trace: entering con_invidf");
    PRINT_TRACE (4, print_long, con);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "con_invidf");
        return (UNDEF);
    }
    ip = &info[inst];

    if (*con < ip->num_idf_cache &&
        ip->idf_cache[*con] != 0.0) {
        *idf = ip->idf_cache[*con];
    }
    else {
        inv.id_num = *con;
        term_idf = 0.0;
        if (1 == seek_inv (ip->inv_fd, &inv) &&
            1 == read_inv (ip->inv_fd, &inv) &&
            inv.num_listwt > 0) {
            term_idf = log ((double) inv.num_listwt);
        }
        *idf = ip->log_num_docs - term_idf;
        if (*con < ip->num_idf_cache)
            ip->idf_cache[*con] = *idf;
    }

    PRINT_TRACE (4, print_float, idf);
    PRINT_TRACE (2, print_string, "Trace: leaving con_invidf");
    return (1);
}
    

int
close_con_invidf (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_con_invidf");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_con_invidf");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;
    /* Free buffers if this was last close of this inst */
    if (ip->valid_info == 0) {
        if (UNDEF == close_inv (ip->inv_fd))
            return (UNDEF);
        (void) free ((char *) ip->idf_cache);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_con_invidf");
    return (0);
}
