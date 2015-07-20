#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_rel_hd.c,v 11.2 1993/02/03 16:54:13 smart Exp $";
#endif
/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "buf.h"
#include "rel_header.h"

static SM_BUF internal_output = {0, 0, (char *) 0};

/* Print the give relation header to stdout or memory*/
void
print_rel_header (rh, output)
REL_HEADER *rh;
SM_BUF *output;
{
    SM_BUF *out_p;
    char temp_buf[PATH_LEN];

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    (void) sprintf (temp_buf, 
                    "num_entries\t\t%ld\nmax_primary_value\t%ld\nmax_secondary_value\t%ld\ntype\t\t\t%d\nnum_var_files\t\t%d\n_entry_size\t\t%d\n_internal\t\t%ld\n",
                    rh->num_entries,
                    rh->max_primary_value,
                    rh->max_secondary_value,
                    (int) rh->type,
                    (int) rh->num_var_files,
                    (int) rh->_entry_size,
                    rh->_internal);

    if (UNDEF == add_buf_string (temp_buf, output))
        return;

    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}

