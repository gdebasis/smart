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
 *2 vec_filtervec (vec_file, pvec_file, inst)
 *3   char *vec_file;
 *3   char *pvec_file;
 *3   int inst;

 *4 init_vec_filtervec (spec, unused)
 *4 close_vec_filtervec (inst)
 *7 Adapted from vec_pvectxt, vec_pvectxt_dynamic.
 *7 Instead of constructing a pnorm filter, a new query vector is generated.
 *7 The new vector contains exactly the terms in the pnorm filter. The
 *7 weight assigned to a term depends inversely on the number of iterations 
 *7 it took for the term to be added to the corresponding filter.

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
static long fs_flag, rmode;
static float reqd_percent;
static PROC_TAB *tokcon;
static SPEC_PARAM spec_args[] = {
    {"convert.vec_pvectxt.synphr_file", getspec_dbfile, (char *)&synphr_file},
    {"convert.vec_pvectxt.fs_flag", getspec_long, (char *)&fs_flag},
    {"convert.vec_pvectxt.reqd_percent", getspec_float, (char *)&reqd_percent},
    {"convert.vec_pvectxt.token_to_con", getspec_func, (char *)&tokcon},
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
static int tokcon_inst;

static int add_phrase(), included(), compare_wt(), compare_con();
static void add_term(), form_filter();


int
init_vec_filtervec (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
	return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_vec_filtervec");

    if (fs_flag &&
	UNDEF == lookup_spec (spec, &fs_spec_args[0], num_fs_spec_args))
	return (UNDEF);
    if (UNDEF == (tokcon_inst=tokcon->init_proc(spec, "index.section.word.")))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_vec_filtervec");
    return 0;
}

int
vec_filtervec (vec_file, fvec_file, inst)
char *vec_file;
char *fvec_file;
int inst;
{
    char inbuf[PATH_LEN], *token;
    int in_fd, out_fd, fs_fd;
    long line_len, did, num_phr, i;
    long ctype_len, orig_num_terms, new_num_terms, num_terms;
    FILE *sp_fp;
    CON_WT con_list[MAX_TERMS];
    VEC vec, newvec;
    OCCINFO *occp;
    FDBK_STATS fs;

    PRINT_TRACE (2, print_string, "Trace: entering vec_filtervec");

    /* Open files */
    if (UNDEF == (in_fd = open_vector(vec_file, rmode)) ||
	UNDEF == (out_fd = open_vector(fvec_file, SRDWR|SCREATE)) ||
	!VALID_FILE (synphr_file) ||
        NULL == (sp_fp = fopen(synphr_file, "r"))) {
        set_error (0, "Error opening file", "vec_filtervec");
	return (UNDEF);
    }
    if (fs_flag &&
	UNDEF == (fs_fd = open_fdbkstats(fs_file, fsmode)))
	return(UNDEF);

    while (1 == read_vector(in_fd, &vec)) {
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

	/* Get fdbk_stats if specified */
	if (fs_flag) {
	    fs.id_num = vec.id_num.id;
	    if (1 != seek_fdbkstats(fs_fd, &fs) ||
		UNDEF == read_fdbkstats(fs_fd, &fs))
		return(UNDEF);
	    for (occp = fs.occinfo, i = 0; i < vec.ctype_len[0]; occp++, i++) {
		assert(occp->ctype == 0 && occp->con == vec.con_wtp[i].con);
		vec.con_wtp[i].wt = /* #ret/df */
		    (occp->rel_ret + occp->nrel_ret) /
		    (float) (occp->rel_ret + occp->nrel_ret + occp->nret);
	    }
	}

	/* Select terms for pnorm query */
	orig_num_terms = vec.ctype_len[0];
	new_num_terms = (long) (reqd_percent * (float) orig_num_terms + 0.5);
	qsort(vec.con_wtp, orig_num_terms, sizeof(CON_WT), compare_wt);
	for (i = 0, num_terms = 0; 
	     (i < orig_num_terms) && (num_terms < new_num_terms); i++)
	    /* find phrase containing term; if term is not in phrase, toss */
	    add_term(vec.con_wtp[i].con, con_list, &num_terms, num_phr);

	/* Form new query vector */
	newvec.id_num = vec.id_num;
	newvec.num_ctype = 1;
	newvec.ctype_len = &ctype_len;
	newvec.num_conwt = newvec.ctype_len[0] = num_terms;
	if (NULL == (newvec.con_wtp = Malloc(num_terms, CON_WT)))
	    return(UNDEF);
	form_filter(con_list, &newvec);
	if (UNDEF == seek_vector(out_fd, &newvec) ||
	    1 != write_vector(out_fd, &newvec))
	    return(UNDEF);
	Free(newvec.con_wtp);
    }

    if (UNDEF == close_vector(in_fd) ||
	UNDEF == close_vector(out_fd))
        return (UNDEF);
    fclose(sp_fp);
    if (fs_flag &&
	UNDEF == close_fdbkstats(fs_fd))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving vec_filtervec");
    return 0;
}

int
close_vec_filtervec (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_vec_filtervec");

    if (UNDEF == tokcon->close_proc(tokcon_inst))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_vec_filtervec");
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
long con, *num_terms, num_phr;
CON_WT *conlist;
{
    static int batch_no;
    int i, j;

    if (*num_terms == 0) batch_no = 1;
    else batch_no++;

    for (i = 0; i < num_phr; i++) {
	for (j = 0; j < phrase_list[i].num_words; j++)
	    if (con == phrase_list[i].words[j]) 
		break;
	if (j < phrase_list[i].num_words)
	    /* include other terms in that phrase in query */
	    for (j = 0; j < phrase_list[i].num_words; j++) {
		if (!included(phrase_list[i].words[j], conlist, *num_terms)) {
		    conlist[*num_terms].con = phrase_list[i].words[j];
		    conlist[*num_terms].wt = batch_no;
		    (*num_terms)++;
		}
	    }
    }

    return;
}

static void
form_filter(conlist, vec)
CON_WT *conlist;
VEC *vec;
{
    int i, iter_no;

    qsort(conlist, vec->num_conwt, sizeof(CON_WT), compare_wt);
    for (iter_no = 1, i = 0; i < vec->num_conwt; i++) {
	if (i < vec->num_conwt - 1 &&
	    conlist[i].wt != conlist[i+1].wt) {
	    conlist[i].wt = iter_no;
	    iter_no *= 2;
	}
	else conlist[i].wt = iter_no;
    }
    qsort(conlist, vec->num_conwt, sizeof(CON_WT), compare_con);
    bcopy(conlist, vec->con_wtp, vec->num_conwt * sizeof(CON_WT));
    return;
}


static int
included(con, conlist, numcon)
long con, numcon;
CON_WT *conlist;
{
    int i;

    for (i = 0; i < numcon; i++)
	if (conlist[i].con == con)
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

static int
compare_con(c1, c2)
CON_WT *c1, *c2;
{
    if (c1->con > c2->con)
	return 1;
    if (c1->con < c2->con)
	return -1;
    return 0;
}



