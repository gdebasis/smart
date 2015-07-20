#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/pobj_text_eid.c,v 11.2 1993/02/03 16:54:10 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 print texts by eid using textloc_files
 *1 print.obj.doctexts
 *1 print.obj.querytexts
 *2 print_texts_eid (unused, out_file, inst)
 *3   char *unused;
 *3   char *out_file;
 *3   int inst;
 *4 init_print_doctexts_eid (spec, unused)
 *4 init_print_querytexts_eid (spec, unused)
 *5   "print.doc.indivtext"
 *5   "print.query.indivtext"
 *5   "print.trace"
 *4 close_print_texts_eid (inst)
 *6 global_start,global_end used to indicate what range of eids will have
 *6 their texts printed.
 *7 All texts (modulo global_start, global_end) in doc/query textloc_file will
 *7 be printed to file "out_file" (if not VALID_FILE, then stdout).
 *8 Procedure indivtext gives format of text output, and is expected to be
 *8 print.indivtext.text_pp_named.
***********************************************************************/

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "buf.h"
#include "eid.h"
#include "inst.h"

static char *prefix;
static PROC_INST indivtext;

static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    { &prefix, "indivtext",     getspec_func,   (char *) &indivtext.ptab},
    };
static int num_prefix_spec_args = sizeof (prefix_spec_args) /
         sizeof (prefix_spec_args[0]);

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("print.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    long tloc_limit;
    PROC_INST indivtext;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int init_print_texts_eid();
extern long textloc_limit();

int
init_print_doctexts_eid(spec, unused)
SPEC *spec;
char *unused;
{
    return(init_print_texts_eid(spec, "doc."));
}

int
init_print_querytexts_eid(spec, unused)
SPEC *spec;
char *unused;
{
    return(init_print_texts_eid(spec, "query."));
}

static int
init_print_texts_eid(spec, qd_prefix)
SPEC *spec;
char *qd_prefix;
{
    char prefix_string[PATH_LEN];
    STATIC_INFO *ip;
    int new_inst;

    NEW_INST(new_inst);
    if (UNDEF == new_inst)
	return(UNDEF);
    
    ip = &info[new_inst];

    if (UNDEF == lookup_spec(spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_print_texts_eid");

    prefix = prefix_string;
    (void) sprintf(prefix_string, "print.%s", qd_prefix);
    if (UNDEF == (lookup_spec_prefix(spec, &prefix_spec_args[0],
					num_prefix_spec_args)))
	return(UNDEF);

    /* Call all initialization procedures */
    ip->tloc_limit = textloc_limit(spec, qd_prefix);
    if (UNDEF == ip->tloc_limit)
	return (UNDEF);
    ip->indivtext.ptab = indivtext.ptab;
    ip->indivtext.inst = indivtext.ptab->init_proc (spec, qd_prefix);
    if (UNDEF == ip->indivtext.inst)
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_print_texts_eid");

    ip->valid_info = 1;
    return(new_inst);
}

int
print_texts_eid(unused, out_file, inst)
char *unused;
char *out_file;
int inst;
{
    STATIC_INFO *ip;
    int status;
    EID text_id;
    FILE *output;
    SM_BUF output_buf;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "print_texts_eid");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering print_texts_eid");

    output = VALID_FILE (out_file) ? fopen (out_file, "w") : stdout;
    if (NULL == output)
        return (UNDEF);
    output_buf.size = 0;

    /* Get each document in turn */
    if (global_start > 0) {
        text_id.id = global_start;
    } else
	text_id.id = 0;
    EXT_NONE(text_id.ext);

    status = UNDEF;
    while (text_id.id <= ip->tloc_limit && text_id.id <= global_end) {
	output_buf.end = 0;
        status = ip->indivtext.ptab->proc (&text_id, &output_buf,
							ip->indivtext.inst);
	text_id.id++;
	if (0 == status)
	    continue;
	if (1 != status)
            return (UNDEF);
        (void) fwrite (output_buf.buf, 1, output_buf.end, output);
    }

    if (output != stdin)
        (void) fclose (output);

    PRINT_TRACE (2, print_string, "Trace: leaving print_texts_eid");
    return (status);
}


int
close_print_texts_eid (inst)
int inst;
{
    STATIC_INFO *ip;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "close_print_texts_eid");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering close_print_texts_eid");

    if (UNDEF == ip->indivtext.ptab->close_proc(ip->indivtext.inst))
	return (UNDEF);
    ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_print_texts_eid");
    return (0);
}
