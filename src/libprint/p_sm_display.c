#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_sm_display.c,v 11.2 1993/02/03 16:53:46 smart Exp $";
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

void
print_sm_display (display, output)
SM_DISPLAY *display;
SM_BUF *output;
{
    long i;
    SM_BUF *out_p;
    char temp_buf[PATH_LEN];

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    (void) sprintf (temp_buf, "Doc sm_display %ld\n", display->id_num.id);
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    (void) sprintf (temp_buf, "  Title '%s'\n",
                    display->title ? display->title : "NONE");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    (void) sprintf (temp_buf, "  File '%s'\n",
                   display->file_name ? display->file_name : "NONE");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    for (i = 0; i < display->num_sections; i++) {
        (void) sprintf (temp_buf, "    %c\t%8ld\t%8ld\n",
                display->sections[i].section_id,
                display->sections[i].begin_section,
                display->sections[i].end_section);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}
