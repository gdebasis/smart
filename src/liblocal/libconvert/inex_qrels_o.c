#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/inex_qrels_o.c,v 11.2 1993/02/03 16:51:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Convert an INEX qrels text file to SMART RR format
 *1 local.convert.obj.inex_qrels
 *2 inex_qrels_obj (text_file, qrels_file, inst)
 *3 char *text_file;
 *3 char *qrels_file;
 *3 int inst;
 *4 init_inex_qrels_obj (spec, unused)
 *4 close_inex_qrels_obj (inst)

 *7 Read text tuples from INEX qrels file (generated from fullRecallBase.xml)
 *7 of the form
 *7 qid iter docno rel
 *7 (see trec_qrels_o.c for more information).

 *8 Since the docnos are actually filenames, we use the textloc file (instead 
 *8 of the usual DOCNO based approach used for TREC results/qrels). 
 *8 See also tr_inex_o.c

***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "io.h"
#include "functions.h"
#include "spec.h"
#include "trace.h"
#include "smart_error.h"
#include "rr_vec.h"
#include "textloc.h"
#include "rel_header.h"

static char *textloc_file;
static long textloc_mode;
static char *qrels_file;
static long qrels_mode;

typedef struct {
    char *fname;
    long docid;
} FILENAME_TABLE;

static SPEC_PARAM spec_args[] = {
    {"inex_qrels_obj.textloc_file", getspec_dbfile, (char *) &textloc_file},
    {"inex_qrels_obj.textloc_file.rmode", getspec_filemode, (char *) &textloc_mode},
    {"inex_qrels_obj.qrels_file", getspec_dbfile, (char *) &qrels_file},
    {"inex_qrels_obj.qrels_file.rwcmode", getspec_filemode, (char *) &qrels_mode},
    TRACE_PARAM ("inex_qrels_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int comp_fname(const void *, const void *);
static int comp_did(const void *, const void *);


int
init_inex_qrels_obj (spec, unused)
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
                 "Trace: entering/leaving init_inex_qrels_obj");

    return (0);
}

int
inex_qrels_obj (text_file, out_file, inst)
char *text_file;
char *out_file;
int inst;
{
    char *fname, *extension, inbuf[PATH_LEN], docno[PATH_LEN/2];
    int iter, rel;
    int rr_fd, tloc_fd, rr_size;
    long num_docs, qid, i;
    FILE *in_fd;
    REL_HEADER *rh;
    TEXTLOC tloc;
    RR_VEC rr_vec;
    FILENAME_TABLE *table, *tptr, entry;

    PRINT_TRACE (2, print_string, "Trace: entering inex_qrels_obj");

    /* Open text input file */
    if (VALID_FILE (text_file)) {
        if (NULL == (in_fd = fopen (text_file, "r")))
            return (UNDEF);
    }
    else
        in_fd = stdin;
    if (VALID_FILE (out_file))
        qrels_file = out_file;

    /* Open textloc and rr (qrels) file */
    if (UNDEF == (rr_fd = open_rr_vec (qrels_file, qrels_mode)) ||
        UNDEF == (tloc_fd = open_textloc (textloc_file, textloc_mode)))
        return (UNDEF);

    /* Create file name to docid map */
    if (NULL == (rh = get_rel_header (textloc_file)))
        return (UNDEF);
    if (NULL == (table = Malloc(rh->max_primary_value, FILENAME_TABLE))) {
        set_error(SM_INCON_ERR, "Out of memory", "inex_qrels_obj");
        return UNDEF;
    }
    num_docs = 0;
    while (1 == read_textloc (tloc_fd, &tloc)) {
        table[num_docs].docid = tloc.id_num;
        /* get the file name and then strip the .xml extension */
        if (NULL == (fname = rindex(tloc.file_name, '/')))
            fname = tloc.file_name;
        else fname++;
        if (NULL != (extension = rindex(tloc.file_name, '.')))
            *extension = '\0';
        if (NULL == (table[num_docs].fname = strdup(fname)))
            return UNDEF;
        num_docs++;
    }
    qsort(table, num_docs, sizeof(FILENAME_TABLE), comp_fname);

    /* Allocate initial space for qrels vector */
    rr_vec.qid = 0;
    rr_vec.num_rr = 0;
    rr_size = 200;
    if (NULL == (rr_vec.rr = (RR_TUP *) malloc (rr_size * sizeof (RR_TUP))))
        return (UNDEF);

    while (NULL != fgets(inbuf, PATH_LEN, in_fd)) {
        if (4 != sscanf(inbuf, "%ld %d %s %d", &qid, &iter, docno, &rel)) {
            fprintf(stderr, "Malformed line: %s", inbuf);
            continue;
        }

        if (qid != rr_vec.qid) {
            /* Output previous vector after sorting it */
            if (rr_vec.qid != 0) {
                (void) qsort ((char *) rr_vec.rr,
                              (int) rr_vec.num_rr,
                              sizeof (RR_TUP),
                              comp_did);
                if (UNDEF == seek_rr_vec (rr_fd, &rr_vec) ||
                    UNDEF == write_rr_vec (rr_fd, &rr_vec))
                    return (UNDEF);
            }
            rr_vec.qid = qid;
            rr_vec.num_rr = 0;
        }
        if (rel) {
            entry.fname = docno;
            if (NULL == (tptr = bsearch(&entry, table, num_docs, 
                                        sizeof(FILENAME_TABLE),
                                        comp_fname))) {
                fprintf(stderr, "Couldn't find document named %s\n",
                        docno);
                continue;
            }
            rr_vec.rr[rr_vec.num_rr].did = tptr->docid;
            rr_vec.rr[rr_vec.num_rr].rank = 0;
            rr_vec.rr[rr_vec.num_rr].sim = 0.0;
            rr_vec.num_rr++;

            /* Allocate more space for this rr vector if needed */
            if (rr_vec.num_rr >= rr_size) {
                rr_size += rr_size;
                if (NULL == (rr_vec.rr = (RR_TUP *)
                             realloc ((char *) rr_vec.rr,
                                      rr_size * sizeof (RR_TUP))))
                    return (UNDEF);
            }
        }
    }

    /* Output last vector */
    if (rr_vec.qid != 0 && rr_vec.num_rr > 0) {
        (void) qsort ((char *) rr_vec.rr,
                      (int) rr_vec.num_rr,
                      sizeof (RR_TUP),
                      comp_did);
        if (UNDEF == seek_rr_vec (rr_fd, &rr_vec) ||
            UNDEF == write_rr_vec (rr_fd, &rr_vec))
            return (UNDEF);
    }

    for (i = 0; i < num_docs; i++)
        free(table[i].fname);
    free(table);

    if (UNDEF == close_rr_vec (rr_fd) ||
        UNDEF == close_textloc (tloc_fd))
        return (UNDEF);

    if (VALID_FILE (text_file))
        (void) fclose (in_fd);

    PRINT_TRACE (2, print_string, "Trace: leaving inex_qrels_obj");
    return (1);
}

int
close_inex_qrels_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string,
                 "Trace: entering/leaving close_inex_qrels_obj");
    return (0);
}

static int
comp_fname(const void *t1, const void *t2)
{
    FILENAME_TABLE *tt1, *tt2;
    tt1 = (FILENAME_TABLE *) t1;    
    tt2 = (FILENAME_TABLE *) t2;
    return strcmp(tt1->fname, tt2->fname);
}

static int 
comp_did (const void *r1, const void *r2)
{
    RR_TUP *rr1 = (RR_TUP *) r1;
    RR_TUP *rr2 = (RR_TUP *) r2;

    return (rr1->did - rr2->did);
}
