#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/ph_text_fil.c,v 11.2 1993/02/03 16:54:21 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <fcntl.h>
#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "docindex.h"
#include "trace.h"
#include "io.h"
#include "buf.h"

static PROC_INST get_text;

static SPEC_PARAM doc_spec_args[] = {
    {"print.named_get_text", getspec_func, (char *) &get_text.ptab},
    TRACE_PARAM ("print.indiv.trace")
    };
static int num_doc_spec_args = sizeof (doc_spec_args) /
         sizeof (doc_spec_args[0]);

int
init_print_text_filter (spec, qd_prefix)
SPEC *spec;
char *qd_prefix;
{
    char prefix[PATH_LEN];

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &doc_spec_args[0],
                              num_doc_spec_args)) {
        return (UNDEF);
    }
    PRINT_TRACE (2, print_string, "Trace: entering init_print_text_filter");
    (void) sprintf(prefix, "print.indiv.%s", qd_prefix);

    if (UNDEF == (get_text.inst = get_text.ptab->init_proc(spec, prefix))) {
	return(UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_print_text_filter");
    return (0);
}

int
print_text_filter (text_id, output, inst)
EID *text_id;
SM_BUF *output;
int inst;
{
    SM_INDEX_TEXTDOC text;
    SM_BUF tmp_buf;

    PRINT_TRACE (2, print_string, "Trace: entering print_text_filter");

    if (1 != get_text.ptab->proc(text_id, &text, get_text.inst)) {
	(void) fprintf(stderr, "Document %ld cannot be read - Ignored\n",
			text_id->id);
	return(UNDEF);
    }
    
    tmp_buf.size = text.textloc_doc.end_text - text.textloc_doc.begin_text;
    tmp_buf.end = tmp_buf.size;
    tmp_buf.buf = text.doc_text;
    if (output == NULL) {
	if (0 == fwrite(&tmp_buf.buf, tmp_buf.size, 1, stdout)) {
	    (void) fprintf(stderr, "Document %ld cannot be written - Ignored\n",
			text_id->id);
	    return(UNDEF);
	}
    } else {
	if (UNDEF == add_buf(&tmp_buf, output)) {
	    (void) fprintf(stderr, "Document %ld cannot be written - Ignored\n",
			text_id->id);
	    return(UNDEF);
	}
    }

    PRINT_TRACE (2, print_string, "Trace: leaving print_text_filter");
    return (1);
}


int
close_print_text_filter (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_print_text_filter");

    if (UNDEF == get_text.ptab->close_proc(get_text.inst))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_print_text_filter");
    return (0);
}

