#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_spec.c,v 11.2 1993/02/03 16:53:54 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "spec.h"
#include "buf.h"

static SM_BUF internal_output = {0, 0, (char *) 0};

void
print_spec (spec_ptr, output)
SPEC *spec_ptr;
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

    if (spec_ptr == NULL)
        return;
    (void) sprintf (temp_buf, "#Spec_id %d\n", spec_ptr->spec_id);
    if (UNDEF == add_buf_string (temp_buf, output))
        return;
    (void) sprintf (temp_buf, "#Num_spec_items %d\n", spec_ptr->num_list);
    if (UNDEF == add_buf_string (temp_buf, output))
        return;
    for (i = 0; i < spec_ptr->num_list; i++) {
        (void) sprintf (temp_buf,
                        "%s \"%s\"\n",
                        spec_ptr->spec_list[i].name,
                        spec_ptr->spec_list[i].value);
        if (UNDEF == add_buf_string (temp_buf, output))
            return;
    }
    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}
