#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/synt_phrase.c,v 11.2 1993/02/03 16:49:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 
 *1
 *2 vec_pvectxt (vec_file, pvec_file, inst)
 *3   char *vec_file;
 *3   char *pvec_file;
 *3   int inst;

 *4 init_vec_pvectxt (spec, unused)
 *4 close_vec_pvectxt (inst)
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "proc.h"
#include "inst.h"
#include "io.h"
#include "vector.h"
#include "fdbk_stats.h"
#include <ctype.h>
#include <string.h>
#include <unistd.h>

static char *synphr_file;
static long *fs_flag;
static float reqd_percent, pvalue;
static PROC_TAB *contok, *tokcon;
static long rmode;
static SPEC_PARAM spec_args[] = {
    {"convert.vec_pvectxt.synphr_file", getspec_dbfile, (char *)&synphr_file},
    {"convert.vec_pvectxt.fs_flag", getspec_long, (char *)&fs_flag},
    {"convert.vec_pvectxt.reqd_percent", getspec_float, (char *)&reqd_percent},
    {"convert.vec_pvectxt.pvalue", getspec_float, (char *)&pvalue},
    {"convert.vec_pvectxt.token_to_con", getspec_func, (char *)&tokcon},
    {"convert.vec_pvectxt.con_to_token", getspec_func, (char *)&contok},
    {"convert.vec_pvectxt.vec_file.rmode", getspec_filemode, (char *)&rmode},
   TRACE_PARAM ("convert.vec_pvectxt.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *fs_file;
static long fsmode;
static SPEC_PARAM fs_spec_args[] = {
    {"convert.vec_pvectxt.fdbk_stats", getspec_dbfile, (char *)&fs_file},
    {"convert.vec_pvectxt.fdbk_stats.rmode", getspec_filemode, (char *)&fsmode},
};
static int num_fs_spec_args = sizeof (fs_spec_args) / sizeof (fs_spec_args[0]);


/* Using #defines for now instead of mallocs and reallocs */
#define MAX_TERMS 50		/* Max. no. of terms in pnorm query */
#define MAX_PHRASES 50		/* Max. no. of syntactic phrases in a doc. */
#define MAX_PHRASE_LEN 20	/* Max. no. of words in a syntactic phrase */

typedef struct {
    long num_words;
    long words[MAX_PHRASE_LEN];
} SYNT_PHR;

static SYNT_PHR phrase_list[MAX_PHRASES];
static int tokcon_inst, contok_inst;

static int add_phrase(), included(), compare_wt();
static void add_term();


int
init_vec_pvectxt (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
	return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_vec_pvectxt");

    if (fs_flag &&
	UNDEF == lookup_spec (spec, &fs_spec_args[0], num_fs_spec_args))
	return (UNDEF);
    if (UNDEF == (tokcon_inst = tokcon->init_proc(spec, "index.section.word.")) ||
	UNDEF == (contok_inst = contok->init_proc(spec, "ctype.0.")))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_vec_pvectxt");
    return 0;
}

int
vec_pvectxt (vec_file, pvec_file, inst)
char *vec_file;
char *pvec_file;
int inst;
{
    char inbuf[PATH_LEN], *token;
    int vec_fd, fs_fd;
    long line_len, did, num_phr, i;
    long con_list[MAX_TERMS], orig_num_terms, new_num_terms, num_terms;
    FILE *pvec_fp, *sp_fp;
    VEC vec;
    OCCINFO *occp;
    FDBK_STATS fs;

    PRINT_TRACE (2, print_string, "Trace: entering vec_pvectxt");

    /* Open files */
    if (UNDEF == (vec_fd = open_vector(vec_file, rmode)) ||
	!VALID_FILE (pvec_file) ||
	!VALID_FILE (synphr_file) ||
	NULL == (pvec_fp = fopen(pvec_file, "w")) ||
        NULL == (sp_fp = fopen(synphr_file, "r"))) {
        set_error (0, "Error opening file", "vec_pvectxt");
	return (UNDEF);
    }
    if (fs_flag &&
	UNDEF == (fs_fd = open_fdbkstats(fs_file, fsmode)))
	return(UNDEF);

    while (1 == read_vector(vec_fd, &vec)) {
	if(vec.id_num.id < global_start) continue;
        if(vec.id_num.id > global_end) break;

	/* Get phrases for this query */
	fscanf(sp_fp, "%ld\n", &did);
	assert(did == vec.id_num.id);
	num_phr = 0;
	while (NULL != fgets(inbuf, PATH_LEN, sp_fp)) {
	    line_len = strlen(inbuf) - 1;
	    assert(line_len > 0);
	    if (line_len == 1) {
		/* End of current document */
		assert(inbuf[0] == '!');
		break;
	    }
	    /* Handle this phrase */
	    assert(inbuf[0] == '(' && inbuf[line_len - 1] == ')');
	    token = inbuf + 1;
	    inbuf[line_len - 1] = '\0';
	    if (UNDEF == add_phrase(token, &num_phr))
		return(UNDEF);
	    assert(num_phr < MAX_PHRASES);
	}

	/* Select terms for pnorm query */
	orig_num_terms = vec.ctype_len[0];
	new_num_terms = (long) (reqd_percent * (float) orig_num_terms + 0.5);

	/* Use fdbk_stats if specified */
	if (fs_flag) {
	    fs.id_num = vec.id_num.id;
	    if (1 != seek_fdbkstats(fs_fd, &fs) ||
		UNDEF == read_fdbkstats(fs_fd, &fs))
		return(UNDEF);
	    for (occp = fs.occinfo, i = 0; i < orig_num_terms; occp++, i++) {
		assert(occp->ctype == 0 && occp->con == vec.con_wtp[i].con);
		vec.con_wtp[i].wt = /* #ret/df */
		    (occp->rel_ret + occp->nrel_ret) /
		    (float) (occp->rel_ret + occp->nrel_ret + occp->nret);
	    }
	}

	qsort(vec.con_wtp, orig_num_terms, sizeof(CON_WT), compare_wt);
	for (i = 0, num_terms = 0; 
	     (i < orig_num_terms) && (num_terms < new_num_terms); i++)
	    /* find phrase containing term; if term is not in phrase, toss */
	    add_term(vec.con_wtp[i].con, con_list, &num_terms, num_phr);

	/* Print pnorm query */
	fprintf(pvec_fp, "#q%ld = ", vec.id_num.id);
        if (num_terms > 1) fprintf(pvec_fp, "#and %4.1f (", pvalue);
	for (i = 0; i < num_terms - 1; i++) {
	    if (UNDEF == contok->proc(&(con_list[i]), &token, contok_inst))
		return(UNDEF);
	    fprintf(pvec_fp, "'%s', ", token);
	}
	if (UNDEF == contok->proc(&(con_list[i]), &token, contok_inst))
	    return(UNDEF);
	fprintf(pvec_fp, "'%s'%s;\n\n", token, (num_terms > 1) ? ")" : "");
    }

    if (UNDEF == close_vector(vec_fd))
        return (UNDEF);
    fclose(pvec_fp); fclose(sp_fp);
    if (fs_flag &&
	UNDEF == close_fdbkstats(fs_fd))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving vec_pvectxt");
    return 0;
}

int
close_vec_pvectxt (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_vec_pvectxt");

    if (UNDEF == tokcon->close_proc(tokcon_inst) ||
	UNDEF == contok->close_proc(contok_inst))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_vec_pvectxt");
    return (0);
}


/* 
 * Assumes that "tok" is in lisp output format (sans the brackets), i.e.,
 * it consists of a +ve no. of single words separated by single blanks. 
 * For example: "LONG TERM CARE CONFINEMENTS".
 */
static int
add_phrase(phrase_string, num)
char *phrase_string;
long *num;
{
    char *tok, *cptr;
    long num_words, con;

    /* Create the list of single terms for this phrase */
    for (tok = cptr = phrase_string, num_words = 0; *cptr; cptr++) {
	*cptr = (char) tolower(*cptr);
	if (*cptr == ' ') {
	    *cptr = '\0';
	    if (UNDEF == tokcon->proc(tok, &con, tokcon_inst))
		return(UNDEF);
	    if (UNDEF != con) 
		phrase_list[*num].words[num_words++] = con;
	    tok = cptr + 1;
	}
    }
    /* Process last single term */
    if (UNDEF == tokcon->proc(tok, &con, tokcon_inst))
	return(UNDEF);
    if (UNDEF != con) 
	phrase_list[*num].words[num_words++] = con;
    phrase_list[*num].num_words = num_words;

    (*num)++;
    return 1;
}

static void
add_term(con, conlist, num_terms, num_phr)
long con, *conlist, *num_terms, num_phr;
{
    int i, j;

    for (i = 0; i < num_phr; i++) {
	for (j = 0; j < phrase_list[i].num_words; j++)
	    if (con == phrase_list[i].words[j]) 
		break;
	if (j < phrase_list[i].num_words)
	    /* include other terms in that phrase in query */
	    for (j = 0; j < phrase_list[i].num_words; j++) {
		if (!included(phrase_list[i].words[j], conlist, *num_terms))
		    conlist[(*num_terms)++] = phrase_list[i].words[j];
	    }
    }

    return;
}

static int
included(con, conlist, numcon)
long con, *conlist, numcon;
{
    int i;

    for (i = 0; i < numcon; i++)
	if (conlist[i] == con)
	    return 1;
    return 0;
}


static int
compare_wt(c1, c2)
CON_WT *c1, *c2;
{
    if (c1->wt > c2->wt)
	return -1;
    if (c1->wt < c2->wt)
	return 1;
    return 0;
}
