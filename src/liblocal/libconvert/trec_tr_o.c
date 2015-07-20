#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/trec_tr_o.c,v 11.2 1993/02/03 16:51:40 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take an entire TREC results text file, and convert to SMART tr file
 *1 local.convert.obj.trec_tr
 *2 trec_tr_obj (text_file, tr_file, inst)
 *3 char *text_file;
 *3 char *tr_file;
 *3 int inst;

 *4 init_trec_tr_obj (spec, unused)
 *5   "trec_tr_obj.tr_file"
 *5   "trec_tr_obj.tr_file.rwcmode"
 *5   "trec_tr_obj.trec_qrels_file"
 *5   "trec_tr_obj.trace"
 *4 close_trec_tr_obj (inst)

 *7 Read text tuples from text_file of the form
 *7     030  Q0  ZF08-175-870  0   4238   prise1
 *&     qid iter   docno      rank  sim   run_id
 *7 giving TREC document numbers (a string) retrieved by query qid 
 *7 (an integer) with similarity sim (a float).  The other fields are ignored.
 *7 Input is asssumed to be sorted by qid.
 *7 Relevance for each docno to qid is determined from text_qrels_file, which
 *7 consists of text tuples of the form
 *7    qid  iter  docno  rel
 *7 giving TREC document numbers (a string) and their relevance to query qid
 *7 (an integer). Tuples are asssumed to be sorted by qid.
 *7 The text tuples with relevence judgements are converted to TR_VEC form
 *7 and written to the TR_VEC relational object tr_file.
 *7 If text_file is NULL or non-valid (eg "-"), or tr_file is NULL then error.
 *7 Return UNDEF if error, else 1.  It is an error if tr_file exists.

 *8 Procedure is to read all the docs retrieved for a query, and all the
 *8 relevant docs for that query,
 *8 sort and rank the retrieved docs by sim/docno, 
 *8 and look up docno in the relevant docs to determine relevance.
 *8 The qid,did,rank,sim,rel fields of of TR_VEC are filled in; 
 *8 action,iter fields are set to 0.
 *8 Queries for which there are no relevant docs are ignored (the retrieved
 *8 docs are NOT written out).

***********************************************************************/

#include "common.h"
#include "param.h"
#include "io.h"
#include "functions.h"
#include "spec.h"
#include "trace.h"
#include "smart_error.h"
#include "rr_vec.h"
#include "tr_vec.h"
#include "dict.h"
#include "textline.h"

#define TR_INCR 250

static char *default_tr_file;
static long tr_mode;
static char *trec_qrels_file;

static SPEC_PARAM spec_args[] = {
    {"trec_tr_obj.tr_file",          getspec_dbfile,  (char *) &default_tr_file},
    {"trec_tr_obj.tr_file.rwcmode",  getspec_filemode,(char *) &tr_mode},
    {"trec_tr_obj.trec_qrels_file",  getspec_dbfile,  (char *) &trec_qrels_file},
    TRACE_PARAM ("trec_tr_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int init_get_trec_tr(), get_trec_tr();
static int init_get_trec_qrels(), get_trec_qrels();
static int comp_tr_tup_did(), comp_tr_docno(), comp_qrels_docno(),
    comp_sim_docno();
static int form_and_write_trvec();

typedef struct {
    char *docno;
    float sim;
    long rank;
} TEXT_TR;

typedef struct {
    char *docno;
} TEXT_QRELS;

typedef struct {
    long qid;
    long num_text_tr;
    long max_num_text_tr;
    TEXT_TR *text_tr;
} TREC_TR;

typedef struct {
    long qid;
    long num_text_qrels;
    long max_num_text_qrels;
    TEXT_QRELS *text_qrels;
} TREC_QRELS;


int
init_trec_tr_obj (spec, unused)
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
                 "Trace: entering/leaving init_trec_tr_obj");

    return (0);
}

int
trec_tr_obj (text_file, tr_file, inst)
char *text_file;
char *tr_file;
int inst;
{
    int tr_fd;
    TREC_TR trec_tr;
    TREC_QRELS trec_qrels;

    PRINT_TRACE (2, print_string, "Trace: entering trec_tr_obj");

    /* Open text tr input file */
    if (UNDEF == init_get_trec_tr (text_file))
        return (UNDEF);

    /* Open text qrels input file */
    if (UNDEF == init_get_trec_qrels (trec_qrels_file))
        return (UNDEF);

    /* Open output tr file */
    if (tr_file == (char *) NULL)
        tr_file = default_tr_file;
    if (UNDEF == (tr_fd = open_tr_vec (tr_file, tr_mode)))
        return (UNDEF);

    /* Get first trec_tr and trec_qrels */
    trec_tr.max_num_text_tr = 0;
    trec_qrels.max_num_text_qrels = 0;
    if (1 != get_trec_tr (&trec_tr) ||
        1 != get_trec_qrels (&trec_qrels)) {
        return (UNDEF);
    }

    while (trec_tr.qid != UNDEF && trec_qrels.qid != UNDEF) {
        if (trec_tr.qid == trec_qrels.qid) {
            if (UNDEF == form_and_write_trvec (tr_fd, &trec_tr, &trec_qrels))
                return (UNDEF);
            if (UNDEF == get_trec_tr (&trec_tr) ||
                UNDEF == get_trec_qrels (&trec_qrels)) {
                return (UNDEF);
            }
        }
        else if (trec_tr.qid < trec_qrels.qid) {
            if (UNDEF == get_trec_tr (&trec_tr))
                return (UNDEF);
        }
        else {
            if (UNDEF == get_trec_qrels (&trec_qrels))
                return (UNDEF);
        }
    }

    if (UNDEF == close_tr_vec (tr_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving trec_tr_obj");
    return (1);
}

int
close_trec_tr_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string,
                 "Trace: entering/leaving close_trec_tr_obj");
    return (0);
}

/* Space reserved for output TR_TUP tuples */
static TR_TUP *start_tr_tup;
static long max_tr_tup;


static int
form_and_write_trvec (tr_fd, trec_tr, trec_qrels)
int tr_fd;
TREC_TR *trec_tr;
TREC_QRELS *trec_qrels;
{
    TR_TUP *tr_tup;
    TEXT_QRELS *qrels_ptr, *end_qrels;
    long i;

    TR_VEC tr_vec;

    /* Reserve space for output tr_tups, if needed */
    if (trec_tr->num_text_tr > max_tr_tup) {
        if (max_tr_tup > 0) 
            (void) free ((char *) start_tr_tup);
        max_tr_tup += trec_tr->num_text_tr;
        if (NULL == (start_tr_tup = (TR_TUP *)
                     malloc ((unsigned) max_tr_tup * sizeof (TR_TUP))))
            return (UNDEF);
    }

    /* Sort trec_tr by sim, breaking ties lexicographically using docno */
    qsort ((char *) trec_tr->text_tr,
           (int) trec_tr->num_text_tr,
           sizeof (TEXT_TR),
           comp_sim_docno);

    /* Add ranks to trec_tr (starting at 1) */
    for (i = 0; i < trec_tr->num_text_tr; i++) {
        trec_tr->text_tr[i].rank = i+1;
    }

    /* Sort trec_tr lexicographically */
    qsort ((char *) trec_tr->text_tr,
           (int) trec_tr->num_text_tr,
           sizeof (TEXT_TR),
           comp_tr_docno);

    /* Sort trec_qrels lexicographically */
    qsort ((char *) trec_qrels->text_qrels,
           (int) trec_qrels->num_text_qrels,
           sizeof (TEXT_QRELS),
           comp_qrels_docno);

    /* Go through trec_tr, trec_qrels in parallel to determine which
       docno's are in both (ie, which trec_tr are relevant).  Once relevance
       is known, convert trec_tr tuple into TR_TUP. */
    tr_tup = start_tr_tup;
    qrels_ptr = trec_qrels->text_qrels;
    end_qrels = &trec_qrels->text_qrels[trec_qrels->num_text_qrels];
    for (i = 0; i < trec_tr->num_text_tr; i++) {
        while (qrels_ptr < end_qrels &&
               strcmp (qrels_ptr->docno, trec_tr->text_tr[i].docno) < 0)
            qrels_ptr++;
        if (qrels_ptr >= end_qrels ||
            strcmp (qrels_ptr->docno, trec_tr->text_tr[i].docno) > 0)
            /* Doc is non-relevant */
            tr_tup->rel = 0;
        else 
            /* Doc is relevant */
            tr_tup->rel = 1;
        
        tr_tup->did = i;
        tr_tup->rank = trec_tr->text_tr[i].rank;
        tr_tup->sim = trec_tr->text_tr[i].sim;
        tr_tup->action = 0;
        tr_tup->iter = 0;
        tr_tup++;
    }
    /* Form and output the full TR_VEC object for this qid */
    /* First, sort trec_qrels lexicographically */
    qsort ((char *) start_tr_tup,
           (int) trec_tr->num_text_tr,
           sizeof (TR_TUP),
           comp_tr_tup_did);
    tr_vec.qid = trec_tr->qid;
    tr_vec.num_tr = trec_tr->num_text_tr;
    tr_vec.tr = start_tr_tup;
    if (UNDEF == seek_tr_vec (tr_fd, &tr_vec) ||
        1 != write_tr_vec (tr_fd, &tr_vec))
        return (UNDEF);

    return (1);
}

static int 
comp_sim_docno (ptr1, ptr2)
TEXT_TR *ptr1;
TEXT_TR *ptr2;
{
    if (ptr1->sim > ptr2->sim)
        return (-1);
    if (ptr1->sim < ptr2->sim)
        return (1);
    return (strcmp (ptr2->docno, ptr1->docno));
}

static int 
comp_tr_docno (ptr1, ptr2)
TEXT_TR *ptr1;
TEXT_TR *ptr2;
{
    return (strcmp (ptr1->docno, ptr2->docno));
}

static int 
comp_qrels_docno (ptr1, ptr2)
TEXT_QRELS *ptr1;
TEXT_QRELS *ptr2;
{
    return (strcmp (ptr1->docno, ptr2->docno));
}

static int 
comp_tr_tup_did (ptr1, ptr2)
TR_TUP *ptr1;
TR_TUP *ptr2;
{
    return (ptr1->did - ptr2->did);
}

static char *trec_tr_buf;           /* Buffer for entire trec_tr file */
static char *trec_tr_ptr;           /* Current position in trec_tr_buf */

/* Current trec_tr input line, broken up into tokens */
static SM_TEXTLINE trec_tr_line;
static char *trec_tr_array[6];

static int 
init_get_trec_tr (filename)
char *filename;
{
    int fd;
    int size;

    PRINT_TRACE (9, print_string, "Trace: entering init_get_trec_tr");

    /* Read entire file into memory */
    if (-1 == (fd = open (filename, O_RDONLY)) ||
        -1 == (size = lseek (fd, 0L, 2)) ||
        NULL == (trec_tr_buf = malloc ((unsigned) size+1)) ||
        -1 == lseek (fd, 0L, 0) ||
        size != read (fd, trec_tr_buf, size) ||
        -1 == close (fd))
        return (UNDEF);
    trec_tr_buf[size] = '\0';

    trec_tr_line.max_num_tokens = 6;
    trec_tr_line.tokens = trec_tr_array;

    trec_tr_ptr = trec_tr_buf;
    while (*trec_tr_ptr && *trec_tr_ptr != '\n')
        trec_tr_ptr++;
    if (! *trec_tr_ptr) {
        /* No lines in tr_file */
        set_error (SM_ILLPA_ERR, "Illegal tr input line", "trec_tr");
        return (UNDEF);
    }
    /* Replace newline by \0, and break into tokens */
    *trec_tr_ptr++ = '\0';
    PRINT_TRACE (7, print_string, trec_tr_buf);
    if (UNDEF == text_textline (trec_tr_buf, &trec_tr_line) ||
        trec_tr_line.num_tokens != 6) {
        set_error (SM_ILLPA_ERR, "Illegal tr input line", "trec_tr");
        return (UNDEF);
    }
    PRINT_TRACE (9, print_string, "Trace: leaving init_get_trec_tr");
    return (1);
}


static char *trec_qrels_buf;           /* Buffer for entire trec_qrels file */
static char *trec_qrels_ptr;           /* Next line in trec_qrels_buf */

/* Current trec_qrels input line, broken up into tokens */
static SM_TEXTLINE trec_qrels_line;
static char *trec_qrels_array[6];

static int 
init_get_trec_qrels (filename)
char *filename;
{
    int fd;
    int size;

    PRINT_TRACE (9, print_string, "Trace: entering init_get_trec_qrels");
    PRINT_TRACE (9, print_string, filename);

    /* Read entire file into memory */
    if (-1 == (fd = open (filename, O_RDONLY)) ||
        -1 == (size = lseek (fd, 0L, 2)) ||
        NULL == (trec_qrels_buf = malloc ((unsigned) size+1)) ||
        -1 == lseek (fd, 0L, 0) ||
        size != read (fd, trec_qrels_buf, size) ||
        -1 == close (fd))
        return (UNDEF);
    trec_qrels_buf[size] = '\0';

    PRINT_TRACE (9, print_string, "Trace: entering init_get_trec_qrels");
    trec_qrels_line.max_num_tokens = 4;
    trec_qrels_line.tokens = trec_qrels_array;

    trec_qrels_ptr = trec_qrels_buf;
    while (*trec_qrels_ptr && *trec_qrels_ptr != '\n')
        trec_qrels_ptr++;
    if (! *trec_qrels_ptr) {
        /* No lines in tr_file */
        set_error (SM_ILLPA_ERR, "Illegal qrels input line", "trec_tr");
        return (UNDEF);
    }
    /* Replace newline by \0, and break into tokens */
    *trec_qrels_ptr++ = '\0';
    PRINT_TRACE (7, print_string, trec_qrels_buf);
    if (UNDEF == text_textline (trec_qrels_buf, &trec_qrels_line) ||
        trec_qrels_line.num_tokens != 4) {
        set_error (SM_ILLPA_ERR, "Illegal qrels input line", "trec_tr");
        return (UNDEF);
    }
    PRINT_TRACE (9, print_string, "Trace: leaving init_get_trec_qrels");
    return (1);
}


/* Get entire trec_tr vector for next query */
static int
get_trec_tr (trec_tr)
TREC_TR *trec_tr;
{
    long qid;
    char *start_line;

    PRINT_TRACE (9, print_string, "Trace: entering get_trec_tr");

    qid = trec_tr->qid = atol (trec_tr_line.tokens[0]);
    if (qid == -1)
        return (0);

    trec_tr->num_text_tr = 0;
    while (qid == trec_tr->qid) {
        /* Make sure enough space is reserved for next tuple */
        if (trec_tr->num_text_tr >= trec_tr->max_num_text_tr) {
            if (trec_tr->max_num_text_tr == 0) {
                if (NULL == (trec_tr->text_tr = (TEXT_TR *) 
                             malloc ((unsigned) TR_INCR * sizeof (TEXT_TR))))
                    return (UNDEF);
            }
            else if (NULL == (trec_tr->text_tr = (TEXT_TR *)
                           realloc ((char *) trec_tr->text_tr,
                                    (unsigned) (trec_tr->max_num_text_tr +
                                                TR_INCR) * sizeof (TEXT_TR))))
                return (UNDEF);
            trec_tr->max_num_text_tr += TR_INCR;
        }
        /* Add current textline tuple to trec_tr */
        trec_tr->text_tr[trec_tr->num_text_tr].docno = trec_tr_line.tokens[2];
        trec_tr->text_tr[trec_tr->num_text_tr].sim = atof (trec_tr_line.tokens[4]);
        trec_tr->num_text_tr++;

        /* Get next textline tuple */
        do {
            start_line = trec_tr_ptr;
            while (*trec_tr_ptr && *trec_tr_ptr != '\n')
                trec_tr_ptr++;
            if (! *trec_tr_ptr) {
                /* No more lines in tr_file */
                trec_tr_line.tokens[0] = "-1";
                return (1);
            }
            /* Replace newline by \0, and break into tokens */
            *trec_tr_ptr++ = '\0';
            PRINT_TRACE (7, print_string, start_line);
            if (UNDEF == text_textline (start_line, &trec_tr_line) ||
                (trec_tr_line.num_tokens != 6 &&
                 trec_tr_line.num_tokens != 0)) {
                set_error (SM_ILLPA_ERR, "Illegal tr input line", "trec_tr");
                return (UNDEF);
            }
        } while (trec_tr_line.num_tokens == 0) ;
        qid = atol (trec_tr_line.tokens[0]);
    }

    PRINT_TRACE (9, print_string, "Trace: leaving get_trec_tr");
    return (1);
}


/* Get entire trec_qrels vector for next query */
static int
get_trec_qrels (trec_qrels)
TREC_QRELS *trec_qrels;
{
    long qid;
    char *start_line;

    PRINT_TRACE (9, print_string, "Trace: entering get_trec_qrels");
    qid = trec_qrels->qid = atol (trec_qrels_line.tokens[0]);
    if (qid == -1)
        return (0);
    trec_qrels->num_text_qrels = 0;
    while (qid == trec_qrels->qid) {
        /* Make sure enough space is reserved for next tuple */
        if (trec_qrels->num_text_qrels >= trec_qrels->max_num_text_qrels) {
            if (trec_qrels->max_num_text_qrels == 0) {
                if (NULL == (trec_qrels->text_qrels = (TEXT_QRELS *) 
                            malloc ((unsigned) TR_INCR * sizeof (TEXT_QRELS))))
                    return (UNDEF);
            }
            else if (NULL == (trec_qrels->text_qrels = (TEXT_QRELS *)
                        realloc ((char *) trec_qrels->text_qrels,
                                 (unsigned) (trec_qrels->max_num_text_qrels +
                                             TR_INCR) * sizeof (TEXT_QRELS))))
                return (UNDEF);
            trec_qrels->max_num_text_qrels += TR_INCR;
        }
        /* Add current textline tuple to trec_qrels as long as relevant */
        trec_qrels->text_qrels[trec_qrels->num_text_qrels].docno =
            trec_qrels_line.tokens[2];
        if (atol (trec_qrels_line.tokens[3]))
            trec_qrels->num_text_qrels++;

        /* Get next textline tuple */
        do {
            start_line = trec_qrels_ptr;
            while (*trec_qrels_ptr && *trec_qrels_ptr != '\n')
                trec_qrels_ptr++;
            if (! *trec_qrels_ptr) {
                /* No more lines in tr_file */
                trec_qrels_line.tokens[0] = "-1";
                return (1);
            }
            /* Replace newline by \0, and break into tokens */
            *trec_qrels_ptr++ = '\0';
            PRINT_TRACE (7, print_string, start_line);
            if (UNDEF == text_textline (start_line, &trec_qrels_line) ||
                (trec_qrels_line.num_tokens != 4 &&
                trec_qrels_line.num_tokens != 0)) {
                set_error (SM_ILLPA_ERR,
                           "Illegal qrels input line", "trec_qrels");
                return (UNDEF);
            }
        } while (trec_qrels_line.num_tokens == 0);
        qid = atol (trec_qrels_line.tokens[0]);
    }

    PRINT_TRACE (9, print_string, "Trace: leaving get_trec_qrels");
    return (1);
}
