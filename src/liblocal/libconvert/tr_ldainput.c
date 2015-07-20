#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/tr_lda_input.c,v 11.2 1993/02/03 16:51:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 * Take a SMART generated tr file and convert it to a file conforming
 * to the format of an LDA tool.
 * Currently it's done for the GibsLDA++ tool which has the following
 * input format.
 * [M]
 * [document_1]
 * [document_2]
 * ...
 * [document_M]
 *
 *0 Take an entire smart tr results file, and convert to the above format
    for each query in a separate file
***********************************************************************/

#include <ctype.h>
#include "vector.h"
#include "common.h"
#include "param.h"
#include "io.h"
#include "functions.h"
#include "spec.h"
#include "trace.h"
#include "smart_error.h"
#include "tr_vec.h"
#include "textloc.h"
#include "docdesc.h"
#include "buf.h"

static int did_vec_inst;
static char *default_tr_file;
static long tr_mode;
static char *textloc_file;
static long textloc_mode;
static long format;
static int num_top_docs; // Number of documents used to estimate the relevance model
static int num_ctypes = 1;   // write out only the words

static SM_INDEX_DOCDESC doc_desc;
static int contok_inst;

static SPEC_PARAM spec_args[] = {
    {"tr_lda_input.tr_file",      getspec_dbfile,   (char *) &default_tr_file},
    {"tr_lda_input.tr_file.rmode", getspec_filemode,(char *) &tr_mode},
    {"tr_lda_input.output_format", getspec_long, (char *) &format},
    TRACE_PARAM ("tr_lda_input.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int print_vec_lda (VEC *vec, SM_BUF *output, int inst);


int init_tr_lda_input (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
	}

    if (UNDEF == lookup_spec_docdesc (spec, &doc_desc))
        return (UNDEF);

    /* Set param_prefix to be current parameter path for this ctype.
       This will then be used by the con_to_token routine to lookup
       parameters it needs. */
    if (UNDEF == (contok_inst =
                  doc_desc.ctypes[0].con_to_token->init_proc
                  (spec, "index.ctype.0.")))
        return (UNDEF);

    if (UNDEF == (did_vec_inst = init_did_vec (spec, "doc.")))
		        return (UNDEF);

    PRINT_TRACE (2, print_string,
                 "Trace: entering/leaving init_tr_lda_input");

    return (0);
}

int
tr_lda_input (tr_file, text_file_base, inst)
char *tr_file;
char *text_file_base;
int inst;
{
    char *fname, *extension;
    int tr_fd, status;
    long i, did, qid;
    FILE *out_fd;
	VEC  dvec;
    TR_VEC tr_vec;
    SM_BUF output_buf;
	static char text_file_name[256];

    PRINT_TRACE (2, print_string, "Trace: entering tr_lda_input");

    output_buf.size = 0;

    /* Open input doc and dict files */
    if (tr_file == (char *) NULL)
        tr_file = default_tr_file;
    if (UNDEF == (tr_fd = open_tr_vec (tr_file, tr_mode)))
		return UNDEF;

    while (1 == read_tr_vec (tr_fd, &tr_vec)) {
		qid = tr_vec.qid;

       	output_buf.end = 0;
		snprintf(text_file_name, sizeof(text_file_name), "%s.%d.dm", text_file_base, qid);
	    /* Open text output file */
   	    if (NULL == (out_fd = fopen (text_file_name, "w")))
       	    return (UNDEF);

		// Write the number of unique documents
		snprintf(text_file_name, sizeof(text_file_name), "%d\n", tr_vec.num_tr);
    	add_buf_string (text_file_name, &output_buf);
       	fwrite (output_buf.buf, 1, output_buf.end, out_fd);

		// Write each vector
        for (i = 0; i < tr_vec.num_tr; i++) {
            did = tr_vec.tr[i].did;
        	if (UNDEF == (status = did_vec (&did, &dvec, did_vec_inst)))
				return UNDEF;
        	output_buf.end = 0;
        	if (UNDEF == print_vec_lda (&dvec, &output_buf, inst))
            	return (UNDEF);
        	fwrite (output_buf.buf, 1, output_buf.end, out_fd);
        }

    	fclose (out_fd);
    }

    if (UNDEF == close_tr_vec (tr_fd))
        return (UNDEF);


    PRINT_TRACE (2, print_string, "Trace: leaving tr_lda_input");
    return (1);
}

int
close_tr_lda_input (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_tr_lda_input");

    if (UNDEF == doc_desc.ctypes[0].con_to_token->
        close_proc(contok_inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering close_tr_lda_input");

    return (0);
}

static int print_vec_lda (vec, output, inst)
VEC *vec;
SM_BUF *output;
int inst;
{
    long i;
	int j;
    CON_WT *conwtp;
    char buf[32];
    char *token;

    PRINT_TRACE (2, print_string, "Trace: entering print_vec_lda");

    for (i = 0, conwtp = vec->con_wtp; i < vec->ctype_len[0]; i++, conwtp++) {
        if (UNDEF == doc_desc.ctypes[0].con_to_token->
            proc (&conwtp->con, &token, contok_inst)) {
            token = "";
            clr_err();
        }

		// A word has to be repeated tf times. Note: LDA assumes the vector
		// representation and hence word order is not important. 
		for (j = 0; j < conwtp->wt; j++) {
        	add_buf_string (token, output);
        	add_buf_string (" ", output);
    	}
    }
   	add_buf_string ("\n", output);

    PRINT_TRACE (2, print_string, "Trace: leaving print_vec_lda");
    return (1);
}


