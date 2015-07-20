#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/pi_textdoc.c,v 11.2 1993/02/03 16:54:04 smart Exp $";
#endif
/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "docindex.h"
#include "buf.h"

static SM_BUF internal_output = {0, 0, (char *) 0};

/* Print a SM_INDEX_TEXTDOC relation in text form. */
void
print_int_textdoc (textdoc, output)
SM_INDEX_TEXTDOC *textdoc;
SM_BUF *output;
{
    long i;
    long section_length;
    char temp_buf[PATH_LEN+40];
    SM_BUF *out_p;
    SM_BUF sm_buf;

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    if (textdoc->mem_doc.num_sections <= 0)
        return;

    (void) sprintf (&temp_buf[0], ".- --- %ld %s %ld %ld\n",
            textdoc->textloc_doc.id_num,
            textdoc->textloc_doc.file_name,
            textdoc->textloc_doc.begin_text,
            textdoc->textloc_doc.end_text);
    (void) add_buf_string (temp_buf, out_p);

    for (i = 0; i < textdoc->mem_doc.num_sections; i++) {
        section_length = textdoc->mem_doc.sections[i].end_section -
                         textdoc->mem_doc.sections[i].begin_section;
        if (section_length > 0) {
            (void) sprintf (temp_buf, "\n.%c %ld %ld\n",
                    textdoc->mem_doc.sections[i].section_id,
                    textdoc->mem_doc.sections[i].begin_section,
                    textdoc->mem_doc.sections[i].end_section);
            (void) add_buf_string (temp_buf, out_p);
            sm_buf.buf = textdoc->doc_text +
                          textdoc->mem_doc.sections[i].begin_section;
            sm_buf.end = section_length;
            (void) add_buf (&sm_buf, out_p);
        }
    }

    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }

}


/* As above, except that no header information is printed. */
void
print_int_textdoc_nohead (textdoc, output)
SM_INDEX_TEXTDOC *textdoc;
SM_BUF *output;
{
    long i;
    long section_length;
    SM_BUF *out_p;
    SM_BUF sm_buf;

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    if (textdoc->mem_doc.num_sections <= 0)
        return;

    for (i = 0; i < textdoc->mem_doc.num_sections; i++) {
        section_length = textdoc->mem_doc.sections[i].end_section -
                         textdoc->mem_doc.sections[i].begin_section;
        if (section_length > 0) {
            sm_buf.buf = textdoc->doc_text +
                          textdoc->mem_doc.sections[i].begin_section;
            sm_buf.end = section_length;
            (void) add_buf (&sm_buf, out_p);
        }
    }

    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }

}
