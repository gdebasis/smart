#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/pobj_tr_vec.c,v 11.2 1993/02/03 16:54:11 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   *****************************************
 *0 print tr_vec object
 *1 print.obj.tr_qdname
 *2 print_obj_tr_qdname (in_file, out_file, inst)
 *3   char *in_file;
 *3   char *out_file;
 *3   int inst;
 *4 init_print_obj_tr_qdname (spec, unused)
 *5   "print.doc.textloc_file"
 *5   "print.doc.textloc_file.rmode"
 *5   "print.tr_file"
 *5   "print.tr_file.rmode"
 *5   "print.trace"
 *4 close_print_obj_tr_qdname (inst)
 *6 global_start,global_end used to indicate what range of cons will be printed
 *7 The tr_vecdict relation "in_file" (if not VALID_FILE, then use tr_file),
 *7 will be used to print all tr_vecdict entries in that file (modulo global_start,
 *7 global_end).  Tr_vec output to go into file "out_file" (if not VALID_FILE,
 *7 then stdout). Uses the dictionary to print the document names and not just
 *7 the integer identifiers.
 This file is used to print the CLEF-IP run submission where the queries are
 names instead of numbers.
 Note: We print the exact TREC formatted results which need
 not be post-processed by any script.
****************************************************************************************/

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "tr_vec.h"
#include "buf.h"
#include "textloc.h"
#include "docdesc.h"

static char *tr_vec_file;
static long tr_vec_mode;
static char *docno_file;
static long docno_mode;
static char *run_name;
static int  docno_fd; 
static SM_INDEX_DOCDESC doc_desc;
static int contok_inst;
static char *textloc_file;
static long textloc_mode;
static char *qtextloc_file;
static long qtextloc_mode;
static int  textloc_index;
static int  qtextloc_index;
static TR_TUP* trtup_buff;
static int trtup_buff_size;

extern int l_print_tr_qdname (TR_TUP**, int* buffSize, TR_VEC* tr_vec, int docno_fd, int textloc_index, int qtextloc_index, SM_BUF* output, char* run_name, SM_INDEX_DOCDESC*, int*);

static SPEC_PARAM spec_args[] = {
    {"print.doc.docno_file",         getspec_dbfile,   (char *) &docno_file},
    {"print.doc.docno_file.rmode", getspec_filemode, (char *) &docno_mode},
    {"print.tr_file",                getspec_dbfile,   (char *) &tr_vec_file},
    {"print.tr_file.rmode",          getspec_filemode, (char *) &tr_vec_mode},
    {"print.doc.textloc_file",     getspec_dbfile, (char *) &textloc_file},
    {"print.doc.textloc_file.rmode",getspec_filemode, (char *) &textloc_mode},
    {"print.query.textloc_file",     getspec_dbfile, (char *) &qtextloc_file},
    {"print.query.textloc_file.rmode",getspec_filemode, (char *) &qtextloc_mode},
    {"print.run_name",               getspec_string,   (char *) &run_name},
    TRACE_PARAM ("print.trace")
};
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


int
l_init_print_obj_tr_qdname (spec, unused)
SPEC *spec;
char *unused;
{
    PRINT_TRACE (2, print_string, "Trace: entering l_init_print_obj_tr_qdname");

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    if (UNDEF == lookup_spec_docdesc (spec, &doc_desc))
	    return (UNDEF);

	/* Read the vector file for docno mapping */
    if (UNDEF == (docno_fd = open_vector (docno_file, docno_mode)))
        return (UNDEF);

    if (UNDEF == (textloc_index = open_textloc (textloc_file, textloc_mode)))
        return (UNDEF);
    if (UNDEF == (qtextloc_index = open_textloc (qtextloc_file, qtextloc_mode)))
        return (UNDEF);
	trtup_buff_size = 1000;
	if (NULL == (trtup_buff = Malloc(trtup_buff_size, TR_TUP)))
		return UNDEF;

	/* This will be used by the con_to_token routine to lookup
	*  parameters it needs. */
    if (UNDEF == (contok_inst =
                doc_desc.ctypes[0].con_to_token->init_proc
                 (spec, "index.ctype.0.")))
		return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving l_init_print_obj_tr_qdname");
    return (0);
}

int
l_print_obj_tr_qdname (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    TR_VEC  tr_vec;
	TEXTLOC textloc;
    int     status;
    char    *final_tr_vec_file;
    FILE    *output;
    SM_BUF  output_buf;
    int     tr_vec_index;              /* file descriptor for tr_vec_file */

    PRINT_TRACE (2, print_string, "Trace: entering l_print_obj_tr_qdname");

    final_tr_vec_file = VALID_FILE (in_file) ? in_file : tr_vec_file;
    output = VALID_FILE (out_file) ? fopen (out_file, "w") : stdout;
    if (NULL == output)
        return (UNDEF);
    output_buf.size = 0;

    if (UNDEF == (tr_vec_index = open_tr_vec (final_tr_vec_file, tr_vec_mode)))
        return (UNDEF);

    /* Get each tr_vec in turn */
    if (global_start > 0) {
        tr_vec.qid = global_start;
        if (UNDEF == seek_tr_vec (tr_vec_index, &tr_vec)) {
            return (UNDEF);
        }
    }

    while (1 == (status = read_tr_vec (tr_vec_index, &tr_vec)) &&
           tr_vec.qid <= global_end) {
        output_buf.end = 0;

        if ( l_print_tr_qdname (&trtup_buff, &trtup_buff_size, &tr_vec,
								docno_fd, textloc_index, qtextloc_index, &output_buf,
								run_name, &doc_desc, contok_inst) == UNDEF )
			return UNDEF;

        (void) fwrite (output_buf.buf, 1, output_buf.end, output);
    }

    if (UNDEF == close_tr_vec (tr_vec_index))
        return (UNDEF);

    if (output != stdin)
        (void) fclose (output);

    PRINT_TRACE (2, print_string, "Trace: leaving l_print_obj_tr_qdname");
    return (status);
}


int
l_close_print_obj_tr_qdname (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering l_close_print_obj_tr_qdname");

    if (UNDEF == close_vector (docno_fd))
        return (UNDEF);
    if (UNDEF == close_textloc (textloc_index))
        return (UNDEF);
    if (UNDEF == close_textloc (qtextloc_index))
        return (UNDEF);
    if (UNDEF == doc_desc.ctypes[0].con_to_token->
            close_proc(contok_inst))
	    return (UNDEF);
	Free(trtup_buff);

    PRINT_TRACE (2, print_string, "Trace: leaving l_close_print_obj_tr_qdname");
    return (0);
}
