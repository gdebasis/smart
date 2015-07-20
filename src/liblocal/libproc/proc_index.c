#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libproc/proc_index.c,v 11.2 1993/02/03 16:52:23 smart Exp $";
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
 *0 Local hierarchy table: procedures preparsing a text object
 *1 local.index.preparse
 *2 * (input_doc, output_doc, inst)
 *3   SM_TEXTDISPLAY *input_doc;
 *3   SM_INDEX_TEXTDOC *output_doc;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take document/query text locations, either given for 
 *7 a single vector by input_doc or, if input_doc is NULL, by other parameters,
 *7 and preparse that document.
 *7 This is a collection dependant procedure whose implementation will vary
 *7 widely with collection.
 *7 Output is document text in standard SMART pre_parser format:
 *7 the document is broken up into sections of text, each of which can
 *7 later be parsed separately.  Depending on the collection, the
 *7 preparser can ignore, re-format, or even add, text to the original
 *7 document.  This pass is responsible for first getting document in
 *7 ascii text format; eg by running latex.
 *7 Return UNDEF if error,  1 if a non-empty object was preparsed, and 0
 *7 if no object remains to be preparsed.
 *8 Current possibilities are "trec", "trec_query",
***********************************************************************/
extern int init_pp_next_doc(), pp_next_doc(), close_pp_next_doc();
extern int init_pp_named_doc(), pp_named_doc(), close_pp_named_doc();
extern int init_pp_xml(), pp_xml(), close_pp_xml();
static PROC_TAB proc_preparse[] = {
    {"empty",	INIT_EMPTY,	EMPTY,		CLOSE_EMPTY},
    {"xml",   init_pp_xml,    pp_xml,         close_pp_xml},
    {"next_doc", init_pp_next_doc, pp_next_doc, close_pp_next_doc},
    {"named_doc", init_pp_named_doc, pp_named_doc, close_pp_named_doc},
};
static int num_proc_preparse = sizeof (proc_preparse) / sizeof (proc_preparse[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local hierarchy table: procedures preparsing text object into "parts" 
 *1 local.index.parts_preparse
 *2 * (input_doc, output_doc, inst)
 *3   SM_TEXTDISPLAY *input_doc;
 *3   SM_INDEX_TEXTDOC *output_doc;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take document/query text locations, given for 
 *7 a single vector by input_doc and preparse that document.
 *7 Output is document text in standard SMART pre_parser format.
 *7 the document is broken up into sections of text, each of which can
 *7 later be parsed separately.  Sections are determined first by the
 *7 normal collection dependent algorithm, and then further broken down into
 *7 syntactic parts; for example, sentences, paragraphs, or collection
 *7 sections.  Each syntactic part is considered its own section, with the
 *7 same section id as in the original collection dependent algorithm.
 *7 Return UNDEF if error,  1 if a non-empty object was preparsed, and 0
 *7 if no object remains to be preparsed.
 *8 Current possibilities are  "window"
***********************************************************************/
extern int init_pp_window(), pp_window(), close_pp_window();
extern int init_pp_pp_parts(), pp_pp_parts(), close_pp_pp_parts();
static PROC_TAB proc_parts_preparse[] = {
    {"window",           init_pp_window, pp_window,      close_pp_window},
    {"parts",           init_pp_pp_parts, pp_pp_parts,      close_pp_pp_parts},
    };
static int num_proc_parts_preparse = sizeof (proc_parts_preparse) / sizeof (proc_parts_preparse[0]);

extern int init_post_pp_initial(), post_pp_initial(), close_post_pp_initial();
static PROC_TAB proc_post_preparse[] = {
    {"initial", init_post_pp_initial, post_pp_initial, close_post_pp_initial},
    {"empty", INIT_EMPTY, EMPTY, CLOSE_EMPTY},
    };
static int num_proc_post_preparse = sizeof (proc_post_preparse) / sizeof (proc_post_preparse[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local Hierarchy table: procedures to break text of a section into tokens
 *1 local.index.token_sect
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
 *8 Current possibilities are "token_sect"
***********************************************************************/
extern int init_token_trec(), token_trec(), close_token_trec();
extern int init_token_utf8(), token_utf8(), close_token_utf8();
extern int init_token_beng(), token_beng(), close_token_beng();
extern int init_token_hi(), token_hi(), close_token_hi();
extern int init_token_chinese(), token_chinese(), close_token_chinese();
extern int init_token_fire_docnums(), token_fire_docnums(), close_token_fire_docnums();
static PROC_TAB proc_token_sect[] = {
    {"token_trec",     init_token_trec, token_trec, close_token_trec},
    {"token_utf8",     init_token_utf8, token_utf8, close_token_utf8},
    {"token_beng",     init_token_beng, token_beng, close_token_beng},
    {"token_hi",     init_token_hi, token_hi, close_token_hi},
    {"token_chinese",  init_token_chinese, token_chinese, close_token_chinese},
    {"token_fire_docnums",  init_token_fire_docnums, token_fire_docnums, close_token_fire_docnums},
};
static int num_proc_token_sect =
    sizeof (proc_token_sect) / sizeof (proc_token_sect[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local hierarchy table: procedures to help parse a section from tokenized document
 *1 local.index.parse_sect
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
 *8 Current possibilities are "chinese".
***********************************************************************/
extern int init_chinese_parse(), chinese_parse(), close_chinese_parse();
extern int init_parse_utf8(), parse_utf8(), close_parse_utf8();
static PROC_TAB proc_parse_sect[] = {
    {"chinese",             init_chinese_parse,  chinese_parse,     close_chinese_parse},
    {"utf8",             init_parse_utf8,  parse_utf8,     close_parse_utf8},
    };
static int num_proc_parse_sect = sizeof (proc_parse_sect) / sizeof (proc_parse_sect[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to stem a concept token
 *1 local.index.stem
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
 *8 Current possibilities are "bangla"
***********************************************************************/
extern int init_stem_ext(), stem_ext(), close_stem_ext();
extern int init_stem_ext_hash(), stem_ext_hash(), close_stem_ext_hash();
extern int init_stem_bangla(), stem_bangla(), close_stem_bangla();
extern int init_bn_triestem(), bn_triestem(), close_bn_triestem();
extern int init_hn_triestem(), hn_triestem(), close_hn_triestem();
static PROC_TAB proc_stem[] = {
    {"stem_ext",		init_stem_ext,stem_ext,	close_stem_ext},
    {"stem_ext_hash",		init_stem_ext_hash, stem_ext_hash,	close_stem_ext_hash},
    {"bangla",		init_stem_bangla,stem_bangla,	close_stem_bangla},
    {"bn_triestem",	init_bn_triestem, bn_triestem, close_bn_triestem},
    {"hn_triestem",	init_hn_triestem, hn_triestem, close_hn_triestem},
};
static int num_proc_stem = sizeof (proc_stem) / sizeof (proc_stem[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local hierarchy table: procedures to take token and return a concept number
 *1 local.index.token_to_con
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
 *8 Current possibilities are "empty"
***********************************************************************/
extern int init_tokcon_dict_niss(), tokcon_dict_niss(), close_tokcon_dict_niss()
;
static PROC_TAB proc_tocon[] = {
    {"dict_niss",init_tokcon_dict_niss, tokcon_dict_niss,close_tokcon_dict_niss},
    {"empty",            INIT_EMPTY,     EMPTY,          CLOSE_EMPTY}
    };
static int num_proc_tocon = sizeof (proc_tocon) / sizeof (proc_tocon[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: table of other hierarchy tables for indexing procedures
 *1 local.index
 *7 Procedure table mapping a string procedure table to either another
 *7 table of hierarchy tables, or to a stem table which maps 
 *7 names to procedures
 *8 Current possibilities are "preparse"
***********************************************************************/
TAB_PROC_TAB lproc_index[] = {
    {"parse_sect",	TPT_PROC,	NULL,	proc_parse_sect, 
                        &num_proc_parse_sect},
    {"token_sect",	TPT_PROC,	NULL,	proc_token_sect, 
                        &num_proc_token_sect},
    {"token_to_con",	TPT_PROC,	NULL,	proc_tocon,     
                        &num_proc_tocon},
    {"preparse",	TPT_PROC,	NULL,	proc_preparse,	
                        &num_proc_preparse},
    {"parts_preparse",	TPT_PROC,	NULL,	proc_parts_preparse,
                        &num_proc_parts_preparse},
    {"post_preparse",	TPT_PROC,	NULL,	proc_post_preparse,
                        &num_proc_post_preparse},
    {"stem",		TPT_PROC,	NULL, 	proc_stem,	
			&num_proc_stem},
    };

int num_lproc_index = sizeof (lproc_index) / sizeof (lproc_index[0]);
