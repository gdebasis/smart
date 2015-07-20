#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/tloc_highlight.c,v 11.0 1992/07/21 18:20:09 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Given a query vector and a textloc, highlight the query terms in the doc.
 *2 highlight (qvec, textloc, inst)
 *3 VEC *qvec;
 *3 TEXTLOC *textloc;
 *3 int inst;
 *4 init_highlight (spec, unused)
 *5   "inter.highlight.preparse"
 *5   "inter.highlight.trace"
 *4 close_highlight(inst)
 *7 Preparses the document from the textloc, tokenizes and parses it;
 *7 highlights the appropriate concepts in the parsed text by looking
 *7 up the concept numbers in the query vector and writes the result
 *7 to a temporary file; changes textloc to point to this temporary file.
***********************************************************************/

/* #include <signal.h>
#include <curses.h>
#include <term.h>
*/
extern void tputs(); 
extern char *tgetstr();
extern int tgetent();

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "retrieve.h"
#include "vector.h"
#include "docindex.h"
#include "textloc.h"
#include "inst.h"
#include "buf.h"
#include "docdesc.h"


/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
    int spec_id;

    PROC_INST pp;
    int sectid_inst;
    SM_INDEX_DOCDESC doc_desc;
    /* Instantiation Id's for procedures to be called */
    int *token_inst;
    int *parse_inst;

    char *token_buf;
    int token_buf_size;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static char *begin_highlight, *end_highlight;
static char termbuf[PATH_LEN], charbuf[PATH_LEN];
static char output_buf[4*PATH_LEN], *curr, *tmp_file;
static int highlight_size; /* no. of bytes reqd to store highlighting chars. */
static FILE *fp;
static PROC_TAB *pp;

static SPEC_PARAM spec_args[] = {
    { "inter.highlight.named_preparse", getspec_func, (char *) &pp },
    TRACE_PARAM ("inter.highlight.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int in_query(), write_token();
int putchr(char c);
 

int
init_highlight (spec, unused)
SPEC *spec;
char *unused;
{
    char param_prefix[PATH_LEN], *term, *cptr;
    int new_inst, i;
    STATIC_INFO *ip;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    /* Check to see if this spec has already been initialized.  If
       so, that instantiation will be used. */
    for (i = 0; i < max_inst; i++) {
        if (info[i].valid_info && spec->spec_id == info[i].spec_id) {
            info[i].valid_info++;
            PRINT_TRACE (2, print_string,
                         "Trace: entering/leaving init_highlight");
            return (i);
        }
    }
    
    PRINT_TRACE (2, print_string, "Trace: entering init_highlight");
    
    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];
    ip->spec_id = spec->spec_id;

    ip->pp.ptab = pp;
    if(UNDEF == (ip->pp.inst = ip->pp.ptab->init_proc(spec, "doc.")))
      return (UNDEF);

    if (UNDEF == (ip->sectid_inst = init_sectid_num (spec, (char *) NULL)))
        return (UNDEF);

    if (UNDEF == (lookup_spec_docdesc (spec, &ip->doc_desc)))
        return (UNDEF);

    /* Reserve space for the instantiation ids of the called procedures. */
    if (NULL == (ip->token_inst = (int *)
                malloc ((unsigned) ip->doc_desc.num_sections * sizeof (int)))||
        NULL == (ip->parse_inst = (int *)
                malloc ((unsigned) ip->doc_desc.num_sections * sizeof (int))))
        return (UNDEF);

    /* Call all initialization procedures */
    for (i = 0; i < ip->doc_desc.num_sections; i++) {
        (void) sprintf (param_prefix, "index.section.%d.", i);
        if (UNDEF == (ip->token_inst[i] =
                      ip->doc_desc.sections[i].tokenizer->init_proc
                      (spec, param_prefix)))
            return (UNDEF);
        if (UNDEF == (ip->parse_inst[i] =
                      ip->doc_desc.sections[i].parser->init_proc
                      (spec, param_prefix)))
            return (UNDEF);
    }

    ip->token_buf_size = 4 * PATH_LEN;
    if(NULL == (ip->token_buf = Malloc(ip->token_buf_size, char)))
      return(UNDEF);

    if((term = getenv("TERM")) == NULL) term = "unknown";
    if(tgetent(termbuf, term) <= 0) strcpy(termbuf, "dumb:hc:");
    cptr = charbuf;
    if(NULL == (begin_highlight = tgetstr("so", &cptr)) ||
       NULL == (end_highlight = tgetstr("se", &cptr)))
      begin_highlight = end_highlight = "";
    else highlight_size = cptr - charbuf;

    if(NULL == (tmp_file = Malloc(PATH_LEN, char))) return(UNDEF);
    (void) sprintf(tmp_file, "/tmp/sm_hi.%d.o", (int) getpid());

    ip->valid_info = 1;
             
    PRINT_TRACE (2, print_string, "Trace: leaving init_highlight");
    
    return (new_inst);
}


int
highlight(qvec, textloc, inst)
VEC *qvec;
TEXTLOC *textloc;
int inst;
{    
  char *cptr;
  long section_num, num_bytes, file_size = 0, last_token, needed, i, j;
  EID docid;
  STATIC_INFO *ip;
  SM_INDEX_TEXTDOC pp_doc;	/* preparsed version of doc. */
  SM_BUF pp_buf;		/* contains the text of each section in turn */
  SM_TOKENSECT t_sect;		/* contains tokenized sections */
  SM_CONSECT p_sect;		/* contains parsed version of sections */ 
  SM_CONLOC *conloc_ptr;
  
  PRINT_TRACE(2, print_string, "Trace: entering highlight");
  
  if(! VALID_INST (inst)) {
    set_error (SM_ILLPA_ERR, "Instantiation", "highlight");
    return(UNDEF);
  }
  ip  = &info[inst];
  
  if(NULL == (fp = fopen(tmp_file, "w")))
    { set_error (SM_ILLPA_ERR, "Opening temp file", "highlight");
      return(UNDEF);
    }

  /* PREPARSE DOC. */
  docid.id = textloc->id_num; EXT_NONE(docid.ext);
  if(UNDEF == ip->pp.ptab->proc(&docid, &pp_doc, ip->pp.inst))
    return(UNDEF);
  
  curr = output_buf;
  /* TOKENIZE AND PARSE DOC. */
  for(i = 0; i < pp_doc.mem_doc.num_sections; i++) 
    { /* Get the section number corresponding to this section id */
      if(UNDEF == sectid_num(&pp_doc.mem_doc.sections[i].section_id,
			     &section_num, ip->sectid_inst))
	return (UNDEF);
    
      /* Construct a sm_buf giving this section's text, and tokenize it */
      pp_buf.buf = pp_doc.doc_text + pp_doc.mem_doc.sections[i].begin_section;
      pp_buf.end = pp_doc.mem_doc.sections[i].end_section -
	pp_doc.mem_doc.sections[i].begin_section;
      t_sect.section_num = section_num;      
      if(UNDEF == 
	 ip->doc_desc.sections[section_num].tokenizer->proc(&pp_buf, &t_sect, 
					         ip->token_inst[section_num]))
 	return (UNDEF);

      /* Parser may stem in place; so, we need to save the original tokens 
       * which we use for output later.
       * 1. Calculate space needed. This assumes that the token strings for
       * this section are stored contiguously in a large character array, 
       * and "t_sect.tokens[i].token"s are offsets into this character array.
       * Thus, t_sect.tokens[0].token points to the beginning of this array,
       * and t_sect.tokens[t_sect.num_tokens - 1].token points close to the
       * end of the array.
       * (When token_sect is used, the last token (t_sect.tokens[t_sect.num_
       * tokens - 1]) is simply an end-of-section marker and is set to NULL.
       * See libindexing/token_sect.c:430. This would cause a segmentation 
       * fault.)
       * 2. Copy over the original tokens.
       * 3. After parsing is over, change the "t_sect.tokens[i].token" 
       * pointers to point to the saved tokens intstead of the stemmed ones.
       */
      needed = t_sect.tokens[t_sect.num_tokens - 1].token
	       + strlen(t_sect.tokens[t_sect.num_tokens - 1].token) + 1
	       - t_sect.tokens[0].token;
      if(ip->token_buf_size < needed)
	{ ip->token_buf_size += needed;
	  Free(ip->token_buf);
	  if(NULL == (ip->token_buf = Malloc(ip->token_buf_size, char)))
	    return(UNDEF);
	}
      bcopy(t_sect.tokens[0].token, ip->token_buf, needed);
	
      /* Parse the tokenized section, yielding a list of concept numbers
	 and locations */
      if(UNDEF ==
	 ip->doc_desc.sections[section_num].parser->proc(&t_sect, &p_sect,
					      ip->parse_inst[section_num]))
	return (UNDEF);

      cptr = ip->token_buf;
      for(j = 0; j < t_sect.num_tokens; j++)
	{ t_sect.tokens[j].token = cptr;
	  while(*cptr != '\0') cptr++;
	  cptr++;
	}

      /* NOTE: conloc_ptr->token_num is the token number within this section */
      if(p_sect.num_concepts > 0) 
	{ last_token = -1;	/* last token written out */
	  for(conloc_ptr = p_sect.concepts;
	      conloc_ptr < p_sect.concepts + p_sect.num_concepts;
	      conloc_ptr++)
	    { if(conloc_ptr->ctype) 
	        continue; /* only single are terms highlighted */
	      while(last_token < conloc_ptr->token_num-1)
		/* write out all tokens upto (not including) current one */
		{ assert(last_token < t_sect.num_tokens-1);
		  if(UNDEF == (num_bytes = 
			       write_token(t_sect.tokens[++last_token].token, 
					   0)))
		    return(UNDEF);
		  file_size += num_bytes;
		}
	      if(UNDEF == (num_bytes = 
			   write_token(t_sect.tokens[++last_token].token, 
				       in_query(conloc_ptr, qvec))))
		return(UNDEF);
	      file_size += num_bytes;	      
	    }
	  /* Output remaining tokens following last one assigned con no. */
	  while (last_token < t_sect.num_tokens - 1) {
	      if (UNDEF == (num_bytes = 
			    write_token(t_sect.tokens[++last_token].token, 0)))
		  return(UNDEF);
	      file_size += num_bytes;
	  }
	}
      else /* output all tokens in this section */
	for(j=0; j < t_sect.num_tokens; j++)
	  { if(UNDEF == (num_bytes = write_token(t_sect.tokens[j].token, 0)))
	      return(UNDEF);
	    file_size += num_bytes;
	  }
    }

  if(fwrite(output_buf, 1, curr - output_buf, fp) < curr - output_buf) 
    { set_error(SM_ILLPA_ERR, "Writing temp file", "highlight");
      return(UNDEF);
    } 
  file_size += curr - output_buf;
  fclose(fp);
  textloc->file_name = tmp_file;
  textloc->begin_text = 0;
  textloc->end_text = file_size;
    
  PRINT_TRACE (2, print_string, "Trace: leaving highlight");
  return (1);
}


int
close_highlight(inst)
int inst;
{
    long i;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_highlight");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_highlight");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;

    if (ip->valid_info == 0) {
        for (i = 0; i < ip->doc_desc.num_sections; i++) {
            if (UNDEF == (ip->doc_desc.sections[i].tokenizer->close_proc
                          (ip->token_inst[i])) ||
                UNDEF == (ip->doc_desc.sections[i].parser->close_proc
                          (ip->parse_inst[i])))
                return (UNDEF);
        }
        if (UNDEF == close_sectid_num (ip->sectid_inst))
            return (UNDEF);

        (void) free ((char *) ip->token_inst);
        (void) free ((char *) ip->parse_inst);
        (void) free ((char *) ip->token_buf);
    }

    remove(tmp_file);

    PRINT_TRACE (2, print_string, "Trace: leaving close_highlight");
    return (0);
}


int
in_query(conloc, query)
SM_CONLOC *conloc;
VEC *query;
{
  int i;
  CON_WT *conwtp;

  conwtp = query->con_wtp;
  for(i=0; i < conloc->ctype; i++)
    conwtp += query->ctype_len[i];
  for(i=0; i < query->ctype_len[conloc->ctype]; i++)
    if(conwtp[i].con == conloc->con) return(1);
  return(0);
}


int 
write_token(token, query_term)
char *token;
int query_term;
{
  int tok_len, bytes_written = 0;
  
  tok_len = strlen(token);
  if(curr + tok_len + highlight_size - output_buf >= 4*PATH_LEN) 
    { if(fwrite(output_buf, 1, curr - output_buf, fp) < curr - output_buf) 
	{ set_error(SM_ILLPA_ERR, "Writing temp file", "highlight");
	  return(UNDEF);
	}
      bytes_written = curr - output_buf;
      curr = output_buf;
    }
  if(query_term) tputs(begin_highlight, 1, putchr);
  bcopy(token, curr, tok_len);
  curr += tok_len;
  if(query_term) tputs(end_highlight, 1, putchr);
  return(bytes_written);
}


int
putchr(char c)
{
  *curr++ = c;
  return((int)c);
}

