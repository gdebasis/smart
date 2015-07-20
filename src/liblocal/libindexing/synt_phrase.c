#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/synt_phrase.c,v 11.2 1993/02/03 16:49:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Generate syntactic phrases given a document.
 *1
 *2 synt_phrase (did, phr_vec, inst)
 *3   EID *did;
 *3   VEC *phr_vec;
 *3   int inst;

 *4 init_synt_phrase (spec, qd_prefix)
 *5   "index.syn_phr.print"
 *5   "index.syn_phr.grammar"
 *5   "index.syn_phr.lisp_trace_flag"
 *5   "index.syn_phr.token_to_con"
 *5   "index.syn_phr.trace"
 *4 close_synt_phrase (inst)

 *7 Generates a vector for syntactic phrases contained in the given doc.
 *7 using POS tagging and flat-bracketting. 
 *7 Return UNDEF if error, else 0;
 *7 Caution: "!" is used as the eot signal by the lisp process. If the
 *7 normal output contains "!" we will be in trouble.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "proc.h"
#include "sm_display.h"
#include "docindex.h"
#include "inst.h"
#include "io.h"
#include "vector.h"
#include <ctype.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    char *token;
    long con;
    float idf;
} WORD_INFO;

static PROC_TAB *print;
static PROC_TAB *tokcon;
static PROC_TAB *contok;
static long stem_flag;
/*static PROC_TAB *weight_vec;*/
static char *grammar;
static long lisp_trace;
static SPEC_PARAM spec_args[] = {
    {"index.syn_phr.print", getspec_func, (char *)&print},
    {"index.syn_phr.token_to_con", getspec_func, (char *)&tokcon},
    {"index.syn_phr.con_to_token", getspec_func, (char *)&contok},
    {"index.syn_phr.stem_wanted", getspec_bool, (char *)&stem_flag},
/*    {"index.syn_phr.weight", getspec_func, (char *)&weight_vec},*/
    {"index.syn_phr.grammar", getspec_string, (char *)&grammar},
    {"index.syn_phr.lisp_trace_flag", getspec_bool, (char *)&lisp_trace},
    TRACE_PARAM ("index.syn_phr.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static long print_inst, w_inst, contok_inst, sp_inst, idf_inst;
static int (*add_phrase)();
static SM_BUF output_buf = {0, 0, (char *) 0};
static WORD_INFO *words;
static int max_words;
static CON_WT *phrases;
static int max_phr;

static char init_instr[] = 
	"(load \"/home/nlp/lib/lisp/shared-init.lisp\") (load-system :gilligan) (in-package :circus) (cd \"~nlp/src/brill/RULE_BASED_TAGGER_V1.14/Bin_and_Data\") (setf stop-phr nil) (defun postproc (bracket-output) (setf *standard-output* *terminal-io*) (let ((text (second (first bracket-output)))) (dolist (phr text) (when (and (listp phr) (equal (first phr) 'NP) (not (member (second phr) stop-phr :test #'equal))) (format t \"~%~a\" (second phr))))) (format t \"~%!\") (setf *standard-output* devnull))\n";
static int stoc[2], ctos[2];
static FILE *lisp_ip, *lisp_op;

static int write_to_pipe(), add_phrase_ss(), add_phrase_noss(), compare_con();

int init_tokcon_dict_niss(), tokcon_dict_niss(), close_tokcon_dict_niss();


int
init_synt_phrase (spec, qd_prefix)
SPEC *spec;
char *qd_prefix;
{
    char outbuf[PATH_LEN];
    int fd, pid;
    FILE *fp;
    CONTEXT old_context;

    PRINT_TRACE (2, print_string, "Trace: entering init_synt_phrase");

    if (-1 == pipe(stoc) ||
	-1 == pipe(ctos)) {
	set_error(0, "Opening pipes", "init_synt_phrase");
	return(UNDEF);
    }

    if (0 == (pid = vfork())) {
	/* Child process: Lisp */
	close(stoc[1]); close(ctos[0]); 
	dup2(stoc[0], 0);
	dup2(ctos[1], 1);
	if (NULL == (fp = fopen("/dev/null", "w"))) {
	    set_error(0, "Opening /dev/null", "init_synt_phrase");
	    return(UNDEF);
	}
	fd = fileno(fp);
	dup2(fd, 2);
	(void) execl("/usr/bin/rsh", "rsh", "ewigkeit", 
		     "/usr/local/lib/lisp/lisp-clx-clos", "-l", 
		     "/home/mitra/NLP/init", (char *) 0);
	fclose(fp);
    }
    else {
	/* Parent process: Smart */
	close(stoc[0]); close(ctos[1]); 

	if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
	    return (UNDEF);

	if (UNDEF == (print_inst = print->init_proc (spec, qd_prefix)) ||
	    UNDEF == (w_inst = tokcon->init_proc(spec, "index.section.word.")))
	    return(UNDEF);
	if (stem_flag &&
	    UNDEF == (contok_inst = contok->init_proc(spec, "ctype.0.")))
	    return(UNDEF);
	old_context = get_context();
	if (0 == strcmp(qd_prefix, "query.")) 
	    set_context (CTXT_INDEXING_DOC);
	if (UNDEF == (sp_inst = init_tokcon_dict_niss(spec, "synphr.")))
	    return(UNDEF);
	set_context (old_context);
	if (UNDEF == (idf_inst = init_con_cw_idf(spec, "ctype.0.")))
	    return(UNDEF);
/*
	(void) sprintf(outbuf, "index.syn_phr.%s", qd_prefix);
	if (UNDEF == (wt_inst = weight_vec->init_proc(spec, outbuf)))
	    return(UNDEF);
*/
	if (stem_flag) add_phrase = add_phrase_ss;
	else add_phrase = add_phrase_noss;
	words = NULL; max_words = 0;
	phrases = NULL; max_phr = 0;

	if (lisp_trace) {
	    if (NULL == (lisp_ip = fopen("/home/mitra/tmp/lisp-input", "w")) ||
		NULL == (lisp_op = fopen("/home/mitra/tmp/lisp-output", "w")))
		return(UNDEF);
	}

	if (UNDEF == write_to_pipe(stoc[1], init_instr)) {
	    set_error(0, "Writing to pipe", "init_synt_phrase");
	    return(UNDEF);
	}

	if (VALID_FILE(grammar)) {
	    sprintf(outbuf, "(defparameter *brill-grammar* '(%s))\n", grammar);
	    if (UNDEF == write_to_pipe(stoc[1], outbuf)) {
		set_error(0, "Writing to pipe", "init_synt_phrase");
		return(UNDEF);
	    }
	}

	if (0 == strcmp(qd_prefix, "query.")) {
	    sprintf(outbuf, 
		    "(setf stop-phr '((A RELEVANT DOCUMENT) (RELEVANT A DOCUMENT) (RELEVANT DATA) (RELEVANT DOCUMENT) (RELEVANT DOCUMENTS) (RELEVANT INFORMATION) (THE RELEVANT ITEM)))");
	    if (UNDEF == write_to_pipe(stoc[1], outbuf)) {
		set_error(0, "Writing to pipe", "init_synt_phrase");
		return(UNDEF);
	    }
	}

	PRINT_TRACE (2, print_string, "Trace: leaving init_synt_phrase");
	return 0;
    }

    /* never reached */
    return(0);
}

int
synt_phrase (did, phr_vec, inst)
EID *did;
VEC *phr_vec;
int inst;
{
    char tmp_file[PATH_LEN], outbuf[PATH_LEN], inbuf[PATH_LEN], *instart, eom;
    long toread, nread, num_phr, tf, i, j;
    FILE *fp;

    PRINT_TRACE (2, print_string, "Trace: entering synt_phrase");

    /* Print text to a tmp file */
    output_buf.end = 0;	
    if (UNDEF == print->proc (did, &output_buf, print_inst))
	return UNDEF;
    sprintf (tmp_file, "/fsys/thor/d/tmp/sm_text.%ld.o", did->id);
    if (NULL == (fp = fopen(tmp_file, "w"))) {
	set_error (0, "Opening temp file", "synt_phrase");
        return(UNDEF);
    }
    if (output_buf.end != fwrite(output_buf.buf, 1, output_buf.end, fp)) {
	set_error (0, "Writing temp file", "synt_phrase");
        return(UNDEF);
    }
    fclose(fp);

    /* Call flat-bracketer on tmp file */
    sprintf(outbuf, "(setf ppin (flat-full \"%s\"))\n", tmp_file);
    if (UNDEF == write_to_pipe(stoc[1], outbuf) ||
	UNDEF == write_to_pipe(stoc[1], "(postproc ppin)\n")) {
	set_error (0, "Writing to pipe", "synt_phrase");
        return(UNDEF);
    }

    /* Get phrase-list from flat-bracketer */
    /* We do block-reads for efficiency. Blocks need not contain an integral
     * number of lines/tokens. We pretend they do. When a block ends with an 
     * incomplete line, we save the incomplete line at the beginning of the 
     * input buffer and read the next block in after this initial part. Thus,
     * the block looks as if it started at the beginning of the last line in
     * the previous block.
     * Caution: if a line is longer than PATH_LEN, we will be in trouble.
     */
    num_phr = 0;
    eom = 0;
    instart = inbuf;	/* read from pipe to location pointed to by instart */
    toread = PATH_LEN;	/* read atmost toread characters */
    while (!eom) {
	char *start_line, *end_line, *end_buf, *token;

	nread = read(ctos[0], instart, toread);
	if (lisp_trace) {
	    fwrite(instart, 1, nread, lisp_op); 
	    fflush(lisp_op);
	}
	if (instart[nread - 1] == '!') {
	    nread--; eom = 1;
	}

	/* Process each line in turn: a line is either
	 * empty (no non-whitespace chars), or
	 * a comment (starts with ';'), or
	 * a phrase (starts with '(' and ends with ')'.
	 */
	start_line = inbuf;
	end_buf = instart + nread;
	while (1) {
	    while (start_line < end_buf && isspace(*start_line))
		start_line++;
	    if (start_line == end_buf) {
		instart = inbuf;
		toread = PATH_LEN;
		break;
	    }
	    for (end_line = start_line; 
		 end_line < end_buf && *end_line != '\n';
		 end_line++);
	    if (end_line == end_buf) {
		/* Incomplete line: save in the beginning of inbuf,
		 * update instart, toread and continue. */
		 bcopy(start_line, inbuf, end_line - start_line);
		 instart = inbuf + (end_line - start_line);
		 toread = PATH_LEN - (end_line - start_line);
		 if (toread == 0) {
		     fprintf(stderr, "Line too long\n");
		     write(2, inbuf, PATH_LEN);
		     return(UNDEF);
		 }
		 break;
	    }
	    if (*start_line == '(' && *(end_line - 1) == ')') {
		/* Useful line */
		token = start_line + 1;
		*(end_line - 1) = '\0';
		if (UNDEF == (*add_phrase)(token, &num_phr))
		    return(UNDEF);
	    }
	    else if (*start_line != ';') {
		*end_line = '\0';
		fprintf(stderr, "Unexpected lisp output: %s\n", start_line);
	    }
	    start_line = end_line + 1;
	}
    }

    /* Generate the final weighted vector */
    /* See note for add_phrase. */
    if (num_phr > 0) {
	qsort(phrases, num_phr, sizeof(CON_WT), compare_con);
	tf = 1;
	for (i = 0, j = 1; j < num_phr; j++) {
	    if (phrases[j].con == phrases[i].con)
		tf++;
	    else {
		phrases[i].wt *= (1 + log(tf));
		phrases[++i] = phrases[j];
		tf = 1;
	    }
	}
	phrases[i].wt *= (1 + log(tf));
	num_phr = i + 1;
    }
    phr_vec->id_num = *did;
    phr_vec->num_conwt = phr_vec->ctype_len[0] = num_phr;
    phr_vec->con_wtp = phrases;

    if (!lisp_trace) {
	remove(tmp_file);
	sprintf (tmp_file, "/fsys/thor/d/tmp/sm_text.%ld.o.bi", did->id);
	remove(tmp_file);
	sprintf (tmp_file, "/fsys/thor/d/tmp/sm_text.%ld.o.bo", did->id);
	remove(tmp_file);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving synt_phrase");
    return 0;
}

int
close_synt_phrase (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_synt_phrase");

    if (UNDEF == write_to_pipe(stoc[1], "(quit)\n")) {
	set_error(0, "Writing to pipe", "close_synt_phrase");
	return(UNDEF);
    }

    if (UNDEF == print->close_proc(print_inst) ||
	UNDEF == tokcon->close_proc(w_inst) ||
	UNDEF == close_tokcon_dict_niss(sp_inst) ||
	UNDEF == close_con_cw_idf(idf_inst))
/*	UNDEF == weight_vec->close_proc(wt_inst))*/
	return(UNDEF);

    if (stem_flag &&
	UNDEF == contok->close_proc(contok_inst))
	return(UNDEF);

    if (max_words) Free(words);
    if (max_phr) Free(phrases);

    if (lisp_trace) {
	fclose(lisp_ip); fclose(lisp_op);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_synt_phrase");
    return (0);
}


static int
write_to_pipe(p, s)
int p;
char *s;
{
    int towrite, nwritten;

    towrite = strlen(s);
    nwritten = write(p, s, towrite);
    if (lisp_trace) {
	fwrite(s, 1, towrite, lisp_ip); fflush(lisp_ip);
    }
    if (towrite != nwritten)
	return UNDEF;
    return 1;    
}

/* 1. Add the phrase represented by tok and phrases formed by taking each 
 *    pair of words in tok to the syntactic phrase dictionary.
 * 2. Compute respective idfs (phrase idf = max(idf of constituent single 
 *    words)).
 * Ideally, weighting should be done through weight_vec->proc. Doing the
 * idf that way is a little troublesome and also inefficient. Hence the
 * following hack: idf is computed here; and the tf factor is computed
 * when the duplicates are collapsed above.
 * Assumes that "tok" is in lisp output format (sans the brackets), i.e.,
 * it consists of a +ve no. of single words separated by single blanks. 
 * For example: "LONG TERM CARE CONFINEMENTS".
 */
int
add_phrase_noss(tok, num)
char *tok;
long *num;
{
    char tok_buf[PATH_LEN], *cptr;
    long num_words, num_p, w_con, sp_con, i, j;
    float idf;

    /* Count the number of single words */
    for (cptr = tok, num_words = 1; *cptr; cptr++) {
	*cptr = (char) tolower(*cptr);
	if (*cptr == ' ') num_words++;
    }
    if (num_words == 1) return(0);

    /* Allocate space if needed */
    if (num_words > max_words) {
	if (max_words) Free(words);
	max_words += 2 * num_words;
	if (NULL == (words = Malloc(max_words, WORD_INFO)))
	    return(UNDEF);
    }
    if (*num + (num_words * (num_words - 1))/2 + 1 > max_phr) {
	max_phr += num_words * (num_words - 1) + 1;
	if (NULL == (phrases = Realloc(phrases, max_phr, CON_WT)))
	    return(UNDEF);
    }

    /* Set up WORD_INFO: token, concept number and idf for each word */
    for (i = 0, words[0].token = cptr = tok; i < num_words-1; cptr++) {
	if (*cptr == ' ') {
	    *cptr = '\0';
	    words[++i].token = cptr + 1;
	}
    }
    for (i = 0; i < num_words; i++) {
	if (UNDEF == tokcon->proc(words[i].token, &w_con, w_inst))
	    return(UNDEF);
	idf = 0.0;
	if (UNDEF != w_con &&
	    UNDEF == con_cw_idf(&w_con, &idf, idf_inst))
	    return(UNDEF);
	words[i].con = w_con;
	words[i].idf = idf;
    }

    /* Add each pair of words as a phrase */
    num_p = *num;
    for (i = 0; i < num_words; i++) {
	for (j = i + 1; j < num_words; j++) {
	    sprintf(tok_buf, "%s %s", words[i].token, words[j].token);
	    if (UNDEF == tokcon_dict_niss(tok_buf, &sp_con, sp_inst))
		return(UNDEF);
	    if (UNDEF == sp_con) continue;
	    phrases[num_p].con = sp_con;
	    phrases[num_p++].wt = MAX(words[i].idf, words[j].idf);
	}
    }

    /* Add entire phrase */
    if (num_words > 2) {
	for (i = 0, cptr = tok, idf = 0.0; i < num_words - 1; cptr++) 
	    if (*cptr == '\0') {
		*cptr = ' ';
		if (words[i].idf > idf) idf = words[i].idf;
		i++;
	    }
	if (words[i].idf > idf) idf = words[i].idf;
	if (UNDEF == tokcon_dict_niss(tok, &sp_con, sp_inst))
	    return(UNDEF);
	if (UNDEF != sp_con) {
	    phrases[num_p].con = sp_con;
	    phrases[num_p++].wt = idf;
	}
    }
    *num = num_p;
    return 1;
}

/* As above, except that all stopwords are removed from tok and 
 * the remaining words are replaced by their stems.
 */
int
add_phrase_ss(tok, num)
char *tok;
long *num;
{
    char stem_buf[PATH_LEN], tok_buf[PATH_LEN], *stemmed_tok, *cptr;
    long num_words, num_p, w_con, sp_con, i, j;
    float idf;

    /* Count the number of single words */
    for (cptr = tok, num_words = 1; *cptr; cptr++) {
	*cptr = (char) tolower(*cptr);
	if (*cptr == ' ') num_words++;
    }
    if (num_words == 1) return(0);

    /* Allocate space if needed */
    if (num_words > max_words) {
	if (max_words) Free(words);
	max_words += 2 * num_words;
	if (NULL == (words = Malloc(max_words, WORD_INFO)))
	    return(UNDEF);
    }

    /* Set up WORD_INFO: token, concept number and idf for each word */
    for (i = 0, words[0].token = cptr = tok; i < num_words-1; cptr++) {
	if (*cptr == ' ') {
	    *cptr = '\0';
	    words[++i].token = cptr + 1;
	}
    }
    for (i = 0, j = 0, cptr = stem_buf; i < num_words; i++) {
	if (UNDEF == tokcon->proc(words[i].token, &w_con, w_inst))
	    return(UNDEF);
	if (UNDEF == w_con) continue;
	if (UNDEF == contok->proc(&w_con, &stemmed_tok, contok_inst))
	    return(UNDEF);
	strcpy(cptr, stemmed_tok); 
	if (UNDEF == con_cw_idf(&w_con, &idf, idf_inst))
	    return(UNDEF);
	words[j].token = cptr; cptr += strlen(stemmed_tok) + 1;
	words[j].con = w_con;
	words[j++].idf = idf;
    }
    if (j < 2) return(0);
    num_words = j;

    /* Allocate space if needed */
    if (*num + (num_words * (num_words - 1))/2 + 1 > max_phr) {
	max_phr += num_words * (num_words - 1) + 1;
	if (NULL == (phrases = Realloc(phrases, max_phr, CON_WT)))
	    return(UNDEF);
    }

    /* Add each pair of words as a phrase */
    num_p = *num;
    for (i = 0; i < num_words; i++) {
	for (j = i + 1; j < num_words; j++) {
	    sprintf(tok_buf, "%s %s", words[i].token, words[j].token);
	    if (UNDEF == tokcon_dict_niss(tok_buf, &sp_con, sp_inst))
		return(UNDEF);
	    if (UNDEF == sp_con) continue;
	    phrases[num_p].con = sp_con;
	    phrases[num_p++].wt = MAX(words[i].idf, words[j].idf);
	}
    }

    /* Add entire phrase */
    if (num_words > 2) {
	for (i = 0, cptr = tok_buf, idf = 0.0; i < num_words; i++, cptr++) {
	    sprintf(cptr, "%s", words[i].token);
	    cptr += strlen(words[i].token);
	    *cptr = ' ';
	    if (words[i].idf > idf) idf = words[i].idf;
	}
	*(cptr-1) = '\0';
	if (UNDEF == tokcon_dict_niss(tok_buf, &sp_con, sp_inst))
	    return(UNDEF);
	if (UNDEF != sp_con) { 
	    phrases[num_p].con = sp_con;
	    phrases[num_p++].wt = idf;
	}
    }

    *num = num_p;
    return 1;
}

static int
compare_con(c1, c2)
CON_WT *c1, *c2;
{
    return(c1->con - c2->con);
}
