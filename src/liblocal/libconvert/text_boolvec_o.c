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
 *2 text_boolvec_obj (text_file, vec_file, inst)
 *3   char *text_file;
 *3   char *vec_file;
 *3   int inst;

 *4 init_text_boolvec_obj (spec, unused)
 *4 close_text_boolvec_obj (inst)

 *7 Input format: each line contains the conjunctive normal form filter 
 *7 for one query in the following format:
 *7 <qid> term1 term2 (term3 term4)
 *7 This is interpreted as term1 AND term2 AND (term3 OR term4).
 *7 For ease of processing, phrases must be enclosed in "" and must 
 *7 follow all single terms within a clause.
 *7 For the output vector
 *7     num_ctype = 2 * number of clauses in the filter
 *7  	ctype 0: single words in clause 1
 *7	ctype 1: phrases in clause 1
 *7	ctype 2: single words in clause 2
 *7     ctype 3: phrases in clause 2
 *7     etc.
 *7 REASONABLE ASSUMPTION: only words and phrases are used.
 *9 WARNING: Assumes input line is well-behaved i.e. error condition 
 *9 checking is almost non-existent.
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

static PROC_TAB *w_tokcon, *p_tokcon, *contok;
static long rwmode;
static SPEC_PARAM spec_args[] = {
    {"convert.text_bool.word.token_to_con", getspec_func, (char *)&w_tokcon},
    {"convert.text_bool.phrase.token_to_con", getspec_func, (char *)&p_tokcon},
    {"convert.text_bool.con_to_token", getspec_func, (char *)&contok},
    {"convert.text_bool.vec_file.rwmode", getspec_filemode, (char *)&rwmode},
    TRACE_PARAM ("convert.text_bool.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int w_inst, p_inst, w_idf_inst, p_idf_inst, contok_inst;

static int get_phrase_tok(), comp_con();


int
init_text_boolvec_obj (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
	return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_text_boolvec_obj");

    if (UNDEF == (w_inst=w_tokcon->init_proc(spec, "index.section.1.word.")) ||
	UNDEF == (p_inst = p_tokcon->init_proc(spec, "index.section.1.phrase.")) ||
	UNDEF == (contok_inst = contok->init_proc(spec, "ctype.0.")) ||
	UNDEF == (w_idf_inst = init_con_cw_idf(spec, "ctype.0.")) ||
	UNDEF == (p_idf_inst = init_con_cw_idf(spec, "ctype.1.")))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_text_boolvec_obj");
    return 0;
}

int
text_boolvec_obj (text_file, vec_file, inst)
char *text_file;
char *vec_file;
int inst;
{
    char inbuf[PATH_LEN], phrase_buf[128], *token, *c, saved_c, phrase_flag;
    int vec_fd;
    long qid, ctype, num_conwt, max_ctype, max_conwt, start_ctype, con;
    long *ctype_len;
    FILE *in_fp;
    CON_WT *con_wtp;
    VEC vec;

    PRINT_TRACE (2, print_string, "Trace: entering text_boolvec_obj");

    /* Open files */
    if (VALID_FILE (text_file)) {
        if (NULL == (in_fp = fopen (text_file, "r")))
            return (UNDEF);
    }
    else
        in_fp = stdin;
    if (UNDEF == (vec_fd = open_vector(vec_file, rwmode)))
	return (UNDEF);

    max_ctype = 10;
    max_conwt = 30;
    if (NULL == (ctype_len = Malloc(max_ctype, long)) ||
	NULL == (con_wtp = Malloc(max_conwt, CON_WT)))
	return(UNDEF);

    /* WARNING: Assumes input line is well-behaved i.e. error condition 
     * checking is almost non-existent.
     */
    while (NULL != fgets (inbuf, PATH_LEN, in_fp)) {
	qid = atol(inbuf);
	if (qid < global_start) continue;
	if (qid > global_end) break;
	vec.id_num.id = qid; EXT_NONE(vec.id_num.ext);

	c = inbuf;
	/* skip over the query id */
	while (isdigit(*c)) c++;

	ctype = num_conwt = 0;
	while (*c) {
	    while (isspace(*c)) c++;
	    if (ctype+1 >= max_ctype) {
		max_ctype *= 2;
		if (NULL == (ctype_len = Realloc(ctype_len, max_ctype, long)))
		    return(UNDEF);
	    }
	    if (*c == '(') {
		/* OR clause */
		c++; 
		ctype_len[ctype] = 0;
		start_ctype = num_conwt;
		phrase_flag = 0;
		while (*c != ')') {
		    while (isspace(*c)) c++;
		    if (*c == '"') {
			if (!phrase_flag) {
			    phrase_flag = 1;
			    qsort(con_wtp + start_ctype, ctype_len[ctype], 
				  sizeof(CON_WT), comp_con);
			    ctype_len[++ctype] = 0;
			    start_ctype = num_conwt;
			}			
			if (UNDEF == get_phrase_tok(&c, &phrase_buf[0]) ||
			    UNDEF == p_tokcon->proc(&phrase_buf[0], &con, 
						    p_inst))
			    return(UNDEF);
			if (num_conwt >= max_conwt) {
			    max_conwt *= 2;
			    if (NULL == (con_wtp = Realloc(con_wtp, max_conwt,
							   CON_WT)))
				return(UNDEF);
			}
			con_wtp[num_conwt].con = con;
			if (UNDEF == con_cw_idf(&con, &(con_wtp[num_conwt].wt),
						p_idf_inst))
			    return(UNDEF);
			num_conwt++;
			ctype_len[ctype]++;
		    }
		    else {
			if (phrase_flag) {
			    set_error(SM_INCON_ERR, "Single word unexpected",
				      "text_boolvec_obj");
			    return(UNDEF);
			}
			token = c++;
			while (!isspace(*c) && *c != ')') {
			    *c = tolower(*c);
			    c++;
			}
			saved_c = *c;
			*c = '\0';
			if (UNDEF == w_tokcon->proc(token, &con, w_inst))
			    return(UNDEF);
			*c = saved_c;
			if (num_conwt >= max_conwt) {
			    max_conwt *= 2;
			    if (NULL == (con_wtp = Realloc(con_wtp, max_conwt, 
							   CON_WT)))
				return(UNDEF);
			}
			con_wtp[num_conwt].con = con;
			if (UNDEF == con_cw_idf(&con, &(con_wtp[num_conwt].wt),
						w_idf_inst))
			    return(UNDEF);
			num_conwt++;
			ctype_len[ctype]++;
		    }
		}
		qsort(con_wtp + start_ctype, ctype_len[ctype], sizeof(CON_WT), 
		      comp_con);		
		if (!phrase_flag) ctype_len[++ctype] = 0; 
		ctype++; 
		c++;
	    }
	    else if (*c == '"') {
		/* single phrase AND clause */
		if (UNDEF == get_phrase_tok(&c, &phrase_buf[0]) ||
		    UNDEF == p_tokcon->proc(&phrase_buf[0], &con, p_inst))
		    return(UNDEF);
		if (num_conwt >= max_conwt) {
		    max_conwt *= 2;
		    if (NULL == (con_wtp=Realloc(con_wtp, max_conwt, CON_WT)))
			return(UNDEF);
		}
		con_wtp[num_conwt].con = con;
		if (UNDEF == con_cw_idf(&con, &(con_wtp[num_conwt].wt), 
					p_idf_inst))
                    return(UNDEF);
		num_conwt++;
		ctype_len[ctype++] = 0;
		ctype_len[ctype++] = 1;
	    }
	    else if (*c) {
		/* single word AND clause */
		token = c;
		while (*c && !isspace(*c)) {
		    *c = tolower(*c);
		    c++;
		}
		saved_c = *c;
		*c = '\0';
		if (UNDEF == w_tokcon->proc(token, &con, w_inst))
		    return(UNDEF);
		*c = saved_c;
		if (num_conwt >= max_conwt) {
		    max_conwt *= 2;
		    if (NULL == (con_wtp=Realloc(con_wtp, max_conwt, CON_WT)))
			return(UNDEF);
		}
		con_wtp[num_conwt].con = con;
		if (UNDEF == con_cw_idf(&con, &(con_wtp[num_conwt].wt), 
					w_idf_inst))
                    return(UNDEF);
		num_conwt++;
		ctype_len[ctype++] = 1;
		ctype_len[ctype++] = 0;
	    }
	}

	vec.num_ctype = ctype;
	vec.ctype_len = ctype_len;
	vec.num_conwt = num_conwt;
	vec.con_wtp = con_wtp;
	if (UNDEF == seek_vector(vec_fd, &vec) ||
	    UNDEF == write_vector(vec_fd, &vec))
	    return(UNDEF);
    }

    if (UNDEF == close_vector(vec_fd))
        return (UNDEF);
    fclose(in_fp); 

    PRINT_TRACE (2, print_string, "Trace: leaving text_boolvec_obj");
    return 0;
}

int
close_text_boolvec_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_text_boolvec_obj");

    if (UNDEF == w_tokcon->close_proc(w_inst) ||
	UNDEF == p_tokcon->close_proc(p_inst) ||
	UNDEF == contok->close_proc(contok_inst) ||
	UNDEF == close_con_cw_idf(w_idf_inst) ||
	UNDEF == close_con_cw_idf(p_idf_inst))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_text_boolvec_obj");
    return (0);
}


/* At entry: *c points to the first " of the phrase.
 * At exit: *c points just past the last " of the phrase.
 */
static int
get_phrase_tok(c, phrase_buf)
char **c, *phrase_buf;
{
    char *token, *w_stem, *buf_ptr;
    long con;

    token = ++(*c);
    /* get stem for first word */
    while (**c && !isspace(**c)) {
	**c = tolower(**c);
	(*c)++;
    }
    assert(**c);
    **c = '\0';
    if (UNDEF == w_tokcon->proc(token, &con, w_inst) ||
	UNDEF == contok->proc(&con, &w_stem, contok_inst))
	return(UNDEF);
    strcpy(phrase_buf, w_stem);
    buf_ptr = phrase_buf + strlen(w_stem);
    *buf_ptr++ = ' ';

    /* get stem for second word */
    (*c)++;
    while (**c && isspace(**c)) (*c)++;
    token = *c;
    while (**c && **c != '"') {
	**c = tolower(**c);
	(*c)++;
    }
    assert(**c == '"');
    **c = '\0'; 
    if (UNDEF == w_tokcon->proc(token, &con, w_inst) 
	UNDEF == contok->proc(&con, &w_stem, contok_inst))
	return(UNDEF);
    strcat(buf_ptr, w_stem);
    (*c)++;

    return (1);
}

static int
comp_con(c1, c2)
CON_WT *c1, *c2;
{
    if (c1->con > c2->con)
	return 1;
    if (c1->con < c2->con)
	return -1;
    return 0;
}
