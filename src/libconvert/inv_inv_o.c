#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/inv_inv_o.c,v 11.2 1993/02/03 16:49:35 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Copy a inverted file relational object
 *1 convert.obj.inv_inv_obj
 *2 inv_inv_obj (in_inv_file, out_inv_file, inst)
 *3   char *in_inv_file;
 *3   char *out_inv_file;
 *3   int inst;

 *4 init_inv_inv_obj (spec, unused)
 *5   "convert.inv_inv.inv_file"
 *5   "convert.inv_inv.inv_file.rmode"
 *5   "convert.inv_inv.inv_file.rwcmode"
 *5   "convert.deleted_doc_file"
 *5   "inv_inv_obj.trace"
 *4 close_inv_inv_obj(inst)

 *6 global_start and global_end are checked.  Only those inv lists with
 *6 id_num falling between them are converted.
 *7 Read inv tuples from in_inv_file and copy to out_inv_file.
 *7 If in_inv_file is NULL then inv_file is used.
 *7 If out_inv_file is NULL, then error.
 *7 If deleted_doc_file is a valid file, then it is assumed to be a
 *7 text file composed of docid's to be deleted during the copying process.
 *7 The docid's are assumed to be numerically sorted.
 *7 Return UNDEF if error, else 0;
 *9 WARNING: Use of global_start and global_end is kludgey.  Instead of
 *9 applying to the key_num of the inverted list itself, it is applied to
 *9 the contents of the list.  This will change at some point. For the
 *9 time being, it allows easier programming of document deletion.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "proc.h"
#include "inv.h"
#include "io.h"

static char *default_inv_file;
static long read_mode;
static long write_mode;
static char *deleted_doc_file;

static SPEC_PARAM spec_args[] = {
    {"convert.inv_inv.inv_file",  getspec_dbfile, (char *) &default_inv_file},
    {"convert.inv_inv.inv_file.rmode",getspec_filemode, (char *) &read_mode},
    {"convert.inv_inv.inv_file.rwcmode",getspec_filemode, (char *) &write_mode},
    {"convert.deleted_doc_file",  getspec_dbfile, (char *) &deleted_doc_file},
    TRACE_PARAM ("inv_inv_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


int
init_inv_inv_obj (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_inv_inv_obj");
    PRINT_TRACE (2, print_string, "Trace: leaving init_inv_inv_obj");

    return (0);
}

int
inv_inv_obj (in_inv_file, out_inv_file, inst)
char *in_inv_file;
char *out_inv_file;
int inst;
{
    INV inv;
    int status;
    int in_index, out_index;
    long *del_dids;
    long num_del_dids;
    long did_index;
    LISTWT *buf = NULL;
    LISTWT *new_ptr;
    long num_buf = 0;
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering inv_inv_obj");

    num_del_dids = 0;
    if (VALID_FILE (deleted_doc_file) &&
        UNDEF == text_long_array(deleted_doc_file, &del_dids, &num_del_dids))
        return (UNDEF);

    if (in_inv_file == NULL)
        in_inv_file = default_inv_file;
    if (UNDEF == (in_index = open_inv (in_inv_file, read_mode))){
        return (UNDEF);
    }

    if (! VALID_FILE (out_inv_file)) {
        set_error (SM_ILLPA_ERR, "inv_inv_obj", out_inv_file);
        return (UNDEF);
    }
    if (UNDEF == (out_index = open_inv (out_inv_file, write_mode))){
        return (UNDEF);
    }

    /* Copy each inv list in turn */
    inv.id_num = 0;
    if (global_start > 0) {
        inv.id_num = global_start;
        if (UNDEF == seek_inv (in_index, &inv)) {
            return (UNDEF);
        }
    }

    while (1 == (status = read_inv (in_index, &inv)) &&
           inv.id_num <= global_end) {
        if (num_del_dids > 0) {
            /* Must delete docs appearing in del_dids */
            /* First, make sure enough space is reserved */
            if (inv.num_listwt > num_buf) {
                if (num_buf > 0)
                    (void) free ((char *) buf);
                num_buf += inv.num_listwt;
                if (NULL == (buf = (LISTWT *)
                             malloc ((unsigned) num_buf * sizeof (LISTWT))))
                    return (UNDEF);
            }
            /* Now go through del_dids and listwt lists in parallel (both
               are sorted by increasing did), copying those dids not in
               del_dids */
            did_index = 0;
            new_ptr = buf;
            for (i = 0; i < inv.num_listwt; i++) {
                while (did_index < num_del_dids &&
                       del_dids[did_index] < inv.listwt[i].list)
                    did_index++;
                if (did_index < num_del_dids &&
                    del_dids[did_index] == inv.listwt[i].list)
                    continue;
                new_ptr->list = inv.listwt[i].list;
                new_ptr->wt = inv.listwt[i].wt;
                new_ptr++;
            }
            inv.num_listwt = new_ptr - buf;
            inv.listwt = buf;
        }

        if (inv.num_listwt > 0) {
            if (UNDEF == seek_inv (out_index, &inv) ||
                1 != write_inv (out_index, &inv))
                return (UNDEF);
        }
    }

    if (UNDEF == close_inv (out_index) || 
        UNDEF == close_inv (in_index))
        return (UNDEF);
    if (VALID_FILE (deleted_doc_file) &&
        UNDEF == free_long_array (deleted_doc_file, &del_dids, &num_del_dids))
        return (UNDEF);
    if (num_buf > 0)
        (void) free ((char *) buf);
        
    PRINT_TRACE (2, print_string, "Trace: leaving print_inv_inv_obj");
    return (status);
}

int
close_inv_inv_obj(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_inv_inv_obj");

    PRINT_TRACE (2, print_string, "Trace: entering close_inv_inv_obj");
    return (0);
}
