#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/pobj_tr_vec.c,v 11.2 1993/02/03 16:54:11 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 print tr_vec object
 *1 print.obj.tr_vec
 *2 eval_mrr_trvec (in_file, out_file, inst)
 *3   char *in_file;
 *3   char *out_file;
 *3   int inst;
 *4 init_eval_mrr_trvec (spec, unused)
 *5   "print.tr_file"
 *5   "print.tr_file.rmode"
 *5   "print.trace"
 *4 close_eval_mrr_trvec (inst)
 *6 global_start,global_end used to indicate what range of cons will be printed
 *7 The tr_vec relation "in_file" (if not VALID_FILE, then use tr_file),
 *7 will be used to print all tr_vec entries in that file (modulo global_start,
 *7 global_end).  Tr_vec output to go into file "out_file" (if not VALID_FILE,
 *7 then stdout).
***********************************************************************/

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "tr_vec.h"
#include "retrieve.h"
#include "vector.h"
#include "buf.h"
#include "vector.h"

static char *tr_vec_file;
static long tr_vec_mode;
static PROC_INST get_query;
static int cutoff;

static SPEC_PARAM spec_args[] = {
    {"mrr.tr_file",     getspec_dbfile, (char *) &tr_vec_file},
    {"mrr.tr_file.rmode",getspec_filemode, (char *) &tr_vec_mode},
    {"mrr.get_query",    getspec_func, (char *) &get_query.ptab},
    {"mrr.cutoff",       getspec_int, (int *) &cutoff},
    TRACE_PARAM ("print.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static SM_BUF internal_output = {0, 0, (char *) 0};

float eval_mrr (tr_vec, output);

int
init_eval_mrr_trvec (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_eval_mrr_trvec");

    if (UNDEF == (get_query.inst =
                  get_query.ptab->init_proc (spec, NULL)))
		return UNDEF;
    PRINT_TRACE (2, print_string, "Trace: leaving init_eval_mrr_trvec");
    return (0);
}

int
eval_mrr_trvec (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    TR_VEC tr_vec;
    int status;
    char *final_tr_vec_file;
    FILE *output;
    SM_BUF output_buf;
    int tr_vec_index;              /* file descriptor for tr_vec_file */
	float recip_rank = 0;
	static char temp_buf[64];
	QUERY_VECTOR qvec;
	VEC *vec;
	int num_queries = 0;

    PRINT_TRACE (2, print_string, "Trace: entering eval_mrr_trvec");

    final_tr_vec_file = VALID_FILE (in_file) ? in_file : tr_vec_file;
    output = VALID_FILE (out_file) ? fopen (out_file, "w") : stdout;
    if (NULL == output)
        return (UNDEF);
    output_buf.size = 0;

    if (UNDEF == (tr_vec_index = open_tr_vec (final_tr_vec_file, tr_vec_mode)))
        return (UNDEF);

    while (1 == (status = get_query.ptab->proc (NULL, &qvec, get_query.inst))) {
	    if (qvec.qid < global_start)
		    continue;
		if (qvec.qid > global_end)
			break;
		num_queries++;

    	/* Get each tr_vec in turn */
		vec = (VEC*)(qvec.vector);
	    tr_vec.qid = vec->id_num.id;
		status = seek_tr_vec(tr_vec_index, &tr_vec);
		if (status == 0) {
			continue;
		}

    	if (UNDEF == status ||
			UNDEF == read_tr_vec(tr_vec_index, &tr_vec))
			return(UNDEF);

        output_buf.end = 0;
        recip_rank += eval_mrr (&tr_vec, &output_buf);
        (void) fwrite (output_buf.buf, 1, output_buf.end, output);
    }

	snprintf(temp_buf, sizeof(temp_buf), "MRR = %f/%d = %f\n", recip_rank, num_queries, recip_rank/(float)num_queries);
	if (UNDEF == add_buf_string (temp_buf, &output_buf))
		return 0;
    (void) fwrite (output_buf.buf, 1, output_buf.end, output);

    if (UNDEF == close_tr_vec (tr_vec_index))
        return (UNDEF);

    if (output != stdin)
        (void) fclose (output);

    PRINT_TRACE (2, print_string, "Trace: leaving eval_mrr_trvec");
    return (status);
}


int
close_eval_mrr_trvec (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_eval_mrr_trvec");

    if (UNDEF == get_query.ptab->close_proc (get_query.inst))
		return UNDEF;
    PRINT_TRACE (2, print_string, "Trace: leaving close_eval_mrr_trvec");
    return (0);
}


float eval_mrr (tr_vec, output)
TR_VEC *tr_vec;
SM_BUF *output;
{
    SM_BUF *out_p;
    TR_TUP *tr_tup;
    char temp_buf[PATH_LEN];
	int min_rank;
	float recip_rank;

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }
	min_rank = tr_vec->num_tr+1;
    for (tr_tup = tr_vec->tr;
         tr_tup < &tr_vec->tr[tr_vec->num_tr];
         tr_tup++) {
		if (tr_tup->rel && tr_tup->rank <= cutoff) {
			if (tr_tup->rank < min_rank) {
				min_rank = tr_tup->rank;
			}
		}
	}

	if (min_rank <= tr_vec->num_tr)
		recip_rank = 1/((float)min_rank);
	else
		recip_rank = 0;

    (void) sprintf (temp_buf, "%d %f\n", tr_vec->qid, recip_rank);
	if (UNDEF == add_buf_string (temp_buf, out_p))
		return 0;
    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
	return recip_rank;
}

