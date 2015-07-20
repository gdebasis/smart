#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_textloc.c,v 11.2 1993/02/03 16:53:56 smart Exp $";
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
#include "textloc.h"
#include "docindex.h"
#include "buf.h"

static SM_BUF internal_output = {0, 0, (char *) 0};

void
print_textloc (textloc, output)
TEXTLOC *textloc;
SM_BUF *output;
{

    char buf[PATH_LEN];
    SM_BUF *out_p;

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    (void) sprintf (buf, "Doc textloc %ld\n", textloc->id_num);
    (void) add_buf_string (buf, out_p);
    (void) sprintf (buf,
                    "  Title '%s'\n", textloc->title?textloc->title : "NONE");
    (void) add_buf_string (buf, out_p);
    (void) sprintf (buf,
            "  File '%s'\n", textloc->file_name ? textloc->file_name : "NONE");
    (void) add_buf_string (buf, out_p);
    (void) sprintf (buf,
                    "    %8ld\t%8ld\t\t%8ld\n",
                    textloc->begin_text,
                    textloc->end_text,
                    textloc->doc_type);
    (void) add_buf_string (buf, out_p);

    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}

