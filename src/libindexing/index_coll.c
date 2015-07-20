#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/index_coll.c,v 11.2 1993/02/03 16:50:51 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/
/********************   PROCEDURE DESCRIPTION   ************************
 *0 Top level procedure to add a set of docs to a collection
 *1 index.top.doc_coll
 *2 index_doc (unused1, unused2, inst)
 *3   char *unused1;
 *3   char *unused2;
 *3   int inst;

 *4 init_index_doc (spec, unused)
 *5   "index.preparse.doc.next_preparse"
 *5   "index.doc.next_vecid"
 *5   "index.doc.addtextloc"
 *5   "index.doc.index_pp"
 *5   "index.doc.store"
 *5   "index.trace"
 *4 close_index_doc (inst)

 *6 global_context set to indicate CTXT_INDEXING_DOC

 *7 Call each of the indicated procedures in turn to
 *7     preparse,
 *7     assign a vector id,
 *7     add location info about doc to textloc file,
 *7     Break doc into tokens
 *7     Parse tokens and determine concepts to represent doc
 *7     make a vector out of those concepts
 *7     weight the vector
 *7     store the vector (either directly as vector or in other forms)
 *7 Returns UNDEF if error, else 1.

 *8  Doc text is tokenized, stemmed, stopped.
 *8  doc and possibly inv file are updated.
 *8  textloc file updated.
 *8  dict updated.
 *8   Performs the equivalent of (from the old SMART system)
 *8  pre_parse
 *8  creat_index
 *8  enter_text
 *8  add_textloc
 *8   
 *8  Foreach document that the preparser finds.
 *8  Pass0 pass1 are now done together.
 *8  Pass0:  Pre-parser
 *8  No real input.  (Lower levels read input.preparse.doc.input_list_file.)
 *8  Output is document text in standard SMART pre_parser format:
 *8  the document is broken up into sections of text, each of which can
 *8  later be parsed separately.  Depending on the collection, the
 *8  preparser can ignore, re-format, or even add, text to the original
 *8  document.  This pass is responsible for first getting document in
 *8  ascii text format; eg by running latex.
 *8   
 *8  Pass1:
 *8    Bring entire document into memory, separating the section
 *8    headers from the text.  Construct an internal
 *8    sm_display object giving the start and end of each section
 *8    within memory.
 *8   
 *8  Pass 2
 *8    Take the section headers specified in Pass1 and update the textloc
 *8    file
 *8   
 *8  Pass3:
 *8    Go through the sm_display object section by section,
 *8    calling the appropriate parsing method on each section.
 *8    Constuct an internal sm_tokendoc relation giving 
 *8        token  ctype  did  para  sent
 *8    for the entire doc.
 *8   
 *8  Pass4
 *8    Perform phrasing, remove stopwords, perform stemming on
 *8    the sm_tokendoc relation, and convert to concept numbers.
 *8    Output is a sm_conloc relation (unsorted) with duplicates.
 *8   
 *8  Pass5
 *8    Construct a sm_vec object with tf weighted terms.
 *8   
 *8  Pass6
 *8    Pass this sm_vec to the appropriate weighting functions which
 *8    will modify the sm_vec object in place.
 *8    
 *8  Pass7
 *8    Write this vector out to the doc file, and inv_file if specified.
 *8   
 *8  Then return to Pass1 with the next document.

***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Top level procedure to add a set of queries to a collection
 *1 index.top.query_coll
 *2 index_query (unused1, unused2, inst)
 *3   char *unused1;
 *3   char *unused2;
 *3   int inst;

 *4 init_index_query (spec, unused)
 *5   "index.preparse.query.next_preparse"
 *5   "index.query.next_vecid"
 *5   "index.query.addtextloc"
 *5   "index.query.index_pp"
 *5   "index.query.store"
 *4 close_index_query (inst)

 *6 global_context set to indicate CTXT_INDEXING_QUERY

 *7 Call each of the indicated procedures in turn to
 *7     preparse,
 *7     assign a vector id,
 *7     add location info about query to textloc file,
 *7     Break query into tokens
 *7     Parse tokens and determine concepts to represent query
 *7     make a vector out of those concepts
 *7     weight the vector
 *7     store the vector (either directly as vector or in other forms)
 *7 Returns UNDEF if error, else 1.

 *8  Query text is tokenized, stemmed, stopped.
 *8  query and possibly inv file are updated.
 *8  textloc file updated.
 *8  dict updated.
 *8   Performs the equivalent of (from the old SMART system)
 *8  pre_parse
 *8  creat_index
 *8  enter_text
 *8  add_display
 *8   
 *8  Foreach query that the preparser finds.
 *8  Pass0 pass1 are now done together.
 *8  Pass0:  Pre-parser
 *8  No real input.  (Lower levels read input.preparse.query.input_list_file.)
 *8  Output is query text in standard SMART pre_parser format:
 *8  the query is broken up into sections of text, each of which can
 *8  later be parsed separately.  Depending on the collection, the
 *8  preparser can ignore, re-format, or even add, text to the original
 *8  query.  This pass is responsible for first getting query in
 *8  ascii text format; eg by running latex.
 *8   
 *8  Pass1:
 *8    Bring entire query into memory, separating the section
 *8    headers from the text.  Construct an internal
 *8    sm_display object giving the start and end of each section
 *8    within memory.
 *8   
 *8  Pass 2
 *8    Take the section headers specified in Pass1 and update the textloc
 *8    file
 *8   
 *8  Pass3:
 *8    Go through the sm_display object section by section,
 *8    calling the appropriate parsing method on each section.
 *8    Constuct an internal sm_tokendoc relation giving 
 *8        token  ctype  did  para  sent
 *8    for the entire doc.
 *8   
 *8  Pass4
 *8    Perform phrasing, remove stopwords, perform stemming on
 *8    the sm_tokendoc relation, and convert to concept numbers.
 *8    Output is a sm_conloc relation (unsorted) with duplicates.
 *8   
 *8  Pass5
 *8    Construct a sm_vec object with tf weighted terms.
 *8   
 *8  Pass6
 *8    Pass this sm_vec to the appropriate weighting functions which
 *8    will modify the sm_vec object in place.
 *8    
 *8  Pass7
 *8    Write this vector out to the doc file, and inv_file if specified.
 *8   
 *8  Then return to Pass1 with the next document (query)

***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "docindex.h"
#include "vector.h"
#include "trace.h"
#include "context.h"

static PROC_INST pp,             /* Preparse procedures */
    next_vecid,                  /* Determine next valid id to use */
    padd_textloc,                   /* Add_textloc output procedures */
    post_pp,			 /* Postprocessing of preparser output */
    index_pp,                    /* Convert preparsed text to vector */
    store;                       /* Write vector to output file */

static SPEC_PARAM doc_spec_args[] = {
    {"index.preparse.doc.next_preparse", getspec_func, (char *) &pp.ptab},
    {"index.doc.next_vecid",       getspec_func, (char *) &next_vecid.ptab},
    {"index.doc.addtextloc",       getspec_func, (char *) &padd_textloc.ptab},
    {"index.doc.post_pp",         getspec_func, (char *) &post_pp.ptab},
    {"index.doc.index_pp",         getspec_func, (char *) &index_pp.ptab},
    {"index.doc.store",            getspec_func, (char *) &store.ptab},
    TRACE_PARAM ("index.trace")
    };
static int num_doc_spec_args = sizeof (doc_spec_args) /
         sizeof (doc_spec_args[0]);

static SPEC_PARAM query_spec_args[] = {
    {"index.preparse.query.next_preparse", getspec_func, (char *) &pp.ptab},
    {"index.query.next_vecid",       getspec_func, (char *) &next_vecid.ptab},
    {"index.query.addtextloc",       getspec_func, (char *) &padd_textloc.ptab},
    {"index.query.post_pp",         getspec_func, (char *) &post_pp.ptab},
    {"index.query.index_pp",         getspec_func, (char *) &index_pp.ptab},
    {"index.query.store",            getspec_func, (char *) &store.ptab},
    TRACE_PARAM ("index.trace")
    };
static int num_query_spec_args = sizeof (query_spec_args) /
         sizeof (query_spec_args[0]);

static int init_index_coll(), index_coll(), close_index_coll();

int
init_index_query_coll (spec, unused)
SPEC *spec;
char *unused;
{
    CONTEXT old_context;

    /* Set context indicating that we are indexing a query.  Tells
       inferior procedures to use params appropriate for query instead of
       doc */
    old_context = get_context();
    set_context (CTXT_INDEXING_QUERY);
    if (UNDEF == lookup_spec (spec,
                              &query_spec_args[0],
                              num_query_spec_args)) {
        return (UNDEF);
    }

    if (UNDEF == init_index_coll (spec, "query."))
        return (UNDEF);

    set_context (old_context);

    return (0);
}

int
index_query_coll (param1, param2, inst)
char *param1, *param2;
int inst;
{
    return (index_coll (param1, param2, inst));
}

int
close_index_query_coll (inst)
int inst;
{
    return (close_index_coll (inst));
}

int
init_reindex_doc_coll (spec, unused)
SPEC *spec;
char *unused;
{
    CONTEXT old_context;

    /* Set context indicating that we are reindexing a document.  Tells
       inferior procedures to use params appropriate for doc instead of
       query */
    old_context = get_context();
    set_context (CTXT_DOC);
    if (UNDEF == lookup_spec (spec,
                              &doc_spec_args[0],
                              num_doc_spec_args)) {
        return (UNDEF);
    }

    if (UNDEF == init_index_coll (spec, "doc."))
        return (UNDEF);

    set_context (old_context);

    return (0);
}

int
init_index_doc_coll (spec, unused)
SPEC *spec;
char *unused;
{
    CONTEXT old_context;

    /* Set context indicating that we are indexing a document.  Tells
       inferior procedures to use params appropriate for doc instead of
       query */
    old_context = get_context();
    set_context (CTXT_INDEXING_DOC);
    if (UNDEF == lookup_spec (spec,
                              &doc_spec_args[0],
                              num_doc_spec_args)) {
        return (UNDEF);
    }

    if (UNDEF == init_index_coll (spec, "doc."))
        return (UNDEF);

    set_context (old_context);

    return (0);
}

int
index_doc_coll (param1, param2, inst)
char *param1, *param2;
int inst;
{
    return (index_coll (param1, param2, inst));
}

int
close_index_doc_coll (inst)
int inst;
{
    return (close_index_coll (inst));
}

static int
init_index_coll (spec, prefix)
SPEC *spec;
char *prefix;
{
    PRINT_TRACE (2, print_string, "Trace: entering init_index_coll");

    /* Call all initialization procedures */
    if (UNDEF == (pp.inst = pp.ptab->init_proc (spec, prefix)) ||
        UNDEF == (next_vecid.inst = 
                  next_vecid.ptab->init_proc (spec, prefix)) ||
        UNDEF == (padd_textloc.inst = 
                  padd_textloc.ptab->init_proc (spec, prefix)) ||
        UNDEF == (post_pp.inst =
                  post_pp.ptab->init_proc (spec, prefix)) ||
        UNDEF == (index_pp.inst =
                  index_pp.ptab->init_proc (spec, prefix)) ||
        UNDEF == (store.inst =
                  store.ptab->init_proc (spec, prefix))) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_index_coll");
    return (0);
}

static int
index_coll (unused1, unused2, inst)
char *unused1;
char *unused2;
int inst;
{
    int status;
    SM_INDEX_TEXTDOC pp_vec;
    SM_VECTOR vec;

    PRINT_TRACE (2, print_string, "Trace: entering index_coll");

    /* Get each document in turn */
    while (1) {
        /* Get next doc id and store in pp_vec */
        if (UNDEF == next_vecid.ptab->proc ((char *)NULL, 
                                            &pp_vec.id_num,
                                            next_vecid.inst))
            return (UNDEF);

        /* Check to see if finished, and if tracing is desired. */
        if (pp_vec.id_num.id > global_end)
            break;
        SET_TRACE (pp_vec.id_num.id);

        /* Get next document (if it exists) */
        status =  pp.ptab->proc ((char *) NULL, &pp_vec, pp.inst);
        if (status == UNDEF) {
            return (UNDEF);
        }
        if (status == 0) {
            /* End of input */
            break;
        }

        if (UNDEF == padd_textloc.ptab->proc (&pp_vec,
                                              (char *)NULL,
                                              padd_textloc.inst) ||
	    UNDEF == post_pp.ptab->proc (&pp_vec, (SM_INDEX_TEXTDOC *)NULL, 
					 post_pp.inst) ||
            UNDEF == index_pp.ptab->proc (&pp_vec, &vec, index_pp.inst) ||
            UNDEF == store.ptab->proc (&vec, (char *)NULL, store.inst)) {
            return (UNDEF);
        }
    }

    PRINT_TRACE (2, print_string, "Trace: leaving index_coll");
    return (1);
}


static int
close_index_coll (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_index_coll");

    if (UNDEF == pp.ptab->close_proc (pp.inst)||
        UNDEF == next_vecid.ptab->close_proc(next_vecid.inst) ||
        UNDEF == padd_textloc.ptab->close_proc(padd_textloc.inst) ||
        UNDEF == post_pp.ptab->close_proc(post_pp.inst) ||
        UNDEF == index_pp.ptab->close_proc(index_pp.inst) ||
        UNDEF == store.ptab->close_proc(store.inst)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_index_coll");
    return (0);
}
