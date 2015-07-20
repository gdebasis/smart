#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/ph_text_for.c,v 11.2 1993/02/03 16:53:59 smart Exp $";
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
#include "docdesc.h"
#include "trace.h"
#include "io.h"
#include "buf.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("print.indiv.trace")
};
static int num_spec_args = sizeof(spec_args) / sizeof(spec_args[0]);

static char *qord_prefix;
static PROC_INST preparse;
static char *format;
static SPEC_PARAM_PREFIX qord_spec_args[] = {
    {&qord_prefix, "named_preparse", getspec_func, (char *)&preparse.ptab},
    {&qord_prefix, "format", getspec_string, (char *)&format},
    };
static int num_qord_spec_args = sizeof(qord_spec_args) /
				sizeof(qord_spec_args[0]);

static void print_textdoc_form();

int
init_print_text_form (spec, qd_prefix)
SPEC *spec;
char *qd_prefix;
{
    char prefix[PATH_LEN];

    if (UNDEF == lookup_spec(spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_print_text_form");

    (void) sprintf(prefix, "print.%s", qd_prefix);
    qord_prefix = prefix;
    if (UNDEF == lookup_spec_prefix(spec, &qord_spec_args[0],
				    num_qord_spec_args))
	return(UNDEF);
    
    /* Call all initialization procedures */
    if (UNDEF == (preparse.inst = preparse.ptab->init_proc(spec, qd_prefix)))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_print_text_form");
    return (0);
}

int
print_text_form (text_id, output, inst)
EID *text_id;
SM_BUF *output;
int inst;
{
    SM_INDEX_TEXTDOC pp_doc;

    PRINT_TRACE (2, print_string, "Trace: entering print_text_form");

    if (1 != preparse.ptab->proc (text_id, &pp_doc, preparse.inst))
        return (UNDEF);
    print_textdoc_form (&pp_doc, output);

    PRINT_TRACE (2, print_string, "Trace: leaving print_text_form");
    return (1);
}


int
close_print_text_form (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_print_text_form");

    if (UNDEF == preparse.ptab->close_proc (preparse.inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_print_text_form");
    return (0);
}

static SM_BUF internal_output = {0, 0, (char *) 0};

static void
print_textdoc_form (doc, output)
SM_INDEX_TEXTDOC *doc;
SM_BUF *output;
{
    SM_BUF *out_p;
    char *ptr;
    long i;
    long section_length;
    int nl_to_sp;             /* Flag to indicate want to compress \n to ' ' */
    SM_BUF sm_buf;
    char buf;
    int save_start;
    char *temp_ptr;

    if (! VALID_FILE (format)) {
        print_int_textdoc (doc, output);
        return;
    }

    if (doc->mem_doc.num_sections == 0)
        return;

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    for (ptr = format; *ptr; ptr++) {
        switch (*ptr) {
          case '\\':
            sm_buf.buf = &buf;
            sm_buf.end = 1;
            ptr++;
            switch (*ptr) {
              case 'n':
                buf = '\n';
                break;
              case 't':
                buf = '\t';
                break;
              case 'b':
                buf = '\b';
                break;
              case 'r':
                buf = '\r';
                break;
              case 'f':
                buf = '\f';
                break;
              default:    /* BUG?? Does not handle octal numbers */
                buf = *ptr;
                break;
            }
            if (UNDEF == add_buf (&sm_buf, output))
                return;
            break;
          case '%':
            ptr++;
            if (*ptr == '-') {
                nl_to_sp = 1;
                ptr++;
            }
            else {
                nl_to_sp = 0;
            }
            for (i = 0; i < doc->mem_doc.num_sections; i++) {
                if (*ptr == doc->mem_doc.sections[i].section_id) {
                    section_length = doc->mem_doc.sections[i].end_section -
                                     doc->mem_doc.sections[i].begin_section;

                    sm_buf.buf = doc->doc_text +
                                  doc->mem_doc.sections[i].begin_section;
                    sm_buf.end = section_length;
                    save_start = output->end;
                    if (UNDEF == add_buf (&sm_buf, output))
                        return;
                    if (nl_to_sp) {
                        /* Change all newlines to spaces */
                        for (temp_ptr = &output->buf[save_start];
                             temp_ptr < &output->buf[output->end];
                             temp_ptr++) {
                            if (*temp_ptr == '\n')
                                *temp_ptr = ' ';
                        }
                    }
                }
            }
            break;
          default:
            sm_buf.buf = &buf;
            sm_buf.end = 1;
            buf = *ptr;
            if (UNDEF == add_buf (&sm_buf, output))
                return;
            break;
        }
    }
    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        (void) fflush (stdout);
        out_p->end = 0;
    }
}
