#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/inter_util.c,v 11.2 1993/02/03 16:51:24 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "textloc.h"
#include "io.h"
#include "smart_error.h"
#include "spec.h"
#include "vector.h"
#include "inter.h"
#include "trace.h"
#include "inst.h"

static char *dtextloc_file;
static long dtextloc_mode;

static SPEC_PARAM spec_args[] = {
    {"inter.doc.textloc_file",  getspec_dbfile,     (char *) &dtextloc_file},
    {"inter.doc.textloc_file.rmode", getspec_filemode, (char *) &dtextloc_mode},
    TRACE_PARAM ("inter.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    SPEC *saved_spec;
    int dtextloc_fd;
    int did_vec_inst;    
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int
init_inter_util (spec, unused)
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

    PRINT_TRACE (2, print_string, "Trace: entering init_inter_util");

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    ip->saved_spec = spec;

    /* Open Textloc file to get file location and section offsets for did */
    if (UNDEF == (ip->dtextloc_fd = open_textloc(dtextloc_file,dtextloc_mode)))
        return (UNDEF);

    if (UNDEF == (ip->did_vec_inst = init_did_vec(spec, (char *) NULL)))
	return (UNDEF);

    ip->valid_info = 1;
    return (new_inst);
}

int
close_inter_util (inst)
int inst;
{
    STATIC_INFO *ip;
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.util");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Check to see if still have valid instantiations using this data */
    if (--ip->valid_info > 0)
        return (0);

    PRINT_TRACE (2, print_string, "Trace: entering close_inter_util");

    if (UNDEF == close_textloc (ip->dtextloc_fd))
        return (UNDEF);
    if (UNDEF == close_did_vec (ip->did_vec_inst))
        return (UNDEF);

    return (0);
}

/******************************************************************
 *
 * Get the textloc for a docid.  Note that we have to ensure it's
 * expanded before we try to find the textloc (which is stored only
 * by full docid, not a part).
 *
 ******************************************************************/
int
inter_get_textloc (is, did, textloc, inst)
INTER_STATE *is;
long did;
TEXTLOC *textloc;
int inst;
{
    STATIC_INFO *ip;
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.util");
        return (UNDEF);
    }
    ip  = &info[inst];

    textloc->id_num = did;
    if (1 != seek_textloc (ip->dtextloc_fd, textloc) ||
        1 != read_textloc (ip->dtextloc_fd, textloc))
        return (UNDEF);

    return (1);
}

/******************************************************************
 *
 * Get the title for a docid.
 *
 ******************************************************************/
int
inter_get_title (is, did, title, inst)
INTER_STATE *is;
long did;
char **title;
int inst;
{
    STATIC_INFO *ip;
    TEXTLOC textloc;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.util");
        return (UNDEF);
    }
    ip  = &info[inst];

    textloc.id_num = did;
    if (1 != seek_textloc (ip->dtextloc_fd, &textloc) ||
        1 != read_textloc (ip->dtextloc_fd, &textloc))
        return (UNDEF);
    *title = textloc.title;

    return (1);
}

/******************************************************************
 *
 * Get a vector for a string docid
 *
 ******************************************************************/
int
inter_get_vec (didstring, vec, inst)
char *didstring;
VEC *vec;
int inst;
{
    STATIC_INFO *ip;
    long did;
    
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.util");
        return (UNDEF);
    }
    ip  = &info[inst];

    did = atol (didstring);
    return (did_vec (&did, vec, ip->did_vec_inst));
}


