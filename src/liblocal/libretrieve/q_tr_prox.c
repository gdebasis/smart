#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libretrieve/q_tr_proc.c,v 11.2 1993/02/03 16:52:42 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Compute similarities between a query and a given set of docs
 *1 local.retrieve.q_tr.prox
 *2 q_tr_proc (seen_vec, tr_vec, inst)
 *3   TR_VEC *seen_vec;
 *3   TR_VEC *tr_vec;
 *3   int inst;
 *4 init_q_tr_proc (spec, unused)
 *5   "retrieve.qtr.trace
 *5   "retrieve.qtr.qd_tr
 *5   "retrieve.qtr
 *4 close_q_tr_proc (inst)
 *7 For the documents in seen_vec, compute sim based on proximity
 *9
***********************************************************************/

#include <limits.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"
#include "tr_vec.h"
#include "retrieve.h"

static char *vec_file;
static long vec_file_mode;
static long num_wanted;
static float ctype_weight;
static SPEC_PARAM spec_args[] = {
    {"retrieve.did_convec.vec_file",getspec_dbfile,(char *) &vec_file},
    {"retrieve.did_convec.doc_file.rmode",getspec_filemode,(char *) &vec_file_mode},
    {"retrieve.num_wanted", getspec_long, (char *) &num_wanted},
    {"ctype.3.sim_ctype_weight", getspec_float, (char *) &ctype_weight},
    TRACE_PARAM ("retrieve.qtr.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/*
 * WARNING: this struct is shared with did_convec.c
 * and should eventually go in a .h file
 */
typedef struct {
    long con;
    unsigned short sect;
    unsigned short sent;
} CON_SECT_SENT;

typedef struct {
    long con1;
    long con2;
    float wt;
} PROX;

static int num_inst = 0;
static int contok_inst, tokcon_inst;

static long max_num_conss = 0;
static long num_conss;
static CON_SECT_SENT *conss;
static int vec_fd;

static int qid_vec_inst;
static long max_num_top_results = 0;
static TOP_RESULT *top_results;

static int comp_top_sim(), comp_long();
int init_contok_dict_noinfo(), contok_dict_noinfo(), close_contok_dict_noinfo();
int init_tokcon_dict_niss(), tokcon_dict_niss(), close_tokcon_dict_niss();

int
init_q_tr_prox (spec, unused)
SPEC *spec;
char *unused;
{
    if (num_inst++) {
        PRINT_TRACE (2, print_string,
                     "Trace: entering/leaving init_q_tr_prox");
        return (0);
    }

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_q_tr_prox");

    if (UNDEF == (vec_fd = open_vector (vec_file, vec_file_mode)))
        return (UNDEF);

    /* Call all initialization procedures */
    if (UNDEF == (contok_inst = init_contok_dict_noinfo(spec, "ctype.3.")) ||
        UNDEF == (tokcon_inst = init_tokcon_dict_niss(spec,
						      "index.section.1.word.")) ||
	UNDEF == (qid_vec_inst = init_qid_vec (spec, (char *) NULL)))
	return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving init_q_tr_prox");
    return (num_inst);
}

int
q_tr_prox (seen_vec, results, inst)
TR_VEC *seen_vec;
RESULT *results;
int inst;
{
    long i, j, k, l;
    VEC	qvec, convec;
    CON_WT *con_wtp;
    PROX *prox;
    char *term_pair, *term1, *term2;
    long *uniq_con, num_uniq = 0;

    PRINT_TRACE (2, print_string, "Trace: entering q_tr_prox");
    PRINT_TRACE (6, print_tr_vec, seen_vec);

    /*
     * Get the query vector
     */
    if (UNDEF == qid_vec(&seen_vec->qid, &qvec, qid_vec_inst) ||
	UNDEF == save_vec(&qvec))
	return UNDEF;

    /* if the query has no proximity pairs (ctype 3) then done */
    if (qvec.num_ctype < 4) {
        (void) free_vec(&qvec);
        return (1);
    }

    /* Reserve space for results */
    if (seen_vec->num_tr > max_num_top_results) {
        if (max_num_top_results != 0)
            (void) free ((char *) top_results);
        max_num_top_results = seen_vec->num_tr;
        if (NULL == (top_results = (TOP_RESULT *)
                     malloc (max_num_top_results * sizeof (TOP_RESULT))))
            return (UNDEF);
    }

    con_wtp = qvec.con_wtp + qvec.ctype_len[0] +
                  qvec.ctype_len[1] + qvec.ctype_len[2];
    if (NULL == (prox = (PROX *)
                 malloc ((unsigned) qvec.ctype_len[3] * sizeof (PROX))) ||
	NULL == (uniq_con = (long *)
		 malloc ((unsigned) qvec.ctype_len[3] * 2 * sizeof (long))))
        return (UNDEF);
    for (i = 0; i < qvec.ctype_len[3]; i++, con_wtp++) {
        /* Get the term-pair corresponding to this ctype 2 term */
        if (UNDEF == contok_dict_noinfo(&(con_wtp->con), &term_pair,
					contok_inst))
	    return(UNDEF);
        term2 = term1 = term_pair;
        while (*term2 != ' ') term2++;
        *term2 = '\0';
        term2++;
        if (UNDEF == tokcon_dict_niss(term1, &prox[i].con1, tokcon_inst) ||
            UNDEF == tokcon_dict_niss(term2, &prox[i].con2, tokcon_inst))
            return(UNDEF);
	prox[i].wt = con_wtp->wt * ctype_weight;
	uniq_con[num_uniq++] = prox[i].con1;
	uniq_con[num_uniq++] = prox[i].con2;
    }

    qsort((char *) uniq_con, (int) num_uniq, sizeof (long), comp_long);
    for (i = 1; i < num_uniq; i++) {
        if (uniq_con[i] == uniq_con[i-1]) {
	    for (j = i+1; j < num_uniq; j++)
	        uniq_con[j-1] = uniq_con[j];
	    i--;
	    num_uniq--;
	}
    }

    for (i = 0; i < seen_vec->num_tr; i++) {
        top_results[i].did = seen_vec->tr[i].did;
	top_results[i].sim = seen_vec->tr[i].sim;
        convec.id_num.id = seen_vec->tr[i].did;
        EXT_NONE(convec.id_num.ext);
        if (1 != seek_vector (vec_fd, &convec) ||
            1 != read_vector (vec_fd, &convec))
                return (UNDEF);

	if (convec.num_conwt > max_num_conss) {
	    if (max_num_conss != 0)
	        (void) free ((char *) conss);
	    max_num_conss += convec.num_conwt;
	    if (NULL == (conss = (CON_SECT_SENT *)
			 malloc (max_num_conss * sizeof (CON_SECT_SENT))))
	        return (UNDEF);
	}

	num_conss = 0;
	for (j = 0; j < convec.num_conwt; j++) {
	    for (k = 0;
		 k < num_uniq && uniq_con[k] < convec.con_wtp[j].con;
		 k++);
	    if ( k < num_uniq && uniq_con[k] == convec.con_wtp[j].con)
	      conss[num_conss++] = *((CON_SECT_SENT *) &convec.con_wtp[j]);
	}

	for (j = 0; j < qvec.ctype_len[3]; j++) {
	    long dist = 0, num_times = 0;
	    long tf1 = 0, tf2 = 0, tmp;

	    for (k = 0; k < num_conss; k++) {
	        if (prox[j].con1 == conss[k].con)
		    tf1++;
	        if (prox[j].con2 == conss[k].con)
		    tf2++;
	    }

	    if (tf1 == 0 || tf2 == 0)
	        continue;

	    if (tf1 > tf2) {
		tmp = prox[j].con1;
		prox[j].con1 = prox[j].con2;
		prox[j].con2 = tmp;
	    }

	    for (k = 0; k < num_conss; k++) {
	        if (prox[j].con1 == conss[k].con) {
		    long top_dist = LONG_MAX, bot_dist = LONG_MAX;

		    for (l = k-1;
			 l > 0 && conss[l].sect == conss[k].sect
			   && conss[l].sent >= conss[k].sent - 3
			   && conss[l].con != prox[j].con1;
			 l--) {
		        if (conss[l].con == prox[j].con2) {
			    top_dist = conss[k].sent-conss[l].sent;
			    break;
			}
		    }
		    for (l = k+1;
			 l < num_conss && conss[l].sect == conss[k].sect
			   && conss[l].sent <= conss[k].sent + 3
			   && conss[l].con != prox[j].con1;
			 l++) {
		        if (conss[l].con == prox[j].con2) {
			    bot_dist = conss[l].sent-conss[k].sent;
			    break;
			}
		    }

		    if (top_dist != LONG_MAX || bot_dist != LONG_MAX) {
		        num_times++;
			dist += MIN(top_dist, bot_dist);
		    }
	        }
	    }

	    if (num_times > 0) {
	        top_results[i].sim +=
		    prox[j].wt * (1.0 - 0.3 * (float) dist/(float) num_times) *
		    (float) (1.0 + log ((double) num_times));
	    }
	}
    }

    (void) free_vec(&qvec);
    (void) free ((char *) prox);
    (void) free ((char *) uniq_con);
	      
    results->qid = seen_vec->qid;
    results->top_results = top_results;
    results->num_top_results = seen_vec->num_tr;
    results->num_full_results = 0;

    qsort ((char *) results->top_results,
           (int) results->num_top_results,
           sizeof (TOP_RESULT),
           comp_top_sim);
    results->num_top_results = MIN(results->num_top_results, num_wanted);

    PRINT_TRACE (4, print_top_results, results);
    PRINT_TRACE (2, print_string, "Trace: leaving q_tr_prox");
    return (1);
}


int
close_q_tr_prox (inst)
int inst;
{
    if (--num_inst) {
        PRINT_TRACE (2, print_string,
                     "Trace: entering/leaving close_q_tr_prox");
        return (0);
    }

    PRINT_TRACE (2, print_string, "Trace: entering close_q_tr_prox");

    if (UNDEF == close_qid_vec(qid_vec_inst))
	return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving close_q_tr_prox");
    return (0);
}

static int
comp_top_sim (ptr1, ptr2)
TOP_RESULT *ptr1;
TOP_RESULT *ptr2;
{
    if (ptr1->sim > ptr2->sim ||
        (ptr1->sim == ptr2->sim &&
         ptr1->did > ptr2->did))
        return (-1);
    return (1);
}

static int
comp_long(i, j)
long *i;
long *j;
{
    if (*i > *j)
        return (1);
    if (*i < *j)
        return (-1);
    return (0);
}
