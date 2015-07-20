#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/show_docs.c,v 11.2 1993/02/03 16:51:28 smart Exp $";
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
#include "smart_error.h"
#include "spec.h"
#include "proc.h"
#include "context.h"
#include "eid.h"
#include "retrieve.h"
#include "tr_vec.h"
#include "docindex.h"
#include "inter.h"
#include "textloc.h"
#include "inst.h"
#include "trace.h"
 
static long raw_doc_flag;        
static PROC_TAB *print_proc_ptab;
 
static SPEC_PARAM spec_args[] = {
    {"inter.print.indivtext",     getspec_func,    (char *) &print_proc_ptab},
    {"inter.print.rawdocflag",    getspec_bool,    (char *) &raw_doc_flag},
    TRACE_PARAM ("inter.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);
 
static int show_doc();
 
#define MAX_TOKENS MAX_COMMAND_LINE*2
 
/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
    SPEC *saved_spec;
 
    PROC_INST print_proc;
    int util_inst;
    long raw_doc_flag;
} STATIC_INFO;
 
static STATIC_INFO *info;
static int max_inst = 0;
 
int
init_show_doc (spec, unused)
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
 
    PRINT_TRACE (2, print_string, "Trace: entering init_show_doc");
 
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
 
    ip->raw_doc_flag = raw_doc_flag;
 
    ip->valid_info = 1;
 
    PRINT_TRACE (2, print_string, "Trace: leaving init_show_doc");
 
    return (new_inst);
}
 
 
int
close_show_doc (inst)
int inst;
{
    STATIC_INFO *ip;
 
    PRINT_TRACE (2, print_string, "Trace: entering close_show_doc");
 
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.show_doc");
        return (UNDEF);
    }
    ip  = &info[inst];
 
    /* Check to see if still have valid instantiations using this data */
    if (--ip->valid_info > 0)
        return (0);
 
    if (UNDEF == ip->print_proc.ptab->close_proc (ip->print_proc.inst))
        return (UNDEF);
 
    if (UNDEF == close_inter_util (ip->util_inst))
        return (UNDEF);
 
    PRINT_TRACE (2, print_string, "Trace: leaving close_show_doc");
 
    return (1);
}
 
int
show_next_doc (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    TR_TUP *tr_tup;
    STATIC_INFO *ip;
    int status;
 
    PRINT_TRACE (2, print_string, "Trace: entering show_next_doc");
 
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.show_next_doc");
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
 
    status = show_doc (is, tr_tup->did, inst);
 
    PRINT_TRACE (2, print_string, "Trace: leaving show_next_doc");
 
    return (status);
 
}
 
int
show_named_doc (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    STATIC_INFO *ip;
    long docid, i;
    int status = 0;
 
    PRINT_TRACE (2, print_string, "Trace: entering show_named_doc");
 
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.show_doc");
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
	status = show_doc (is, docid, inst);
    }
 
    PRINT_TRACE (2, print_string, "Trace: leaving show_named_doc");
 
    return (status);
}
 
int
show_current_doc (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    TR_TUP *tr_tup;
    STATIC_INFO *ip;
    int status;
 
    PRINT_TRACE (2, print_string, "Trace: entering show_next_doc");
 
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.show_doc");
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
 
    status = show_doc (is, tr_tup->did, inst);
 
    PRINT_TRACE (2, print_string, "Trace: leaving show_current_doc");
 
    return (status);
}
 
static int
show_doc (is, docid, inst)
INTER_STATE *is;
long docid;
int inst;
{
    TEXTLOC textloc;
    char temp_buf[PATH_LEN];
    STATIC_INFO *ip;
    EID eid;
 
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.show_doc");
        return (UNDEF);
    }
    ip  = &info[inst];
 
    if (ip->raw_doc_flag) {
        if (UNDEF == inter_get_textloc (is, docid, &textloc, ip->util_inst)) {
            (void) sprintf (temp_buf, "Can't find document %ld\n", docid);
            if (UNDEF == add_buf_string (temp_buf, &is->err_buf))
                return (UNDEF);
            return (0);
        }
        print_int_textloc (&textloc, &is->output_buf);
    }
    else {
        eid.id = docid;
        EXT_NONE(eid.ext);
 
        if (UNDEF == ip->print_proc.ptab->proc (&eid,
                                             &is->output_buf,
                                             ip->print_proc.inst)) {
            return (UNDEF);
        }
    }
 
    return (1);
}
 
 
int
show_query (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering show_query");
 
    if (is->num_command_line > 1) {
        if (UNDEF==add_buf_string(
                            "Command does not take an argument; ignored\n",
                            &is->err_buf ))
            return UNDEF;
        return 0;
    }
 
    if (is->query_text.end == 0) {
        if (UNDEF == add_buf_string ("No query defined\n", &is->err_buf))
            return (UNDEF);
        return (0);
    }
 
    print_buf (&is->query_text, &is->output_buf);
    PRINT_TRACE (2, print_string, "Trace: leaving show_query");
    return (1);
}
 
 
 
int
set_raw_doc (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    STATIC_INFO *ip;
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.toggle_raw_doc");
        return (UNDEF);
    }
    ip  = &info[inst];
 
    ip->raw_doc_flag = 1;
    if (UNDEF == add_buf_string ("Raw doc will be printed",
                                 &is->output_buf))
        return (UNDEF);
    return (1);
}
 
 
int
set_format_doc (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    STATIC_INFO *ip;
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.toggle_formatted_doc");
        return (UNDEF);
    }
    ip  = &info[inst];
 
    ip->raw_doc_flag = 0;
    if (UNDEF == add_buf_string ("Formatted doc will be printed",
                                 &is->output_buf))
        return (UNDEF);
    return (1);
}
