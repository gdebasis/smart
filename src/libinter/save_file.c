#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/save_file.c,v 11.2 1993/02/03 16:51:25 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "io.h"
#include "smart_error.h"
#include "spec.h"
#include "inter.h"
#include "inst.h"

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    char *save_file;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int
init_inter_save_file (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    ip->save_file = (char *) NULL;

    ip->valid_info++;

    return (new_inst);
}

int
close_inter_save_file (inst)
int inst;
{
    STATIC_INFO *ip;
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_inter_save_file");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (NULL != ip->save_file)
        (void) free ((char *) ip->save_file);

    ip->valid_info--;
    return (1);
}


int
inter_save_file (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    int outfd;
    char temp_buf[PATH_LEN + 25];
    STATIC_INFO *ip;
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter_save_file");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (ip->save_file == NULL) {
        if (NULL == (ip->save_file = malloc (PATH_LEN)))
            return (UNDEF);
        ip->save_file[0] = '\0';
    }

    if (is->num_command_line >= 2) {
        (void) strncpy (ip->save_file, is->command_line[1], PATH_LEN-1);
        ip->save_file[PATH_LEN-1] = '\0';
    }

    if (ip->save_file[0] == '\0') {
        if (UNDEF == add_buf_string ("No save file specified\n", &is->err_buf))
            return (UNDEF);
        return (0);
    }

    /* Open new output file for appending and copy last output to it */
    if (-1 == (outfd = open (ip->save_file,
                             SWRONLY | SCREATE | SAPPEND, 0644)) ||
        is->output_buf.end != write (outfd,
                                     is->output_buf.buf,
                                     is->output_buf.end) ||
        -1 == close (outfd)) {
        (void) sprintf (temp_buf,
                        "Cannot write file %s\n",
                        ip->save_file);
        if (UNDEF == add_buf_string (temp_buf, &is->err_buf))
            return (UNDEF);
        return (0);
    }
    
    (void) sprintf (temp_buf, "%d bytes written to file %s\n",
                    is->output_buf.end, ip->save_file);
    if (UNDEF == add_buf_string (temp_buf, &is->err_buf))
        return (UNDEF);

    return (1);
}
