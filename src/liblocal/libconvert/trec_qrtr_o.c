#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/trec_qrtr_o.c,v 11.2 1993/02/03 16:51:50 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Convert an entire TREC qrels text file to SMART TR format.
 *1 local.convert.obj.trec_qrels_tr
 *2 trec_qrels_tr_obj (text_file, qrels_file, inst)
 *3 char *text_file;
 *3 char *qrels_file;
 *3 int inst;

 *4 init_trec_qrels_tr_obj (spec, unused)
 *5   "trec_qrels_tr_obj.qrels_file"
 *5   "trec_qrels_tr_obj.qrels_file.rwcmode"
 *5   "trec_qrels_tr_obj.dict_file"
 *5   "trec_qrels_tr_obj.dict_file.rmode"
 *5   "trec_qrels_tr_obj.inv_file"
 *5   "trec_qrels_tr_obj.inv_file.rmode"
 *5   "trec_qrels_tr_obj.trace"
 *4 close_trec_qrels_tr_obj (inst)

 *7 Read text tuples from text_file of the form
 *7    qid  iter  docno  rel
 *7 giving TREC document numbers (a string) and their relevance to query qid 
 *7 (an integer). Input is asssumed to be sorted by qid.  The text tuples
 *7 are converted to TR_VEC and written to the TR_VEC relational object 
 *7 qrels_file.
 *7 If text_file is NULL or non-valid (eg "-"), then stdin is used.
 *7 If qrels_file is NULL (normal case), then value of qrels_file 
 *7 spec parameter is used.
 *7 Return UNDEF if error, else 1.  It is an error if either qrels_file
 *7 or dict_file already exists.

 *8 Procedure is to use a previously constructed dictionary of docno's 
 *8 occurring in the collection.
 *8 The concept derived from the dictionary entry of a docno is then
 *8 looked up in a previously constructed inv_file to map to the 
 *8 internal SMART document id for that doc, which is used as a did field 
 *8 in the qrels file. 
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
#include "dict.h"
#include "textline.h"
#include "inv.h"

static char *default_qrels_file;
static long qrels_mode;
static char *dict_file;
static long dict_mode;
static char *inv_file;
static long inv_mode;

static SPEC_PARAM spec_args[] = {
    {"trec_qrels_tr_obj.qrels_file",  getspec_dbfile, (char *) &default_qrels_file},
    {"trec_qrels_tr_obj.qrels_file.rwcmode", getspec_filemode,(char *) &qrels_mode},
    {"trec_qrels_tr_obj.dict_file",  getspec_dbfile, (char *) &dict_file},
    {"trec_qrels_tr_obj.dict_file.rmode", getspec_filemode, (char *) &dict_mode},
    {"trec_qrels_tr_obj.inv_file",  getspec_dbfile, (char *) &inv_file},
    {"trec_qrels_tr_obj.inv_file.rmode", getspec_filemode, (char *) &inv_mode},
    TRACE_PARAM ("trec_qrels_tr_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int comp_did(), lookup_dict();

int
init_trec_qrels_tr_obj (spec, unused)
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
                 "Trace: entering/leaving init_trec_qrels_tr_obj");

    return (0);
}

int
trec_qrels_tr_obj (text_file, qrels_file, inst)
char *text_file;
char *qrels_file;
int inst;
{
    TR_VEC tr_vec;
    unsigned tr_size;       /* Number of TR_TUP elements space allocated */

    FILE *in_fd;
    int tr_fd, dict_fd, inv_fd;
    char line_buf[PATH_LEN];
    SM_TEXTLINE textline;
    char *token_array[4];

    /* Values on the input line */
    long qid;
    char *docno;
    long rel;

    PRINT_TRACE (2, print_string, "Trace: entering trec_qrels_tr_obj");

    /* Open text input file */
    if (VALID_FILE (text_file)) {
        if (NULL == (in_fd = fopen (text_file, "r")))
            return (UNDEF);
    }
    else
        in_fd = stdin;

    /* Open output qrels and input dict and inv files */
    if (qrels_file == (char *) NULL)
        qrels_file = default_qrels_file;
    if (UNDEF == (tr_fd = open_tr_vec (qrels_file, qrels_mode)) ||
        UNDEF == (dict_fd = open_dict (dict_file, dict_mode)) ||
        UNDEF == (inv_fd = open_inv (inv_file, inv_mode)))
        return (UNDEF);

    /* Allocate initial space for qrels vector */
    tr_vec.qid = 0;
    tr_vec.num_tr = 0;
    tr_size = 200;
    if (NULL == (tr_vec.tr = (TR_TUP *) malloc (tr_size * sizeof (TR_TUP))))
        return (UNDEF);
    
    textline.max_num_tokens = 4;
    textline.tokens = token_array;
    
    while (NULL != fgets (line_buf, PATH_LEN, in_fd)) {
        PRINT_TRACE (9, print_string, line_buf);
        /* Break line_buf up into tokens */
        (void) text_textline (line_buf, &textline);
        /* Skip over blank lines, other malformed lines are errors */
        if (textline.num_tokens == 0)
            continue;
        if (textline.num_tokens != 4) {
            set_error (SM_ILLPA_ERR, "Illegal input line", "trec_qrels_tr");
            return (UNDEF);
        }
        qid = atol (textline.tokens[0]);
        docno = textline.tokens[2];
        rel = atol (textline.tokens[3]);
        if (qid != tr_vec.qid) {
            /* Output previous vector after sorting it */
            if (tr_vec.qid != 0) {
                (void) qsort ((char *) tr_vec.tr,
                              (int) tr_vec.num_tr,
                              sizeof (TR_TUP),
                              comp_did);
                if (UNDEF == seek_tr_vec (tr_fd, &tr_vec) ||
                    UNDEF == write_tr_vec (tr_fd, &tr_vec))
                    return (UNDEF);
            }
            tr_vec.qid = qid;
            tr_vec.num_tr = 0;
        }
        if (UNDEF == lookup_dict (dict_fd,
                                  inv_fd,
                                  docno,
                                  &tr_vec.tr[tr_vec.num_tr].did))
            return (UNDEF);

        tr_vec.tr[tr_vec.num_tr].rank = tr_vec.num_tr;
        tr_vec.tr[tr_vec.num_tr].action = 0;
        tr_vec.tr[tr_vec.num_tr].rel = rel;
        tr_vec.tr[tr_vec.num_tr].iter = 0;
        tr_vec.tr[tr_vec.num_tr].trtup_unused = 0;
        tr_vec.tr[tr_vec.num_tr].sim = -tr_vec.num_tr;
        tr_vec.num_tr++;

        /* Allocate more space for this tr vector if needed */
        if (tr_vec.num_tr >= tr_size) {
            tr_size += tr_size;
            if (NULL == (tr_vec.tr = (TR_TUP *)
                         realloc ((char *) tr_vec.tr,
                                  tr_size * sizeof (TR_TUP))))
                return (UNDEF);
        }
    }

    /* Output last vector */
    if (tr_vec.qid != 0 && tr_vec.num_tr > 0) {
        (void) qsort ((char *) tr_vec.tr,
                      (int) tr_vec.num_tr,
                      sizeof (TR_TUP),
                      comp_did);
        if (UNDEF == seek_tr_vec (tr_fd, &tr_vec) ||
            UNDEF == write_tr_vec (tr_fd, &tr_vec))
            return (UNDEF);
    }

    if (UNDEF == close_tr_vec (tr_fd) ||
        UNDEF == close_dict (dict_fd) ||
        UNDEF == close_inv (inv_fd)) {
        return (UNDEF);
    }
    
    if (VALID_FILE (text_file))
        (void) fclose (in_fd);

    PRINT_TRACE (2, print_string, "Trace: leaving trec_qrels_tr_obj");
    return (1);
}

int
close_trec_qrels_tr_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string,
                 "Trace: entering/leaving close_trec_qrels_tr_obj");
    return (0);
}


static int 
comp_did (tr1, tr2)
TR_TUP *tr1;
TR_TUP *tr2;
{
    return (tr1->did - tr2->did);
}


static int
lookup_dict (dict_fd, inv_fd, docno, did)
int dict_fd;
int inv_fd;
char *docno;
long *did;
{
    DICT_ENTRY dict;
    INV inv;
    char *ptr;

    if (*docno == '\0') {
        set_error (SM_ILLPA_ERR, "NULL docno", "trec_qrels_tr");
        return (UNDEF);
    }
    /* Translate docno to all lower-case letters */
    for (ptr = docno; *ptr; ptr++) {
        if (isupper (*ptr))
            *ptr = tolower (*ptr);
    }

    PRINT_TRACE (7, print_string, docno);

    dict.token = docno;
    dict.info = 0;
    dict.con = UNDEF;

    if (1 != seek_dict (dict_fd, &dict) ||
        1 != read_dict (dict_fd, &dict)) {
        set_error (SM_INCON_ERR, docno, "trec_qrels_tr.dict");
        return (UNDEF);
    }

    PRINT_TRACE (7, print_long, &dict.con);

    inv.id_num = dict.con;
    if (1 != seek_inv (inv_fd, &inv) ||
        1 != read_inv (inv_fd, &inv) ||
        inv.num_listwt < 1) {
/*        inv.num_listwt != 1) { */
        set_error (SM_INCON_ERR, docno, "trec_qrels_tr.inv");
        return (UNDEF);
    }

    *did = inv.listwt[0].list;

    PRINT_TRACE (7, print_long, did);
    return (1);
}

