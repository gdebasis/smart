#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/tr_inex_o.c,v 11.2 1993/02/03 16:51:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take an entire smart tr results file, and convert to text INEX results file
 *1 local.convert.obj.tr_inex
 *2 tr_inex_obj (tr_file, text_file, inst)
 *3 char *tr_file;
 *3 char *text_file;
 *3 int inst;
 *4 init_tr_inex_obj (spec, unused)
 *5   "tr_inex_obj.run_id"
 *5   "tr_inex_obj.tr_file"
 *5   "tr_inex_obj.tr_file.rmode"
 *5   "tr_inex_obj.textloc_file"
 *5   "tr_inex_obj.textloc_file.rmode"
 *5   "tr_inex_obj.trace"
 *4 close_tr_inex_obj (inst)

 *7 Read TR_VEC tuples from tr_file and convert to TREC format.  This involves
 *7 changing the internal SMART did to the official TREC DOCNO.
 *7 Input is conceptually tuples of the form
 *7     qid  did  rank  action  rel  iter  sim
 
 *8 Changes added for INEX07 submission.-Sukomal
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "io.h"
#include "functions.h"
#include "spec.h"
#include "trace.h"
#include "smart_error.h"
#include "tr_vec.h"
#include "textloc.h"

#define HEADER_STRING \
"<inex-submission participant-id=\"19\" run-id=\"%s\" task=\"Focused\" query=\"automatic\" result-type=\"element\">\n" \
"<topic-fields title=\"yes\" mmtitle=\"no\" castitle=\"no\" description=\"yes\" narrative=\"no\" />\n" \
"<description>Content-only approach using straightforward term-weighted vectors and inner-product similarity with query expansion.\n</description>\n" \
"<collections>\n  <collection>wikipedia</collection>\n</collections>\n"

static char *run_id;
static char *default_tr_file;
static long tr_mode;
static char *textloc_file;
static long textloc_mode;
static long format;

static SPEC_PARAM spec_args[] = {
    {"tr_inex_obj.run_id",       getspec_string,   (char *) &run_id},
    {"tr_inex_obj.tr_file",      getspec_dbfile,   (char *) &default_tr_file},
    {"tr_inex_obj.tr_file.rmode", getspec_filemode,(char *) &tr_mode},
    {"tr_inex_obj.textloc_file" ,    getspec_dbfile,   (char *) &textloc_file},
    {"tr_inex_obj.textloc_file.rmode", getspec_filemode,(char *) &textloc_mode},
    {"tr_inex_obj.submission_format", getspec_long, (char *) &format},
    TRACE_PARAM ("tr_inex_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int comp_rank(const void *, const void *);


int
init_tr_inex_obj (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string,
                 "Trace: entering/leaving init_tr_inex_obj");

    return (0);
}

int
tr_inex_obj (tr_file, text_file, inst)
char *tr_file;
char *text_file;
int inst;
{
    char *fname, *extension;
    int tr_fd, tloc_fd;
    long i;
    FILE *out_fd;
    TEXTLOC tloc;
    TR_VEC tr_vec;

    PRINT_TRACE (2, print_string, "Trace: entering tr_inex_obj");

    /* Open text output file */
    if (VALID_FILE (text_file)) {
        if (NULL == (out_fd = fopen (text_file, "w")))
            return (UNDEF);
    }
    else
        out_fd = stdout;

    /* Open input doc and dict files */
    if (tr_file == (char *) NULL)
        tr_file = default_tr_file;
    if (UNDEF == (tr_fd = open_tr_vec (tr_file, tr_mode)) ||
        UNDEF == (tloc_fd = open_textloc (textloc_file, textloc_mode)))
        return (UNDEF);

    if (format < 2009) fprintf(out_fd, HEADER_STRING, run_id);
    while (1 == read_tr_vec (tr_fd, &tr_vec)) {
        PRINT_TRACE (7, print_tr_vec, &tr_vec);
        if (format < 2009)
            fprintf(out_fd, "<topic topic-id=\"%ld\">\n", tr_vec.qid);
        qsort(tr_vec.tr, tr_vec.num_tr, sizeof(TR_TUP), comp_rank);
        for (i = 0; i < tr_vec.num_tr; i++) {
            /* Lookup did in textloc file to get filename */
            tloc.id_num = tr_vec.tr[i].did;
            PRINT_TRACE (7, print_long, &tloc.id_num);
            if (1 != seek_textloc (tloc_fd, &tloc) ||
                1 != read_textloc (tloc_fd, &tloc)) 
                return UNDEF;
            /* get the file name and then strip the .xml extension */
            if (NULL == (fname = rindex(tloc.file_name, '/')))
                fname = tloc.file_name;
            else fname++;
            if (NULL != (extension = rindex(tloc.file_name, '.')))
                *extension = '\0';
            if (format < 2009) {
                (void) fprintf (out_fd, 
                                "  <result>\n    <file>%s</file>\n    <path>/article[1]</path>\n    <rsv>%.4f</rsv>\n  </result>\n",
                                fname, tr_vec.tr[i].sim);
            }
            else {
                fprintf (out_fd, "%ld Q0 %s %d %f %s /article[1]\n",
                         tr_vec.qid, 
                         fname,
                         i+1,
                         tr_vec.tr[i].sim,
                         run_id);
            }
        }
        if (format < 2009) (void) fprintf (out_fd, "</topic>\n");
    }
    if (format < 2009) fprintf(out_fd, "</inex-submission>\n");

    if (UNDEF == close_tr_vec (tr_fd) ||
        UNDEF == close_textloc (tloc_fd))
        return (UNDEF);

    if (VALID_FILE (text_file))
        (void) fclose (out_fd);

    PRINT_TRACE (2, print_string, "Trace: leaving tr_inex_obj");
    return (1);
}

int
close_tr_inex_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string,
                 "Trace: entering/leaving close_tr_inex_obj");
    return (0);
}

static int
comp_rank(const void *t1, const void *t2)
{
    TR_TUP *tt1, *tt2;
    tt1 = (TR_TUP *) t1;    
    tt2 = (TR_TUP *) t2;
    return tt1->rank - tt2->rank;
}
