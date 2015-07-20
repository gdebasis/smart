#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libproc/proc_index.c,v 11.2 1993/02/03 16:53:42 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "proc.h"

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table giving top level procedures to index a collection
 *1 index.top
 *2 * (unused1, unused2, inst)
 *3    char *unused1;
 *3    char *unused2;
 *3    int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures are top-level procedures which index an entire vector
 *7 object.  As top-level procedures, they are responsible for setting
 *7 trace conditions, and for determining other execution time constraints,
 *7 such as when execution should stop (eg, if global_end is exceeded).
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are "doc_coll", "query_coll", "exp_coll",
 *8 "add_parts", "parts_coll"
***********************************************************************/

extern int init_index_doc_coll(), index_doc_coll(), close_index_doc_coll();
extern int init_reindex_doc_coll();
/* extern int init_index_exp_coll(), index_exp_coll(), close_index_exp_coll(); */
extern int init_index_query_coll(), index_query_coll(),
    close_index_query_coll();
static PROC_TAB topproc_index[] = {
    {"doc_coll",  init_index_doc_coll,  index_doc_coll,  close_index_doc_coll},
    {"reindex_doc",init_reindex_doc_coll,index_doc_coll, close_index_doc_coll},
    {"query_coll",init_index_query_coll,index_query_coll,close_index_query_coll},
    /*     {"exp_coll",  init_index_exp_coll,  index_exp_coll,  close_index_exp_coll}, */
};
static int num_topproc_index =
    sizeof (topproc_index) / sizeof (topproc_index[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table giving procedures to index a single vector from text
 *1 index.vec_index
 *2 * (textdisp, vec, inst)
 *3   SM_TEXTLOC *textdisp;
 *3   VEC *vec;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take the location of the vector, given by textdisp,
 *7 and index that vector, result in vec.
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are "doc", "query"
***********************************************************************/
extern int init_index_doc(), index_doc(), close_index_doc();
extern int init_index_query(), index_query(), close_index_query();
static PROC_TAB proc_vec_index[] = {
    {"doc",      init_index_doc,    index_doc,   close_index_doc},
    {"query",    init_index_query,  index_query, close_index_query},
};
static int num_proc_vec_index =
    sizeof (proc_vec_index) / sizeof (proc_vec_index[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures indexing a single vector from preparsed text.
 *1 index.index_pp
 *2 * (pp_vec, vec, inst)
 *3   SM_INDEX_TEXTDOC *pp_vec;
 *3   VEC *vec;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take the preparsed text of a vector, given by pp_vec
 *7 and index that vector, result in vec.
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are "index_pp", "index_pp_wv", "index_pp_parts"
***********************************************************************/
extern int init_index_pp(), index_pp(), close_index_pp();
/* extern int init_index_pp_wv(), index_pp_wv(), close_index_pp_wv(); */
static PROC_TAB proc_index_pp[] = {
    {"index_pp",      init_index_pp,    index_pp,   close_index_pp},
    /*     {"index_pp_wv",      init_index_pp_wv,    index_pp_wv,   close_index_pp_wv}, */
};
static int num_proc_index_pp =
    sizeof (proc_index_pp) / sizeof (proc_index_pp[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures fetching named text
 *1 index.preparse.get_named_text
 *2 * (text_id, text, inst)
 *3   EID *text_id;
 *3   SM_INDEX_TEXTDOC *text;
 *3   int inst;
 *4 init_* (spec, query_or_doc_prefix)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures fetch the text corresponding to the input document/
 *7 query id.  They are responsible for getting the text into text
 *7 format, e.g. by running latex, if such a step is necessary.
 *7 Return UNDEF if error, 1 if a non-empty object was fetched, and 0
 *7 if no object remained to be fetched.
 *8 Current possibilities are "standard".
***********************************************************************/
extern int init_get_named_text(), get_named_text(), close_get_named_text();
static PROC_TAB proc_get_named[] = {
    {"standard", init_get_named_text, get_named_text, close_get_named_text},
    /* get_named_text_dm via document manager */
    };
static int num_proc_get_named = sizeof(proc_get_named)
				/ sizeof(proc_get_named[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures fetching next text
 *1 index.preparse.get_next_text
 *2 * (sm_buf, text, inst)
 *3   SM_BUF *sm_buf;
 *3   SM_INDEX_TEXTDOC *text;
 *3   int inst;
 *4 init_* (spec, query_or_doc_prefix)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures fetch the next text from either a list of input files,
 *7 or possibly directly from the user.
 *7 They are responsible for getting the text into text format, e.g.
 *7 by running latex, if such a step is necessary.
 *7 Return UNDEF if error, 1 if a non-empty object was fetched, and 0
 *7 if no object remained to be fetched.
 *8 Current possibilities are "standard", "user_cat", "user_edit"
***********************************************************************/
extern int init_get_next_text(), get_next_text(), close_get_next_text();
extern int init_get_qtext_cat(), get_qtext_cat(), close_get_qtext_cat();
extern int init_get_qtext_edit(), get_qtext_edit(), close_get_qtext_edit();

static PROC_TAB proc_get_next[] = {
    {"standard", init_get_next_text, get_next_text, close_get_next_text},
    {"user_cat", init_get_qtext_cat, get_qtext_cat, close_get_qtext_cat},
    {"user_edit", init_get_qtext_edit, get_qtext_edit, close_get_qtext_edit},
    /* get_next_text_dm via document manager */
    };
static int num_proc_get_next = sizeof(proc_get_next)
				/ sizeof(proc_get_next[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures preparsing already fetched text
 *1 index.preparse.preparser
 *2 * (sm_buf, sm_disp, inst)
 *3   SM_BUF *sm_buf;
 *3   SM_DISPLAY *sm_disp;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These are collection-dependant procedures whose implementation will vary
 *7 widely with collection.
 *7 They preparse the text in the input SM_BUF, leaving conclusions in the
 *7 output SM_DISPLAY in standard SMART pre_parser format:
 *7 the document is broken up into sections of text, each of which can
 *7 later be parsed separately.  Depending on the collection, the
 *7 preparser can ignore, re-format, or even add, text to the original
 *7 document.
 *7 Return UNDEF if error, 1 if a non-empty object was preparsed, and 0
 *7 if no object was found to preparse.
 *8 Current possibilities are "line".
***********************************************************************/
extern int init_pp_gen_line(), pp_gen_line(), close_pp_gen_line();
static PROC_TAB proc_preparser[] = {
    {"line", init_pp_gen_line, pp_gen_line, close_pp_gen_line},
    /* SGML */
    };
static int num_proc_preparser = sizeof(proc_preparser)
				/ sizeof(proc_preparser[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures fetching and preparsing named text
 *1 index.preparse.named_text
 *2 * (doc_id, output_doc, inst)
 *3   EID *doc_id
 *3   SM_INDEX_TEXTDOC *output_doc;
 *3   int inst;
 *4 init_* (spec, query_or_doc_prefix)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take document/query text ids, fetch the corresponding
 *7 text, and preparse it.  Output is text in standard SMART pre_parser format.
 *7 Return UNDEF if error,  1 if a non-empty object was preparsed, and 0
 *7 if no object was preparsed.
 *8 Current possibilities are "generic".
***********************************************************************/
extern int init_pp_named_text(), pp_named_text(), close_pp_named_text();
static PROC_TAB proc_named_text[] = {
    {"generic", init_pp_named_text, pp_named_text, close_pp_named_text},
    };
static int num_proc_named_text = sizeof(proc_named_text)
				/ sizeof(proc_named_text[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures fetching and preparsing next text
 *1 index.preparse.next_text
 *2 * (unused, output_doc, inst)
 *3   char *unused;
 *3   SM_INDEX_TEXTDOC *output_doc;
 *3   int inst;
 *4 init_* (spec, query_or_doc_prefix)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures fetch the next text from a list of input files and
 *7 preparse it.  Output is text in standard SMART pre_parser format.
 *7 Return UNDEF if error,  1 if a non-empty object was preparsed, and 0
 *7 if no object remained to be preparsed.
 *8 Current possibilities are "generic".
***********************************************************************/
extern int init_pp_next_text(), pp_next_text(), close_pp_next_text();
static PROC_TAB proc_next_text[] = {
    {"generic", init_pp_next_text, pp_next_text, close_pp_next_text},
    };
static int num_proc_next_text = sizeof(proc_next_text)
				/ sizeof(proc_next_text[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: table of tables for preparsing procedures
 *1 index.preparse
 *7 Procedure table mapping a string procedure table name to a stem table
 *7 which maps names to procedures.
 *8 Current possibilities are "get_named_text", "get_next_text", "preparser",
 *8 "named_text", and "next_text".
***********************************************************************/
static TAB_PROC_TAB proc_pp[] = {
    { "get_named_text", TPT_PROC, NULL, proc_get_named,  &num_proc_get_named },
    { "get_next_text",  TPT_PROC, NULL, proc_get_next,   &num_proc_get_next },
    { "preparser",      TPT_PROC, NULL, proc_preparser,  &num_proc_preparser },
    { "named_text",     TPT_PROC, NULL, proc_named_text, &num_proc_named_text },
    { "next_text",      TPT_PROC, NULL, proc_next_text,  &num_proc_next_text },
    };
static int num_proc_pp = sizeof(proc_pp) / sizeof(proc_pp[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures returning next available id
 *1 index.next_vecid
 *2 * (unused, id, inst)
 *3   char *unused;
 *3   long *id;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures pass a result back in id, giving the next available
 *7 vector id for this collection of vectors.
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are "next_vecid_1", "next_vecid", "empty"
***********************************************************************/
extern int init_next_vecid_1(), next_vecid_1(), close_next_vecid_1();
extern int init_next_vecid(), next_vecid(), close_next_vecid();
static PROC_TAB proc_next_vecid[] = {
    {"next_vecid_1",	init_next_vecid_1,next_vecid_1,	close_next_vecid_1},
    {"next_vecid",	init_next_vecid,next_vecid,	close_next_vecid},
    {"empty",		INIT_EMPTY,	EMPTY,		CLOSE_EMPTY},
    };
static int num_proc_next_vecid = sizeof (proc_next_vecid) / sizeof (proc_next_vecid[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures storing text location of document
 *1 index.textloc
 *2 * (doc, unused, inst)
 *3   SM_INDEX_TEXTDOC *doc;
 *3   char *unused;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures store location information of a document (pathname
 *7 plus byte offset of start and end of document) as well as the title of
 *7 the document.
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are "add_textloc", "empty"
***********************************************************************/
extern int init_add_textloc(), add_textloc(), close_add_textloc();
static PROC_TAB proc_addtextloc[] = {
    {"add_textloc",		init_add_textloc,  add_textloc,	close_add_textloc},
    {"empty",		INIT_EMPTY,	EMPTY,		CLOSE_EMPTY},
    };
static int num_proc_addtextloc =
    sizeof (proc_addtextloc) / sizeof (proc_addtextloc[0]);



/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to break text of a document section into tokens
 *1 index.token_sect
 *2 * (pp_doc, t_doc, inst)
 *3   SM_BUF *pp_doc;
 *3   SM_TOKENSECT *t_doc;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures break a preparsed text down into tokens, recognizing
 *7 and returning various classes of tokens.  The text corresponding to a
 *7 single section is done at a time.
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are "token_sect", "token_sgml", "token_no"
***********************************************************************/
extern int init_token_sect(), token_sect(), close_token_sect();
extern int init_token_sgml(), token_sgml(), close_token_sgml();
extern int init_token_sect_no(), token_sect_no(), close_token_sect_no();
static PROC_TAB proc_token_sect[] = {
    {"token_sect",     init_token_sect,	token_sect,	close_token_sect},
    {"token_sgml",     init_token_sgml,	token_sgml,	close_token_sgml},
    {"token_sect_no",  init_token_sect_no,token_sect_no,	close_token_sect_no},
    };
static int num_proc_token_sect =
    sizeof (proc_token_sect) / sizeof (proc_token_sect[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to help parse a section from tokenized document
 *1 index.parse_sect
 *2 * (token, consect, inst)
 *3   SM_TOKENSECT *token;
 *3   SM_CONSECT *consect;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take a list of tokens for a section and return the
 *7 concept number, ctype and the position in the list of tokens 
 *7 that this token was found.
 *7 Return UNDEF if error, 0 if no valid tokens found in this section,
 *7 and 1 upon successful finding of tokens.
 *8 Normally called from a procedure in index.parse.
 *8 Current possibilities are "full", "name", "asis", "none", "adj", "full_all"
***********************************************************************/
extern int init_full_parse(), full_parse(), close_full_parse();
/* extern int init_full_all_parse(), full_all_parse(), close_full_all_parse(); */
/* extern int init_adj_parse(), adj_parse(), close_adj_parse(); */
extern int init_phrase_parse(), phrase_parse(), close_phrase_parse();
extern int init_name_parse(), name_parse(), close_name_parse();
/* extern int init_asis_parse(), asis_parse(), close_asis_parse(); */
extern int  no_parse();
static PROC_TAB proc_parse_sect[] = {
    {"full",		init_full_parse, full_parse,	close_full_parse},
    /*     {"full_all",	init_full_all_parse,     full_all_parse,close_full_all_parse}, */
    /*     {"adj",		init_adj_parse,  adj_parse,	close_adj_parse}, */
    {"phrase",		init_phrase_parse,  phrase_parse,close_phrase_parse},
    {"name",		init_name_parse, name_parse,	close_name_parse},
    /*     {"asis",		init_asis_parse, asis_parse,	close_asis_parse}, */
    {"none",	        INIT_EMPTY,      no_parse,	CLOSE_EMPTY},
    };
static int num_proc_parse_sect = sizeof (proc_parse_sect) / sizeof (proc_parse_sect[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to determine if token is a stopword
 *1 index.stop
 *2 * (word, stopword_flag, inst)
 *3   char *word;
 *3   int *stopword_flag;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take a single token, and determine if this is a common
 *7 word, presumably to be ignored if it is.  Stopword_flag is set to 1
 *7 if a common word, set to 0 otherwise.
 *7 Return UNDEF if error, 0 otherwise
 *8 Current possibilities are "stop_dict", "none"
***********************************************************************/
extern int init_stop_dict(), stop_dict(), close_stop_dict();
extern int stop_no();
static PROC_TAB proc_stop[] = {
    {"stop_dict",	init_stop_dict,	stop_dict,	close_stop_dict},
    {"none",		INIT_EMPTY,	stop_no,	CLOSE_EMPTY},
    };
static int num_proc_stop = sizeof (proc_stop) / sizeof (proc_stop[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to stem a concept token
 *1 index.stem
 *2 * (word, output_word, inst)
 *3   char *word;
 *3   char **output_word;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take a single token, and return a pointer to the start
 *7 of the corresponding stemmed token (appropriate suffixes removed).
 *7 Return UNDEF if error, 1 otherwise
 *8 Current possibilities are "triestem", "remove_s", "none", "spanish"
***********************************************************************/
extern int init_triestem(), triestem(), close_triestem();
extern int init_remove_s(), remove_s(), close_remove_s();
extern int init_stem_spanish(), stem_spanish(), close_stem_spanish();
extern int init_stem_french(), stem_french(), close_stem_french();
extern int init_stem_phrase(), stem_phrase(), close_stem_phrase();
extern int stem_no();
static PROC_TAB proc_stem[] = {
    {"triestem",	init_triestem,	triestem,	close_triestem},
    {"remove_s",	init_remove_s,	remove_s,	close_remove_s},
    {"spanish",		init_stem_spanish,stem_spanish,	close_stem_spanish},
    {"french",		init_stem_french,stem_french,	close_stem_french},
    {"stem_phrase",     init_stem_phrase, stem_phrase,    close_stem_phrase},
    {"none",		INIT_EMPTY,	stem_no,	CLOSE_EMPTY},
    };
static int num_proc_stem = sizeof (proc_stem) / sizeof (proc_stem[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to take a token and return a concept number
 *1 index.token_to_con
 *2 * (word, con, inst)
 *3   char *word;
 *3   long *con;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take a single token, and find the concept number to
 *7 be used to represent that token.
 *7 Return UNDEF if error, 1 otherwise
 *8 Current possibilities are "dict", "dict_noss", "dict_cont", "empty"
***********************************************************************/
extern int init_tokcon_dict(), tokcon_dict(), close_tokcon_dict();
extern int init_tokcon_dict_cont(),tokcon_dict_cont(),close_tokcon_dict_cont();
extern int init_tokcon_dict_noss(),tokcon_dict_noss(),close_tokcon_dict_noss();
static PROC_TAB proc_tocon[] = {
    {"dict",	init_tokcon_dict,	tokcon_dict,	close_tokcon_dict},
    {"dict_cont",init_tokcon_dict_cont, tokcon_dict_cont,close_tokcon_dict_cont},
    {"dict_noss",init_tokcon_dict_noss, tokcon_dict_noss,close_tokcon_dict_noss},
    {"empty",		INIT_EMPTY,	EMPTY,		CLOSE_EMPTY},
    };
static int num_proc_tocon = sizeof (proc_tocon) / sizeof (proc_tocon[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to take concept number, and find the token
 *1 index.con_to_token
 *2 * (con, word, inst)
 *3   long *con;
 *3   char **word;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take a single concept number and find the token string
 *7 that the concept number represents.
 *7 Return UNDEF if error, 1 otherwise
 *8 Current possibilities are "contok_dict", "empty"
***********************************************************************/
/* NOTE. all procedures below also entered in convert.low proc table */
extern int init_contok_dict(), contok_dict(), close_contok_dict();
static PROC_TAB proc_fromcon[] = {
    {"contok_dict",	init_contok_dict,contok_dict,	close_contok_dict},
    {"empty",		INIT_EMPTY,	EMPTY,		CLOSE_EMPTY},
    };
static int num_proc_fromcon = sizeof (proc_fromcon) / sizeof (proc_fromcon[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to create a vector from a list of concepts
 *1 index.makevec
 *2 * (p_doc, vec, inst)
 *3   SM_CONDOC *p_doc;
 *3   SM_VECTOR *vec;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take a list of concepts (eg from index.parse) and create
 *7 a vector from them, where the vector contains the appropriate concept
 *7 numbers, and the weight of each concept is the number of times the
 *7 concept occurs in the list.
 *7 Return UNDEF if error, 1 otherwise
 *8 Current possibilities are "makevec"
***********************************************************************/
extern int init_makevec(), makevec(), close_makevec();
extern int init_makeposvec(), makeposvec(), close_makeposvec();
extern int init_make_avgposvec(),	make_avgposvec(),	close_make_avgposvec();

static PROC_TAB proc_makevec[] = {
    {"makevec",		init_makevec,	makevec,	close_makevec},
    {"makeposvec",		init_makeposvec,	makeposvec,	close_makeposvec},
    {"makeavgposvec",		init_make_avgposvec,	make_avgposvec,	close_make_avgposvec}
};
static int num_proc_makevec = sizeof (proc_makevec) / sizeof (proc_makevec[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to expand a vector.
 *1 index.expand
 *2 * (tf_vec, exp_vec, inst)
 *3   SM_VECTOR *tf_vec;
 *3   SM_VECTOR *exp_vec;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take an indexed vector (tf weights) and create a new
 *7 expanded vector by incorporating additional information.
 *7 Return UNDEF if error, 1 otherwise
 *8 Current possibilities are "none"
***********************************************************************/
extern int init_expand_no(), expand_no(), close_expand_no();
static PROC_TAB proc_expand[] = {
    {"none",		init_expand_no,	expand_no,	close_expand_no},
    };
static int num_proc_expand = sizeof (proc_expand) / sizeof (proc_expand[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to store an indexed vector in collection
 *1 index.store
 *2 * (vec, ignored, inst)
 *3   SM_VECTOR *vec;
 *3   char *ignored;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take a vector and store it in collection database
 *7 files.
 *7 Return UNDEF if error, 1 otherwise
 *8 Current possibilities are "store_vec", "store_vec_aux", 
 *8 "store_aux", "empty"
***********************************************************************/
extern int init_store_vec(), store_vec(), close_store_vec();
extern int init_store_aux(), store_aux(), close_store_aux();
extern int init_store_vec_aux(), store_vec_aux(), close_store_vec_aux();
extern int init_store_posvec_aux(), store_posvec_aux(), close_store_posvec_aux();
static PROC_TAB proc_store[] = {
    {"store_vec",	init_store_vec,	store_vec,	close_store_vec},
    {"store_vec_aux",	init_store_vec_aux,store_vec_aux,close_store_vec_aux},
    {"store_posvec_aux",	init_store_posvec_aux, store_posvec_aux, close_store_posvec_aux},
    {"store_aux",	init_store_aux,	store_aux,	close_store_aux},
    {"empty",		EMPTY,		EMPTY,		EMPTY},
    };
static int num_proc_store = sizeof (proc_store) / sizeof (proc_store[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: table of other hierarchy tables for indexing procedures
 *1 index
 *7 Procedure table mapping a string procedure table to either another
 *7 table of hierarchy tables, or to a stem table which maps 
 *7 names to procedures
 *8 Current possibilities are "index", "vec_index", "index_pp",
 *8 "preparse", "parts_preparse", "next_vecid", "addtextloc", "token",
 *8 "parse", "parse_sect", "stop", "stem", "tocon", "fromcon", "makevec",
 *8 "expand", "store"
***********************************************************************/
TAB_PROC_TAB proc_index[] = {
    {"top",            TPT_PROC,       NULL,   topproc_index,
                        &num_topproc_index},
    {"vec_index",        TPT_PROC,       NULL,   proc_vec_index,
                        &num_proc_vec_index},
    {"index_pp",		TPT_PROC,	NULL,	proc_index_pp,	
                        &num_proc_index_pp},
    {"preparse",		TPT_TAB,	proc_pp,	NULL,	
                        &num_proc_pp},
    {"next_vecid",	TPT_PROC,	NULL,	proc_next_vecid,	
                        &num_proc_next_vecid},
    {"addtextloc",	TPT_PROC,	NULL,	proc_addtextloc, 
			&num_proc_addtextloc},
    {"token_sect",	TPT_PROC,	NULL,	proc_token_sect,	
			&num_proc_token_sect},
    {"parse_sect",	TPT_PROC,	NULL,	proc_parse_sect,	
			&num_proc_parse_sect},
    {"stop",		TPT_PROC,	NULL, 	proc_stop,	
			&num_proc_stop},
    {"stem",		TPT_PROC,	NULL, 	proc_stem,	
			&num_proc_stem},
    {"token_to_con",	TPT_PROC,	NULL, 	proc_tocon,	
			&num_proc_tocon},
    {"con_to_token",	TPT_PROC,	NULL, 	proc_fromcon,	
			&num_proc_fromcon},
    {"makevec",		TPT_PROC,	NULL, 	proc_makevec,	
			&num_proc_makevec},
    {"expand",		TPT_PROC,	NULL, 	proc_expand,	
			&num_proc_expand},
    {"store",		TPT_PROC,	NULL, 	proc_store,	
			&num_proc_store},
    };

int num_proc_index = sizeof (proc_index) / sizeof (proc_index[0]);
