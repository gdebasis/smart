#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/tr_trec_o.c,v 11.2 1993/02/03 16:51:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take an entire smart tr results file, and convert to text TREC results file
 *1 local.convert.obj.tr_trec
 *2 tr_trec_obj (tr_file, text_file, inst)
 *3 char *tr_file;
 *3 char *text_file;
 *3 int inst;
 *4 init_tr_trec_obj (spec, unused)
 *5   "tr_trec_obj.run_id"
 *5   "tr_trec_obj.tr_file"
 *5   "tr_trec_obj.tr_file.rmode"
 *5   "tr_trec_obj.doc_file"
 *5   "tr_trec_obj.doc_file.rmode"
 *5   "tr_trec_obj.dict_file"
 *5   "tr_trec_obj.dict_file.rmode"
 *5   "tr_trec_obj.trace"
 *4 close_tr_trec_obj (inst)

 *7 Read TR_VEC tuples from tr_file and convert to TREC format.  This involves
 *7 changing the internal SMART did to the official TREC DOCNO.
 *7 Input is conceptually tuples of the form
 *7     qid  did  rank  action  rel  iter  sim
 *7 Output text tuples to text_file of the form
 *7     030  Q0  ZF08-175-870  0   4238   prise1
 *&     qid iter   docno      rank  sim   run_id
 *7 giving TREC document numbers (a string) retrieved by query qid 
 *7 (an integer) with similarity sim (a float).  iter is always assumed to
 *7 be 0, rank is the rank given by tr_vec, run_id is supplied
 *7 by the parameter run_id, and is the same for all tuples.
 *7 If text_file is NULL or non-valid (eg "-"), then stdout is used.
 *7 If tr_file is NULL, then value of tr_file 
 *7 spec parameter is used.
 *7 Return UNDEF if error, else 1.  It is an error if tr_file exists.

 *8 Procedure is to assume we have a vector file that maps did into an
 *8 internal concept (each vector is composed of exactly one concept).
 *8 That concept is then looked up in a previously constructed dictionary 
 *8 to get the string docno to be output.
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
#include "vector.h"
#include "eid.h"

static char *run_id;
static long iter_flag;
static char *default_tr_file;
static long tr_mode;
static char *doc_file;
static long doc_mode;
static char *dict_file;
static long dict_mode;

static SPEC_PARAM spec_args[] = {
    {"tr_trec_obj.run_id",       getspec_string,   (char *) &run_id},
    {"tr_trec_obj.need_iter",    getspec_bool,     (char *) &iter_flag},
    {"tr_trec_obj.tr_file",      getspec_dbfile,   (char *) &default_tr_file},
    {"tr_trec_obj.tr_file.rmode", getspec_filemode,(char *) &tr_mode},
    {"tr_trec_obj.doc_file" ,    getspec_dbfile,   (char *) &doc_file},
    {"tr_trec_obj.doc_file.rmode", getspec_filemode,(char *) &doc_mode},
    {"tr_trec_obj.dict_file",    getspec_dbfile,   (char *) &dict_file},
    {"tr_trec_obj.dict_file.rmode",  getspec_filemode, (char *) &dict_mode},
    TRACE_PARAM ("tr_trec_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

int
init_tr_trec_obj (spec, unused)
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
                 "Trace: entering/leaving init_tr_trec_obj");

    return (0);
}

int
tr_trec_obj (tr_file, text_file, inst)
char *tr_file;
char *text_file;
int inst;
{
    FILE *out_fd;
    int tr_fd, doc_fd, dict_fd;
    long i;
    VEC vec;
    TR_VEC tr_vec;
    DICT_ENTRY dict;
    char *ptr;
    char temp_token[20];

    PRINT_TRACE (2, print_string, "Trace: entering tr_trec_obj");

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
        UNDEF == (doc_fd = open_vector (doc_file, doc_mode)) ||
        UNDEF == (dict_fd = open_dict (dict_file, dict_mode)))
        return (UNDEF);

    while (1 == read_tr_vec (tr_fd, &tr_vec)) {
        PRINT_TRACE (7, print_tr_vec, &tr_vec);
        for (i = 0; i < tr_vec.num_tr; i++) {
            /* Lookup did in vector file to get internal concept */
            vec.id_num.id = tr_vec.tr[i].did;
	    EXT_NONE(vec.id_num.ext);
            PRINT_TRACE (7, print_long, &vec.id_num);
            if (1 != seek_vector (doc_fd, &vec) ||
                1 != read_vector (doc_fd, &vec) ||
                1 != vec.num_conwt) {
                (void) sprintf (temp_token, "SMART%ld", tr_vec.tr[i].did);
                dict.token = temp_token;
            }
            else {
                /* Lookup internal concept in dictionary to get string */
                dict.token = NULL;
                dict.con = vec.con_wtp[0].con;
                PRINT_TRACE (7, print_long, &dict.con);
                if (1 != seek_dict (dict_fd, &dict) ||
                    1 != read_dict (dict_fd, &dict))
                    return (UNDEF);
            }

            (void) fprintf (out_fd, "%ld\tQ%d\t", tr_vec.qid, 
                            iter_flag ? tr_vec.tr[i].iter : 0);
            /* Output dict.token in all upper-case letters */
            for (ptr = dict.token; *ptr; ptr++) {
                if (islower (*ptr))
                    (void) putc (toupper (*ptr), out_fd);
                else
                    (void) putc (*ptr, out_fd);
            }
            (void) fprintf (out_fd, "\t%ld\t%10.6f\t%s\n",
                     tr_vec.tr[i].rank,
                     tr_vec.tr[i].sim,
                     run_id);
        }
    }

    if (UNDEF == close_tr_vec (tr_fd) ||
        UNDEF == close_vector (doc_fd) ||
        UNDEF == close_dict (dict_fd)) {
        return (UNDEF);
    }

    if (VALID_FILE (text_file))
        (void) fclose (out_fd);

    PRINT_TRACE (2, print_string, "Trace: leaving tr_trec_obj");
    return (1);
}

int
close_tr_trec_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string,
                 "Trace: entering/leaving close_tr_trec_obj");
    return (0);
}
