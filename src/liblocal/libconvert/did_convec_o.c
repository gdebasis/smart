#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/did_convec_o.c,v 11.0 1992/07/21 18:20:09 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "retrieve.h"
#include "vector.h"
#include "docindex.h"
#include "textloc.h"
#include "inst.h"

static char *vec_file;
static long vec_file_mode;
static char *textloc_file;
static long textloc_file_mode;
static char *did_file;
static SPEC_PARAM spec_args[] = {
    {"convert.did_convec.vec_file", getspec_dbfile, (char *) &vec_file},
    {"convert.did_convec.doc_file.rwcmode", getspec_filemode, (char *) &vec_file_mode},
    {"convert.did_convec.textloc_file", getspec_dbfile, (char *) &textloc_file},
    {"convert.did_convec.textloc_file.rmode",getspec_filemode, (char *) &textloc_file_mode},
    {"convert.did_convec.dis_file",  getspec_dbfile, (char *) &did_file},
    TRACE_PARAM ("convert.doc_convec.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int vec_fd;
    int textloc_fd;
    int did_convec_inst;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int init_did_convec(), did_convec(), close_did_convec();

int
init_did_convec_obj (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_did_convec_o");
    
    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];
    
    if (UNDEF == (ip->vec_fd = open_vector (vec_file, vec_file_mode)))
        return (UNDEF);
    if (UNDEF == (ip->textloc_fd = open_textloc (textloc_file,
						 textloc_file_mode)))
        return (UNDEF);
    if (UNDEF == (ip->did_convec_inst = init_did_convec(spec, (char *)NULL)))
        return UNDEF;

    ip->valid_info = 1;
             
    PRINT_TRACE (2, print_string, "Trace: leaving init_did_convec_o");
    
    return (new_inst);
}


int
did_convec_obj (unused1, unused2, inst)
char *unused1;
char *unused2;
int inst;
{
    STATIC_INFO *ip;
    TEXTLOC textloc;
    VEC vec;
    long *dids;
    long i, num_dids = 0;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "did_convec_o");
        return (UNDEF);
    }
    ip  = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering did_convec_o");

    if (UNDEF == text_long_array(did_file, &dids, &num_dids))
        return (UNDEF);

    for (i = 0; i < num_dids; i++) {
      textloc.id_num = dids[i];
      if (1 != seek_textloc (ip->textloc_fd, &textloc) ||
	  1 != read_textloc (ip->textloc_fd, &textloc))
          return (UNDEF);

      if (UNDEF == did_convec(&textloc, &vec, ip->did_convec_inst))
          return (UNDEF);

      if (UNDEF == seek_vector (ip->vec_fd, &vec) ||
	  UNDEF == write_vector (ip->vec_fd, &vec))
          return (UNDEF);
    }

    if (UNDEF == free_long_array (did_file, &dids, &num_dids))
        return (UNDEF);

    PRINT_TRACE (4, print_vector, vec);
    PRINT_TRACE (2, print_string, "Trace: leaving did_convec_o");
    return (1);
}


int
close_did_convec_obj(inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_did_convec_o");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_did_convec_o");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info = 0;

    if (UNDEF == close_vector (ip->vec_fd))
        return (UNDEF);

    if (UNDEF == close_textloc (ip->textloc_fd))
        return (UNDEF);

    if (UNDEF == close_did_convec(ip->did_convec_inst))
        return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving close_did_convec_o");
    return (0);
}
