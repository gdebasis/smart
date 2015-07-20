#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/trec_qrels_o.c,v 11.2 1993/02/03 16:51:42 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Convert an entire TREC qrels text file to SMART RR format.
 *1 local.convert.obj.trec_qrels
 *2 trec_qrels_obj (text_file, qrels_file, inst)
 *3 char *text_file;
 *3 char *qrels_file;
 *3 int inst;

 *4 init_trec_qrels_obj (spec, unused)
 *5   "trec_qrels_obj.qrels_file"
 *5   "trec_qrels_obj.qrels_file.rwcmode"
 *5   "trec_qrels_obj.dict_file"
 *5   "trec_qrels_obj.dict_file.rmode"
 *5   "trec_qrels_obj.inv_file"
 *5   "trec_qrels_obj.inv_file.rmode"
 *5   "trec_qrels_obj.trace"
 *4 close_trec_qrels_obj (inst)

 *7 Read text tuples from text_file of the form
 *7    qid  iter  docno  rel
 *7 giving TREC document numbers (a string) and their relevance to query qid 
 *7 (an integer). Input is asssumed to be sorted by qid.  The text tuples
 *7 are converted to RR_VEC and written to the RR_VEC relational object 
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
#include "rr_vec.h"
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
    {"trec_qrels_obj.qrels_file",  getspec_dbfile, (char *) &default_qrels_file},
    {"trec_qrels_obj.qrels_file.rwcmode", getspec_filemode,(char *) &qrels_mode},
    {"trec_qrels_obj.dict_file",  getspec_dbfile, (char *) &dict_file},
    {"trec_qrels_obj.dict_file.rmode", getspec_filemode, (char *) &dict_mode},
    {"trec_qrels_obj.inv_file",  getspec_dbfile, (char *) &inv_file},
    {"trec_qrels_obj.inv_file.rmode", getspec_filemode, (char *) &inv_mode},
    TRACE_PARAM ("trec_qrels_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int comp_did(), lookup_dict();

int
init_trec_qrels_obj (spec, unused)
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
                 "Trace: entering/leaving init_trec_qrels_obj");

    return (0);
}

int
trec_qrels_obj (text_file, qrels_file, inst)
char *text_file;
char *qrels_file;
int inst;
{
    RR_VEC rr_vec;
    unsigned rr_size;       /* Number of RR_TUP elements space allocated */

    FILE *in_fd;
    int rr_fd, dict_fd, inv_fd;
    char line_buf[PATH_LEN];
    SM_TEXTLINE textline;
    char *token_array[4];

    /* Values on the input line */
    long qid;
    char *docno;
    long rel;

    PRINT_TRACE (2, print_string, "Trace: entering trec_qrels_obj");

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
    if (UNDEF == (rr_fd = open_rr_vec (qrels_file, qrels_mode)) ||
        UNDEF == (dict_fd = open_dict (dict_file, dict_mode)) ||
        UNDEF == (inv_fd = open_inv (inv_file, inv_mode)))
        return (UNDEF);

    /* Allocate initial space for qrels vector */
    rr_vec.qid = 0;
    rr_vec.num_rr = 0;
    rr_size = 200;
    if (NULL == (rr_vec.rr = (RR_TUP *) malloc (rr_size * sizeof (RR_TUP))))
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
            set_error (SM_ILLPA_ERR, "Illegal input line", "trec_qrels");
            return (UNDEF);
        }
        qid = atol (textline.tokens[0]);
        docno = textline.tokens[2];
        rel = atol (textline.tokens[3]);
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
            if (UNDEF == lookup_dict (dict_fd,
                                      inv_fd,
                                      docno,
                                      &rr_vec.rr[rr_vec.num_rr].did))
/*                return (UNDEF); */
                continue;
            rr_vec.rr[rr_vec.num_rr].rank = 0;
            rr_vec.rr[rr_vec.num_rr].sim = 0.0;
            rr_vec.num_rr++;
    		PRINT_TRACE (2, print_string, "Trace: Got a relevant document from the qrel file");

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

    if (UNDEF == close_rr_vec (rr_fd) ||
        UNDEF == close_dict (dict_fd) ||
        UNDEF == close_inv (inv_fd)) {
        return (UNDEF);
    }
    
    if (VALID_FILE (text_file))
        (void) fclose (in_fd);

    PRINT_TRACE (2, print_string, "Trace: leaving trec_qrels_obj");
    return (1);
}

int
close_trec_qrels_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string,
                 "Trace: entering/leaving close_trec_qrels_obj");
    return (0);
}


static int 
comp_did (rr1, rr2)
RR_TUP *rr1;
RR_TUP *rr2;
{
    return (rr1->did - rr2->did);
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
        set_error (SM_ILLPA_ERR, "NULL docno", "trec_qrels");
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
        set_error (SM_INCON_ERR, docno, "trec_qrels.dict");
        return (UNDEF);
    }

    PRINT_TRACE (7, print_long, &dict.con);

    inv.id_num = dict.con;
    if (1 != seek_inv (inv_fd, &inv) ||
        1 != read_inv (inv_fd, &inv) ||
        inv.num_listwt < 1) {
/*        inv.num_listwt != 1) { */
        set_error (SM_INCON_ERR, docno, "trec_qrels.inv");
        return (UNDEF);
    }

    *did = inv.listwt[0].list;

    PRINT_TRACE (7, print_long, did);
    return (1);
}

