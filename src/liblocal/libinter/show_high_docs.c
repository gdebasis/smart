#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/show_docs.c,v 11.2 1993/02/03 16:51:28 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <ctype.h>
#include <string.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "textloc.h"
#include "io.h"
#include "smart_error.h"
#include "spec.h"
#include "proc.h"
#include "tr_vec.h"
#include "retrieve.h"
#include "docindex.h"
#include "inter.h"
#include "inst.h"
#include "trace.h"

static PROC_TAB *print_proc_ptab;
static long raw_doc_flag, highlightflag;        
static char *dtextloc_file;
static long dtloc_mode;

static SPEC_PARAM spec_args[] = {
    {"inter.print.indivtext",	getspec_func,	(char *) &print_proc_ptab},
    {"inter.print.rawdocflag",	getspec_bool,	(char *) &raw_doc_flag},
    {"inter.print.highlightflag", getspec_bool,	(char *) &highlightflag},
    {"inter.doc.textloc_file",	getspec_dbfile,	(char *) &dtextloc_file},
    {"inter.doc.textloc_file.rmode", getspec_filemode, (char *) &dtloc_mode},
    TRACE_PARAM ("inter.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

int show_high_doc();

#define MAX_TOKENS MAX_COMMAND_LINE*2

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
    SPEC *saved_spec;

    PROC_INST print_proc;
    int util_inst;
    int highlight_inst;
    long raw_doc_flag;
    int dtloc_fd;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_show_high_doc (spec, unused)
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

    PRINT_TRACE (2, print_string, "Trace: entering init_show_high_doc");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    ip->saved_spec = spec;

    /* Initialize procedure for printing a single doc (indicate doc rather
       than query is to be printed by context.
    */

    ip->print_proc.ptab = print_proc_ptab;
    if (UNDEF == (ip->print_proc.inst =
                  ip->print_proc.ptab->init_proc (spec, "doc.")))
        return (UNDEF);

    /* Initialize utility procedures */
    if (UNDEF == (ip->util_inst = init_inter_util (spec, (char *) NULL)))
        return (UNDEF);
    if(UNDEF == (ip->highlight_inst = init_highlight(spec, (char *)NULL)))
      return (UNDEF);
    ip->raw_doc_flag = raw_doc_flag;
    if (UNDEF == (ip->dtloc_fd = open_textloc(dtextloc_file, dtloc_mode)))
        return (UNDEF);

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_show_high_doc");

    return (new_inst);
}


int
close_show_high_doc (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_show_high_doc");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.show_high_doc");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Check to see if still have valid instantiations using this data */
    if (--ip->valid_info > 0)
        return (0);

    if (UNDEF == ip->print_proc.ptab->close_proc (ip->print_proc.inst))
        return (UNDEF);

    if(UNDEF == close_inter_util(ip->util_inst)) return(UNDEF);
    if(UNDEF == close_highlight(ip->highlight_inst)) return(UNDEF);
    if (UNDEF == close_textloc (ip->dtloc_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_show_high_doc");

    return (1);
}

int
show_high_next_doc (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    TR_TUP *tr_tup;
    STATIC_INFO *ip;
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering show_high_next_doc");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.show_high_next_doc");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (++is->current_doc > is->retrieval.tr->num_tr) {
        if (UNDEF == add_buf_string ("No more docs!\n", &is->err_buf))
            return (UNDEF);
        return (0);
    }
    tr_tup = &is->retrieval.tr->tr[is->current_doc-1];
    if (tr_tup->action == ' ' || tr_tup->action == '\0')
        tr_tup->action = '*';

    status = show_high_doc (is, tr_tup->did, inst);

    PRINT_TRACE (2, print_string, "Trace: leaving show_high_next_doc");

    return (status);

}

int
show_high_named_doc (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    STATIC_INFO *ip;
    long docid, i;
    int status;
    char temp_buf[PATH_LEN]; 

    PRINT_TRACE (2, print_string, "Trace: entering show_high_named_doc");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.show_high_high_doc");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (is->num_command_line < 2) {
        if (UNDEF == add_buf_string ("No document specified\n", &is->err_buf))
            return (UNDEF);
        return (0);
    }
    for (i = 1; i < is->num_command_line; i++) {
	docid = atol (is->command_line[i]);
	sprintf(temp_buf, "\n--------\n%s\n--------\n", is->command_line[i]);
	if (UNDEF == add_buf_string (temp_buf, &is->output_buf))
            return (UNDEF);
	status = show_high_doc (is, docid, inst);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving show_high_named_doc");

    return (status);
}

int
show_high_current_doc (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    TR_TUP *tr_tup;
    STATIC_INFO *ip;
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering show_high_next_doc");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.show_high_doc");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (is->current_doc == 0)
        is->current_doc = 1;
    if (is->current_doc > is->retrieval.tr->num_tr) {
        if (UNDEF == add_buf_string ("Illegal document specified\n",
                                     &is->err_buf))
            return (UNDEF);
        return (0);
    }
    tr_tup = &is->retrieval.tr->tr[is->current_doc-1];

    if (tr_tup->action == ' ' || tr_tup->action == '\0')
        tr_tup->action = '*';

    status = show_high_doc (is, tr_tup->did, inst);

    PRINT_TRACE (2, print_string, "Trace: leaving show_high_current_doc");

    return (status);
}

int
show_high_doc (is, docid, inst)
INTER_STATE *is;
long docid;
int inst;
{
    TEXTLOC textloc;
    char temp_buf[PATH_LEN];
    STATIC_INFO *ip;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.show_high_doc");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (highlightflag &&
	is->retrieval.query->qid != UNDEF) {
	/* query is defined; have to highlight query terms */
	if (UNDEF == inter_get_hilite_textloc(is, docid, &textloc, 
					      ip->util_inst)) {
	    sprintf(temp_buf, "Couldn't highlight document %ld\n", docid);
	    if (UNDEF == add_buf_string (temp_buf, &is->err_buf))
		return (UNDEF);
	    return (0);
	}
    }
    else {
	if (UNDEF == inter_get_textloc (is, docid, &textloc, ip->util_inst)) {
	    (void) sprintf(temp_buf, "Can't find document %ld\n", docid);
	    if (UNDEF == add_buf_string (temp_buf, &is->err_buf))
		return (UNDEF);
	    return (0);
	}
    }

    if (ip->raw_doc_flag)
        print_int_textloc (&textloc, &is->output_buf);
    else 
	if(UNDEF == ip->print_proc.ptab->proc (&textloc, &is->output_buf,
					       ip->print_proc.inst)) 
	    return (UNDEF);

    return (1);
}


/******************************************************************
 *
 * Get the textloc for a docid with the query terms highlighted. Note 
 * that we have to ensure it's expanded before we try to find the textloc 
 * (which is stored only by full docid, not a part).
 *
 ******************************************************************/
int
inter_get_hilite_textloc (is, did, textloc, inst)
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
    if (1 != seek_textloc (ip->dtloc_fd, textloc) ||
        1 != read_textloc (ip->dtloc_fd, textloc))
        return (UNDEF);

    if (UNDEF == highlight(is->retrieval.query->vector, textloc, 
			   ip->highlight_inst))
	return UNDEF;

    return (1);
}
