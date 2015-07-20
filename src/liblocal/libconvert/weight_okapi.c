#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/weight_okapi.c,v 11.2 1993/02/03 16:49:20 smart Exp $";
#endif

/* Copyright (c) 1994, 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by Okapi style weighting
 *1 convert.wt_tf.o
 *2 tfwt_okapi (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;
 *4 init_tfwt_okapi (spec, NULL)
 *5   "weight.k1"
 *5   "weight.b"
 *5   "weight.avdl"
 *4 close_tfwt_okapi (inst)

 *7 For each term in vector, convert its weight based on Okapi's
 *7 weighting function:
 *7                          tf
 *7                ------------------------
 *7                k1*{(1-b)+b*dl/avdl} + tf
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"
#include "textloc.h"

static long k1;
static float b;
static long avdl;
static char *textloc_file;
static long read_mode;
static SPEC_PARAM spec_args[] = {
    {"weight.k1", getspec_long, (char *) &k1},
    {"weight.b", getspec_float, (char *) &b},
    {"weight.avdl", getspec_long, (char *) &avdl},
    {"weight.textloc_file", getspec_dbfile, (char *) &textloc_file},
    {"weight.textloc_file.rmode", getspec_filemode, (char *) &read_mode},
    TRACE_PARAM ("weight.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* For time being, assume no data private to instantiation */
static int num_instantiated = 0;

static int textloc_fd;

int
init_tfwt_okapi(spec, unused)
SPEC *spec;
char *unused;
{
    if (num_instantiated) {
        return (num_instantiated++);
    }

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_tfwt_okapi");

    if (! VALID_FILE (textloc_file) ||
        UNDEF == (textloc_fd = open_textloc (textloc_file, read_mode))) {
        set_error (SM_ILLPA_ERR, textloc_file, "tfwt_okapi");
        return (UNDEF);
    }

   PRINT_TRACE (2, print_string, "Trace: leaving init_tfwt_okapi");

   return (num_instantiated++);
}

int
tfwt_okapi (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;
    CON_WT *ptr = conwt;
    float doc_length;
    TEXTLOC textloc;

    if (vec->num_conwt == 0)
        return(0);

    textloc.id_num = vec->id_num.id;
    if (1 != seek_textloc (textloc_fd, &textloc) ||
        1 != read_textloc (textloc_fd, &textloc))
        return (UNDEF);

    doc_length = textloc.end_text - textloc.begin_text;

    /* ptr->wt is tf */
    for (i = 0; i < vec->num_conwt; i++) {
        ptr->wt /= k1*(1-b + b*doc_length/avdl) + ptr->wt;
        ptr++;
    }
    return (1);
}

int
close_tfwt_okapi(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_tfwt_okapi");
    num_instantiated --;
    if (num_instantiated == 0) {
        if (UNDEF == close_textloc (textloc_fd))
            return (UNDEF);
    }
   PRINT_TRACE (2, print_string, "Trace: leaving close_tfwt_okapi");

   return (num_instantiated);
}
