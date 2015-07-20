#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/ph_text_pp_named.c,v 11.2 1993/02/03 16:54:00 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/* Similar to and adapted from libprint/ph_text_name.c.
 * Prints the text referred to by name (eid) in the input format required by 
 * the Mitre POS tagger (.mi files; see ~nlp/src/postagger/README).
 * Sentences are demarcated by <s>...</s> tags; punctuation marks are 
 * surrounded by white space. 
 * Also, add EOD_MARKER string at end of document. Since individual documents
 * may be concatenated into one long pseudo-document and passed through
 * the tagger as a whole, we need to be able to tell document boundaries
 * afterwards.
 */

#include <fcntl.h>
#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "docindex.h"
#include "trace.h"
#include "io.h"
#include "buf.h"
#include "inst.h"
#include "docdesc.h"

static char *prefix;
static PROC_INST proc;
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    { &prefix, "named_preparse",	getspec_func,	(char *)&proc.ptab},
    };
static int num_prefix_spec_args = sizeof (prefix_spec_args) /
         sizeof (prefix_spec_args[0]);

static int single_case_flag;
static SPEC_PARAM spec_args[] = {
    {"index.parse.single_case",	getspec_bool,	(char *) &single_case_flag},
    TRACE_PARAM ("print.indiv.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    PROC_INST proc;
    SM_INDEX_DOCDESC doc_desc;
    int *token_inst;
    int sectid_inst;
} STATIC_INFO;

static SM_BUF internal_output = {0, 0, (char *) 0};
static STATIC_INFO *info;
static int max_inst = 0;


int
init_print_text_mitre_named (spec, qd_prefix)
SPEC *spec;
char *qd_prefix;
{
    char prefix_string[PATH_LEN];
    int new_inst, i;
    STATIC_INFO *ip;

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec(spec, &spec_args[0], num_spec_args))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_print_text_mitre_named");

    NEW_INST(new_inst);
    if (UNDEF == new_inst)
	return(UNDEF);
    ip = &info[new_inst];

    prefix = prefix_string;
    (void) sprintf(prefix_string, "print.%s", qd_prefix);
    if (UNDEF == lookup_spec_prefix(spec, &prefix_spec_args[0],
				    num_prefix_spec_args))
	return(UNDEF);
    if (UNDEF == (lookup_spec_docdesc (spec, &ip->doc_desc)))
        return (UNDEF);

    if (NULL == (ip->token_inst = Malloc(ip->doc_desc.num_sections, int)))
	return(UNDEF);

    /* Call all initialization procedures */
    ip->proc.ptab = proc.ptab;
    if (UNDEF == (ip->proc.inst = proc.ptab->init_proc(spec, qd_prefix)) ||
	UNDEF == (ip->sectid_inst = init_sectid_num (spec, qd_prefix)))    
	return (UNDEF);
    for (i = 0; i < ip->doc_desc.num_sections; i++) {
        (void) sprintf (prefix_string, "%sindex.section.%d.",
			qd_prefix ? qd_prefix : "", i );
        if (UNDEF == (ip->token_inst[i] =
                      ip->doc_desc.sections[i].tokenizer->init_proc
                      (spec, prefix_string)))
            return (UNDEF);
    }

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_print_text_mitre_named");
    return(new_inst);
}


int
print_text_mitre_named (text_id, output, inst)
EID *text_id;
SM_BUF *output;
int inst;
{
    char tok_buf[PATH_LEN];
    int last_word_ended_sent, status;
    long section_num, i, j;
    SM_BUF pp_buf, *out_p;
    SM_INDEX_TEXTDOC pp_doc;
    SM_TOKENTYPE *token;
    SM_TOKENSECT t_sect;
    STATIC_INFO *ip;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "print_text_mitre_named");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering print_text_mitre_named");

    if (output == NULL) {
	out_p = &internal_output;
	out_p->end = 0;
    }
    else out_p = output;

    /* Preparse */
    status = ip->proc.ptab->proc (text_id, &pp_doc, ip->proc.inst);
    if (1 != status) {
	PRINT_TRACE (2, print_string, "Trace: leaving print_text_mitre_named");
        return (status);
    }

    /* Tokenize and parse to get sentence information */
    for (i = 0; i < pp_doc.mem_doc.num_sections; i++) {
	if (UNDEF == add_buf_string("<s>", out_p)) return(UNDEF);
        /* Get the section number corresponding to this section id */
        if (UNDEF == sectid_num (&pp_doc.mem_doc.sections[i].section_id,
                                 &section_num,
                                 ip->sectid_inst))
            return (UNDEF);

        /* Construct a sm_buf giving this section's text, and tokenize it */
        pp_buf.buf = pp_doc.doc_text + pp_doc.mem_doc.sections[i].begin_section;
        pp_buf.end = pp_doc.mem_doc.sections[i].end_section -
            pp_doc.mem_doc.sections[i].begin_section;
        t_sect.section_num = section_num;
        if (UNDEF == ip->doc_desc.sections[section_num].tokenizer->proc 
	    (&pp_buf, &t_sect, ip->token_inst[section_num]))
            return (UNDEF);

	/* Print out each token. Most tokens printed as is. If it's 
	 * punctuation, we print out a space on either side. If it's
	 * the beginning/end of a sentence, we add the tag <s> or </s>.
	 */
	last_word_ended_sent = 1;
	for (j = 0; j < t_sect.num_tokens; j++) {
	    token = &t_sect.tokens[j];
	    switch (token->tokentype) {
	      case SM_INDEX_LOWER:
	      case SM_INDEX_MIXED:
	      case SM_INDEX_DIGIT:
		  if (UNDEF == add_buf_string(token->token, out_p))
		      return(UNDEF);
		  last_word_ended_sent = 0;
		  break;

	      case SM_INDEX_SPACE:
		  if (UNDEF == add_buf_string(token->token, out_p))
		      return(UNDEF);
		  break;

	      case SM_INDEX_NLSPACE:
		  if (UNDEF == add_buf_string(token->token, out_p)) 
		      return(UNDEF);
		  if (check_paragraph (token, last_word_ended_sent) &&
		      !last_word_ended_sent) {
		      if (UNDEF == add_buf_string("</s><s>", out_p))
			  return(UNDEF);
		      last_word_ended_sent = 1;
		  }
		  break;

	      case SM_INDEX_PUNC:
		  /* If this is a sentence break and we had seen some non-
		   * concept token (punctuation), then make sure that the
		   * sent/para they occurred in is known. If this punc ends
		   * a sent, then another sent end will have to be counted
		   * if it occurs (as in "...") so we fake that we've seen
		   * a non-concept token */
		  /* Additional hack: want to print `` or '' as ", rather
		   * than ` ` and ' '.
		   */
		  if ((token->token[0] == '\'' && token->token[1] == '\0' &&
		       j+1 < t_sect.num_tokens &&
		       t_sect.tokens[j+1].token[0] == '\'' &&
		       t_sect.tokens[j+1].token[1] == '\0') ||
		      (token->token[0] == '`' && token->token[1] == '\0' &&
		       j+1 < t_sect.num_tokens &&
		       t_sect.tokens[j+1].token[0] == '`' &&
		       t_sect.tokens[j+1].token[1] == '\0')) {
		      add_buf_string(" \" ", out_p);
		      j++;
		      break;
		  }
		  /* Gross hack: to handle apostrophes appropriately (Mitre
		   * tagger requires foo's -> foo 's; chris' chris '; 
		   * don't -> do n't; we're -> we 're.
		   * Hence: exclude ' from interior_chars; 
		   *     replace ' by " '" in general
		   *     special case handling for n't.
		   */
		  if (token->token[0] == '\'' && token->token[1] == '\0') {
		      char *prev_token = (j > 0) ? (token-1)->token : NULL;
		      if (prev_token && 
			  prev_token[strlen(prev_token)-1] == 'n' &&
			  j < t_sect.num_tokens &&
			  (token+1)->token &&
			  (token+1)->token[0] == 't' &&
			  (token+1)->token[1] == '\0') {
			  /* can't, don't, wasn't etc. 
			   * KLUDGE */
			  assert(out_p->buf[out_p->end - 1] == 'n');
			  out_p->buf[out_p->end - 1] = ' ';
			  if (UNDEF == add_buf_string("n\'", out_p))
			      return(UNDEF);
		      }
		      else {
			  if (UNDEF == add_buf_string(" \'", out_p))
			      return(UNDEF);
		      }
		      break;
		  }
		  if (token->token[0] == '-' && token->token[1] == '\0') {
		      if (UNDEF == add_buf_string("-", out_p))
			  return(UNDEF);
		      break;
		  }
		  if (check_sentence(token, j, t_sect.tokens + t_sect.num_tokens,
				     single_case_flag)) {
		      last_word_ended_sent = 1;
		      sprintf(tok_buf, " %s</s><s>", token->token);
		      if (UNDEF == add_buf_string(tok_buf, out_p))
			  return(UNDEF);
		  }
		  else {
		      last_word_ended_sent = 0;	/* since it didn't */
		      if (token->token[0] == '.' && token->token[1] == '\0') {
			  if (UNDEF == add_buf_string(".", out_p))
			  return(UNDEF);
		      }
		      else {
			  sprintf(tok_buf, " %s ", token->token);
			  if (UNDEF == add_buf_string(tok_buf, out_p))
			      return(UNDEF);
		      }
		  }
		  break;
		  
	      case SM_INDEX_IGNORE:
	      case SM_INDEX_SECT_END:
		  break;

	      case SM_INDEX_SENT:
	      case SM_INDEX_PARA:
	      case SM_INDEX_SECT_START: 
	      default:
		  set_error(SM_INCON_ERR, "Unexpected token type", 
			    "print_text_mitre_named");
		  return (UNDEF);
	    }
	}
	if (UNDEF == add_buf_string("</s>", out_p))
	    return(UNDEF);
    }

    sprintf(tok_buf, "\n<s> SmArTEndOfDocument%ld . </s>\n", 
	    text_id->id);
    if (UNDEF == add_buf_string(tok_buf, out_p))
	return(UNDEF);

    if (output == NULL) {
	(void) fwrite (out_p->buf, 1, out_p->end, stdout);
	out_p->end = 0;
    }

    PRINT_TRACE (2, print_string, "Trace: leaving print_text_mitre_named");
    return (1);
}


int
close_print_text_mitre_named (inst)
int inst;
{
    int i;
    STATIC_INFO *ip;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "close_print_text_mitre_named");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering close_print_text_mitre_named");

    if (UNDEF == ip->proc.ptab->close_proc (ip->proc.inst) ||
	UNDEF == close_sectid_num (ip->sectid_inst))
	return (UNDEF);
    for (i = 0; i < ip->doc_desc.num_sections; i++) {
	if (UNDEF == (ip->doc_desc.sections[i].tokenizer->close_proc (ip->token_inst[i])))
	    return(UNDEF);
    }
    Free(ip->token_inst);

    PRINT_TRACE (2, print_string, "Trace: leaving close_print_text_mitre_named");
    return (0);
}
