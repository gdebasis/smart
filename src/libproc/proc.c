#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libproc/proc.c,v 11.2 1993/02/03 16:53:39 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Root of hierarchy table giving ohter tables of procedures
 *1 root
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure table name to table addresses.
 *8 Current possibilities are "top", "exp", "index","convert", "retrieve",
 *8 "evaluate", "feedback", "print", 
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "proc.h"
#include "spec.h"

extern TAB_PROC_TAB proc_inter[];
extern int num_proc_inter;
extern TAB_PROC_TAB proc_index[];
extern int num_proc_index;
extern TAB_PROC_TAB proc_convert[];
extern int num_proc_convert;
extern TAB_PROC_TAB proc_retrieve[];
extern int num_proc_retrieve;
extern TAB_PROC_TAB proc_print[];
extern int num_proc_print;
extern PROC_TAB proc_exp[];
extern int num_proc_exp;
extern TAB_PROC_TAB proc_feedback[];
extern int num_proc_feedback;
extern TAB_PROC_TAB proc_eval[];
extern int num_proc_eval;

#ifdef LIBLOCAL
extern TAB_PROC_TAB lproc_root[];
extern int num_lproc_root;
#endif  /* LIBLOCAL */

TAB_PROC_TAB proc_root[] = {
    {"inter",	TPT_TAB,	proc_inter, 	NULL,	&num_proc_inter},
    {"index",	TPT_TAB,	proc_index,	NULL,	&num_proc_index},
    {"convert",	TPT_TAB,	proc_convert,   NULL,	&num_proc_convert},
    {"retrieve",TPT_TAB,	proc_retrieve,  NULL,	&num_proc_retrieve},
    {"eval",	TPT_TAB,	proc_eval,	NULL,	&num_proc_eval},
    {"feedback",TPT_TAB,	proc_feedback,  NULL,	&num_proc_feedback},
    {"print",	TPT_TAB,	proc_print,     NULL,	&num_proc_print},
    {"exp",	TPT_PROC,	NULL,		proc_exp,&num_proc_exp},
#ifdef LIBLOCAL
    {"local",	TPT_TAB,	lproc_root,	NULL,	&num_lproc_root},
#endif  /* LIBLOCAL */
    };

int num_proc_root = sizeof (proc_root) / sizeof (proc_root[0]);

/* proc_root_root is declared in the main procedure of any program
   that makes use of any procedures from the procedure hierarchy */
/*  TAB_PROC_TAB proc_root_root = {
 *   "root",     TPT_TAB,        proc_root,      NULL,   &num_proc_root
 *   };
 */

extern TAB_PROC_TAB proc_root_root;

/* Leading dotted components of spec_value are the procedure path. The
   last component is the procedure instantiation. The PROC_TAB entry
   returned is that for the field corresponding to the instantiation in
   the table corresponding to the path */
int
getspec_func (spec_ptr, spec_param, spec_value, proc_ptr)
SPEC *spec_ptr;
char *spec_param;
char *spec_value;
PROC_TAB **proc_ptr;
{
    TAB_PROC_TAB *ptr, *optr;
    PROC_TAB *pptr;
    char *start_path;            /* Beginning of current component */
    char *path;                  /* End of current component (includes .) */
    long i;

    if (NULL == spec_value || spec_value[0] == '\0') {
        return (UNDEF);
    }

    optr = &proc_root_root;
    path = spec_value;
    start_path = path;
    while (*path && *path != '.') {
        path++;
    }

    /* Two ways of ending loop - path == NULL in which case
       are dealing with last component, which is instantiation, or
       we run out of procedure table chains, which is an error if
       path is not NULL.  Note it is legal for us to be at an
       intermediate point in the procedure table chain and path
       to be NULL.
     */
    while (*path && optr->type & TPT_TAB) {
        ptr = optr->tab_proc_tab;

        for (i = 0; i < *optr->num_entries; i++) {
            if (0 == strncmp (ptr[i].name, start_path, path - start_path)) {
                break;
            }
        }
        if (i >= *optr->num_entries) {
            return (UNDEF);
        }
        optr = &ptr[i];
        path++;
        start_path = path;
        while (*path && *path != '.') {
            path++;
        }
    }

    if (NULL == optr->proc_tab) {
        /* Procedure path didn't yield a leaf table */
        return (UNDEF);
    }

    pptr = optr->proc_tab;

    for (i = 0; i < *optr->num_entries; i++) {
        if (0 == strcmp (pptr[i].name, start_path)) {
            break;
        }
    }
    if (i >= *optr->num_entries) {
        return (UNDEF);
    }
    
    *proc_ptr = &pptr[i];
    return (0);
}

