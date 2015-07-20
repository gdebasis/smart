#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/index_exp.c,v 11.2 1993/02/03 16:50:52 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Top level procedure to form an experimental collection
 *1 index.top.exp_coll
 *2 index_exp_coll (unused1, unused2, inst)
 *3   char *unused1;
 *3   char *unused2;
 *3   int inst;

 *4 init_index_exp_coll (spec, unused)
 *5   "index_exp_coll.qrels_text_file"
 *5   "index_exp_coll.qrels_file"
 *5   "index_exp_coll.trace"
 *4 close_index_exp_coll (inst)

 *7 Form an experimental collection by
 *7      calling index_doc_coll to index the document collection
 *7      calling index_query_coll to index the query collection
 *7      calling text_rr_obj to create qrels_file from qrels_text_file.
 *7      (this gives the relevance judgments for the collection).
 *7 Returns UNDEF if error, else 1.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"

static char *qrels_text_file;
static char *qrels_file;

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index_exp_coll.trace")
    {"index_exp_coll.qrels_text_file", getspec_string,(char *) &qrels_text_file},
    {"index_exp_coll.qrels_file",      getspec_dbfile,(char *) &qrels_file},
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

SPEC *save_spec;

int
init_index_exp_coll (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }
    save_spec = spec;

    PRINT_TRACE (2, print_string,
                 "Trace: entering/leaving init_index_exp_coll");

    return (0);
}

int
index_exp_coll (unused1, unused2, inst)
char *unused1;
char *unused2;
int inst;
{
    int temp_inst;

    PRINT_TRACE (2, print_string, "Trace: entering index_exp_coll");

    /* Index the documents */
    if (UNDEF == (temp_inst = init_index_doc_coll (save_spec, (char *)NULL)) ||
        UNDEF == index_doc_coll ((char *)NULL, (char *)NULL, temp_inst) ||
        UNDEF == close_index_doc_coll (temp_inst)) {
        return (UNDEF);
    }

    /* Index the queries */
    if (UNDEF == (temp_inst = init_index_query_coll (save_spec,(char *)NULL))||
        UNDEF == index_query_coll ((char *) NULL, (char *) NULL, temp_inst) ||
        UNDEF == close_index_query_coll (temp_inst)) {
        return (UNDEF);
    }
    /* create the qrels file */
    if (UNDEF == (temp_inst = init_text_rr_obj (save_spec, (char *) NULL)) ||
        UNDEF == text_rr_obj (qrels_text_file, qrels_file, temp_inst) ||
        UNDEF == close_text_rr_obj (temp_inst)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving index_exp_coll");

    return (1);
}

int
close_index_exp_coll (inst)
int inst;
{
    PRINT_TRACE (2, print_string, 
                 "Trace: entering/leaving close_index_exp_coll");

    return (0);
}
