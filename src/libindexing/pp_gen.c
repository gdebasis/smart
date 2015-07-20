#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/pp_generic.c,v 11.0 1992/07/21 18:21:42 chrisb Exp $";
#endif
/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Generic pre-parser for documents with sections.
 *1 index.preparse.named_text.generic
 *1 index.preparse.next_text.generic
 *2 pp_named_text(doc_id, output_doc, inst)
 *3   EID *doc_id;
 *3   SM_INDEX_TEXTDOC *output_doc;
 *3   int inst;
 *2 pp_next_text(unused, output_doc, inst)
 *3   char *unused;
 *3   SM_INDEX_TEXTDOC *output_doc;
 *3   int inst;
 *4 init_pp_named_text(spec_ptr, qd_prefix)
 *4 init_pp_next_text(spec_ptr, qd_prefix)
 *5    "index.preparse.trace"
 *5    "*.named_get_text"
 *5    "*.next_get_text"
 *5    "*.preparser"
 *4 close_pp_named_text(inst)
 *4 close_pp_next_text(inst)

 *7 Put a preparsed document in output_doc which corresponds to either
 *7 the doc_id or the next document found from the list of documents in
 *7 a list file.  Normally qd_prefix will be "doc." or "query." which will 
 *7 be used to determine and call parameterized text-fetcher and preparser.
 *7 Returns 1 if found and preparsed doc, 0 if no more docs, UNDEF if error.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "spec.h"
#include "preparser.h"
#include "smart_error.h"
#include "functions.h"
#include "docindex.h"
#include "trace.h"
#include "context.h"
#include "inst.h"
#include "proc.h"


static SPEC_PARAM pp[] = {
    TRACE_PARAM ("index.preparse.trace")
};
static int num_pp = sizeof(pp) / sizeof(pp[0]);

static char *qord_prefix;
static PROC_INST named_proc, next_proc, pp_proc;
static SPEC_PARAM_PREFIX qord_spec_args[] = {
    { &qord_prefix, "named_get_text",getspec_func,  (char *)&named_proc.ptab },
    { &qord_prefix, "next_get_text", getspec_func,  (char *)&next_proc.ptab },
    { &qord_prefix, "preparser",     getspec_func,  (char *)&pp_proc.ptab },
};
static int num_qord_spec_args = sizeof(qord_spec_args) /
				sizeof(qord_spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    PROC_INST textproc;		/* procedures to fetch text */
    PROC_INST pp_proc;		/* procedures to preparse text */
    SM_BUF textbuf;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int
init_pp_named_text(spec, qd_prefix)
SPEC *spec;
char *qd_prefix;
{
    STATIC_INFO *ip;
    int new_inst;

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    if (UNDEF == lookup_spec (spec, &pp[0], num_pp))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_pp_named_text");

    qord_prefix = qd_prefix;
    if (UNDEF == lookup_spec_prefix(spec, &qord_spec_args[0],
				    num_qord_spec_args))
	return(UNDEF);
    
    /* initialize the preparser */
    ip->pp_proc.ptab = pp_proc.ptab;
    ip->pp_proc.inst = pp_proc.ptab->init_proc(spec, qd_prefix);
    if (UNDEF == ip->pp_proc.inst)
        return(UNDEF);

    /* initialize the text fetcher */
    ip->textproc.ptab = named_proc.ptab;
    ip->textproc.inst = named_proc.ptab->init_proc(spec, qd_prefix);
    if (UNDEF == ip->textproc.inst)
        return (UNDEF);
        
    PRINT_TRACE (2, print_string, "Trace: leaving init_pp_named_text");

    ip->valid_info = 1;

    return (new_inst);
}

int
init_pp_next_text(spec, qd_prefix)
SPEC *spec;
char *qd_prefix;
{
    STATIC_INFO *ip;
    int new_inst;

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    if (UNDEF == lookup_spec (spec, &pp[0], num_pp))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_pp_next_text");

    qord_prefix = qd_prefix;
    if (UNDEF == lookup_spec_prefix(spec, &qord_spec_args[0],
				    num_qord_spec_args))
	return(UNDEF);
    
    /* initialize the preparser */
    ip->pp_proc.ptab = pp_proc.ptab;
    ip->pp_proc.inst = pp_proc.ptab->init_proc(spec, qd_prefix);
    if (UNDEF == ip->pp_proc.inst)
        return(UNDEF);

    /* initialize the text fetcher */
    ip->textproc.ptab = next_proc.ptab;
    ip->textproc.inst = next_proc.ptab->init_proc(spec, qd_prefix);
    if (UNDEF == ip->textproc.inst)
        return (UNDEF);
    
    ip->textbuf.end = 0;
    ip->textbuf.buf = (char *)0;
        
    PRINT_TRACE (2, print_string, "Trace: leaving init_pp_next_text");

    ip->valid_info = 1;

    return (new_inst);
}

int
pp_named_text(doc_id, output_doc, inst)
EID *doc_id;
SM_INDEX_TEXTDOC *output_doc;
int inst;
{
    STATIC_INFO *ip;
    int status;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "pp_named_text");
        return (UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering pp_named_text");

    /* get text, setting up char * and TEXTLOC part of output_doc */
    status = ip->textproc.ptab->proc(doc_id, output_doc, ip->textproc.inst);
    if (UNDEF == status)
        return (UNDEF);
    else if (0 == status) {
        PRINT_TRACE (2, print_string, "Trace: leaving pp_named_text");
        return (0);
    }
    
    /* copy from TEXTLOC to other output places */
    output_doc->id_num.id = output_doc->textloc_doc.id_num;
    EXT_NONE(output_doc->id_num.ext);
    output_doc->mem_doc.id_num.id = output_doc->textloc_doc.id_num;
    EXT_NONE(output_doc->mem_doc.id_num.ext);
    output_doc->mem_doc.file_name = output_doc->textloc_doc.file_name;
    output_doc->mem_doc.title = output_doc->textloc_doc.title;

    /* translate to SM_BUF for preparser */
    ip->textbuf.end = output_doc->textloc_doc.end_text -
		      output_doc->textloc_doc.begin_text;
    ip->textbuf.size = ip->textbuf.end; /* a minimum, but that's ok */
    ip->textbuf.buf = output_doc->doc_text;

    /* preparse text, setting up SM_DISPLAY part of output_doc */
    status = ip->pp_proc.ptab->proc(&(ip->textbuf), &(output_doc->mem_doc),
					ip->pp_proc.inst);
    if (UNDEF == status)
        return (UNDEF);
    else if (0 == status) {
        PRINT_TRACE (2, print_string, "Trace: leaving pp_named_text");
        return (0);
    }

    PRINT_TRACE (4, print_int_textdoc, output_doc);
    PRINT_TRACE (2, print_string, "Trace: leaving pp_named_text");

    return (1);
}

int
pp_next_text(unused, output_doc, inst)
char *unused;
SM_INDEX_TEXTDOC *output_doc;
int inst;
{
    STATIC_INFO *ip;
    int status;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "pp_next_text");
        return (UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering pp_next_text");

    /* get text, setting up char * and TEXTLOC part of output_doc
     * (and telling text-fetcher how much preparser took last time)
     */
    status = ip->textproc.ptab->proc(&(ip->textbuf), output_doc,
							ip->textproc.inst);
    if (UNDEF == status)
        return (UNDEF);
    else if (0 == status) {
        PRINT_TRACE (2, print_string, "Trace: leaving pp_next_text");
        return (0);
    }
    
    /* copy from TEXTLOC to other output places */
    output_doc->id_num.id = output_doc->textloc_doc.id_num;
    EXT_NONE(output_doc->id_num.ext);
    output_doc->mem_doc.id_num.id = output_doc->textloc_doc.id_num;
    EXT_NONE(output_doc->mem_doc.id_num.ext);
    output_doc->mem_doc.file_name = output_doc->textloc_doc.file_name;
    output_doc->mem_doc.title = output_doc->textloc_doc.title;

    /* translate to SM_BUF for preparser */
    ip->textbuf.end = output_doc->textloc_doc.end_text -
		      output_doc->textloc_doc.begin_text;
    ip->textbuf.size = ip->textbuf.end; /* a minimum, but that's ok */
    ip->textbuf.buf = output_doc->doc_text;

    /* preparse text, setting up SM_DISPLAY part of output_doc */
    status = ip->pp_proc.ptab->proc(&(ip->textbuf), &(output_doc->mem_doc),
					ip->pp_proc.inst);
    if (UNDEF == status)
        return (UNDEF);
    else if (0 == status) {
        PRINT_TRACE (2, print_string, "Trace: leaving pp_next_text");
        return (0);
    }

    /* reset TEXTLOC size from SM_BUF, so caller gets right size */
    output_doc->textloc_doc.end_text = output_doc->textloc_doc.begin_text
					+ ip->textbuf.end;

    PRINT_TRACE (4, print_int_textdoc, output_doc);
    PRINT_TRACE (2, print_string, "Trace: leaving pp_next_text");

    return (1);
}

int
close_pp_named_text(inst)
int inst;
{
    STATIC_INFO *ip;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_pp_named_text");
        return (UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering close_pp_named_text");
    ip->valid_info--;
    if (0 == ip->valid_info) {
	if (UNDEF == ip->textproc.ptab->close_proc(ip->textproc.inst))
	    return(UNDEF);
	if (UNDEF == ip->pp_proc.ptab->close_proc(ip->pp_proc.inst))
	    return(UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_pp_named_text");

    return (0);
}

int
close_pp_next_text(inst)
int inst;
{
    STATIC_INFO *ip;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_pp_next_text");
        return (UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering close_pp_next_text");
    ip->valid_info--;
    if (0 == ip->valid_info) {
	if (UNDEF == ip->textproc.ptab->close_proc(ip->textproc.inst))
	    return(UNDEF);
	if (UNDEF == ip->pp_proc.ptab->close_proc(ip->pp_proc.inst))
	    return(UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_pp_next_text");

    return (0);
}
