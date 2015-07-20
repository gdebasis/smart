#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/weights_norm.c,v 11.2 1993/02/03 16:49:20 smart Exp $";
#endif

/* Copyright (c) 1994, 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by byte length normalization
 *1 convert.weight_lnorm
 *2 normwt_bytelength (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;
 *4 init_normwt_bytelength (spec, NULL)
 *5   "weight.power"
 *4 close_normwt_bytelength(inst)
 

 *7 For each term in vector, divide its existing weight by
 *7 the length of the vector.  Length is defined in terms of num_bytes:
 *7    num_bytes^power
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

static double power;
static double pivot1;
static double pivot2;
static double slope1;
static double slope2;
static char *textloc_file;
static long read_mode;

static SPEC_PARAM spec_args[] = {
    {"weight.power", getspec_double, (char *) &power},
    {"weight.pivot1", getspec_double, (char *) &pivot1},
    {"weight.pivot2", getspec_double, (char *) &pivot2},
    {"weight.slope1", getspec_double, (char *) &slope1},
    {"weight.slope2", getspec_double, (char *) &slope2},
    {"weight.textloc_file", getspec_dbfile, (char *) &textloc_file},
    {"weight.textloc_file.rmode", getspec_filemode, (char *) &read_mode},
    TRACE_PARAM ("weight.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* For time being, assume no data private to instantiation */
static int num_instantiated = 0;

static int textloc_fd;

int
init_normwt_bytelength(spec, unused)
SPEC *spec;
char *unused;
{
    if (num_instantiated) {
        return (num_instantiated++);
    }
        
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_normwt_bytelength");

    if (! VALID_FILE (textloc_file) ||
        UNDEF == (textloc_fd = open_textloc (textloc_file, read_mode))) {
        set_error (SM_ILLPA_ERR, textloc_file, "normwt_bytelength");
        return (UNDEF);
    }

   PRINT_TRACE (2, print_string, "Trace: leaving init_normwt_bytelength");

   return (num_instantiated++);
}

int
normwt_bytelength (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;
    CON_WT *ptr = conwt;
    double pivot1_factor, pivot2_factor, norm_factor;
    double doc_length;
    TEXTLOC textloc;

    if (vec->num_conwt == 0)
        return(0);

    textloc.id_num = vec->id_num.id;
    if (1 != seek_textloc (textloc_fd, &textloc) ||
        1 != read_textloc (textloc_fd, &textloc))
        return (UNDEF);

    pivot1_factor = pow(pivot1, power);
    pivot2_factor = pow(pivot2, power);
    doc_length = textloc.end_text - textloc.begin_text;
    norm_factor = pow(doc_length, power);

    if (doc_length < pivot2)
	norm_factor = slope1*norm_factor + (1.0 - slope1)*pivot1_factor;
    else
	norm_factor = slope2*norm_factor -
	    (slope2 - slope1)*pivot2_factor +
	    (1.0 - slope1)*pivot1_factor;

    for (i = 0; i < vec->num_conwt; i++) {
        ptr->wt /= norm_factor;
        ptr++;
    }
    return (1);
}


int
close_normwt_bytelength(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_normwt_bytelength");
    num_instantiated --;
    if (num_instantiated == 0) {
        if (UNDEF == close_textloc (textloc_fd))
            return (UNDEF);
    }
   PRINT_TRACE (2, print_string, "Trace: leaving close_normwt_bytelength");

   return (num_instantiated);
}
