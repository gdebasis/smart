#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_df_stats.c,v 11.2 1993/02/03 16:54:12 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 print vector object
 *1 print.obj.vec
 *2 p_df_stats (unused, out_file, inst)
 *3   char *unused;
 *3   char *out_file;
 *3   int inst;
 *4 init_p_df_stats (spec, unused)
 *5   "print.doc_file"
 *5   "print.doc_file.rmode"
 *5   "print.trace"
 *4 close_p_df_stats (inst)
 *6 global_start,global_end used to indicate what range of cons will be printed
 *7 Vec output to go into file "out_file" (if not VALID_FILE, then stdout).
***********************************************************************/

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "context.h"
#include "trace.h"
#include "vector.h"
#include "docdesc.h"

static char *idf_vec_file;
static char *tf_vec_file;
static char *opt_vec_file;
static long vec_mode;
static PROC_TAB *query_weight;

static SPEC_PARAM spec_args[] = {
    {"print.idf_vec_file", getspec_dbfile, (char *) &idf_vec_file},
    {"print.tf_vec_file",  getspec_dbfile, (char *) &tf_vec_file},
    {"print.opt_vec_file", getspec_dbfile, (char *) &opt_vec_file},
    {"print.rmode",getspec_filemode,       (char *) &vec_mode},
    {"print.query.weight", getspec_func,   (char *) &query_weight},
    TRACE_PARAM ("print.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static SM_INDEX_DOCDESC doc_desc;    /* New style doc description */
static int *con_cw_cf_inst;
static int query_weight_inst;
static int idf_index;              /* file descriptor for idf_file */
static int tf_index;               /* file descriptor for tf_file */
static int opt_index;              /* file descriptor for opt_file */

int
init_p_df_stats (spec, unused)
SPEC *spec;
char *unused;
{
    char param_prefix[PATH_LEN];
    CONTEXT old_context;
    long i;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    if (UNDEF == lookup_spec_docdesc (spec, &doc_desc))
	return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: entering init_p_df_stats");

    old_context = get_context();
    set_context (CTXT_QUERY);
    if (UNDEF == (query_weight_inst =
		  query_weight->init_proc (spec, (char *) NULL)))
	return UNDEF;
    set_context (old_context);

    if (UNDEF == (idf_index = open_vector (idf_vec_file, vec_mode)) ||
	UNDEF == (tf_index = open_vector (tf_vec_file, vec_mode)) ||
	UNDEF == (opt_index = open_vector (opt_vec_file, vec_mode)))
        return (UNDEF);

    if (NULL == (con_cw_cf_inst = (int *)
		 malloc ((unsigned) doc_desc.num_ctypes * sizeof (int))))
	return UNDEF;
    for (i = 0; i < doc_desc.num_ctypes; i++) {
	(void) sprintf (param_prefix, "%s%ld.", "index.ctype.", i);
	if (UNDEF == (con_cw_cf_inst[i] = init_con_cw_cf(spec,
							 param_prefix)))
	    return UNDEF;
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_p_df_stats");
    return (0);
}

int
p_df_stats (unused, out_file, inst)
char *unused;
char *out_file;
int inst;
{
    VEC idf_vec;
    VEC idf_norm_vec;
    VEC tf_vec;
    VEC opt_vec;
    int status;
    FILE *output;
    long ctype, i;
    float freq;

    PRINT_TRACE (2, print_string, "Trace: entering p_df_stats");

    output = VALID_FILE (out_file) ? fopen (out_file, "w") : stdout;
    if (NULL == output)
        return (UNDEF);

    /* Get each document in turn */
    if (global_start > 0) {
        idf_vec.id_num.id = global_start;
	EXT_NONE(idf_vec.id_num.ext);
        tf_vec.id_num.id = global_start;
	EXT_NONE(tf_vec.id_num.ext);
        opt_vec.id_num.id = global_start;
	EXT_NONE(opt_vec.id_num.ext);
        if (UNDEF == seek_vector (idf_index, &idf_vec) ||
	    UNDEF == seek_vector (tf_index, &tf_vec) ||
	    UNDEF == seek_vector (opt_index, &opt_vec)) {
            return (UNDEF);
        }
    }

    while (1 == (status = read_vector (idf_index, &idf_vec)) &&
	   1 == (status = read_vector (tf_index, &tf_vec)) &&
	   1 == (status = read_vector (opt_index, &opt_vec)) &&
	   idf_vec.id_num.id <= global_end) { /* all ids are the same */
	CON_WT *idf_conwtp, *idf_norm_conwtp, *tf_conwtp, *opt_conwtp;

	assert(idf_vec.id_num.id == tf_vec.id_num.id &&
	       tf_vec.id_num.id == opt_vec.id_num.id &&
	       idf_vec.num_ctype == tf_vec.num_ctype &&
	       tf_vec.num_ctype == opt_vec.num_ctype &&
	       idf_vec.num_conwt == tf_vec.num_conwt &&
	       tf_vec.num_conwt == opt_vec.num_conwt);

	idf_conwtp = idf_vec.con_wtp;
	tf_conwtp = tf_vec.con_wtp;
	opt_conwtp = opt_vec.con_wtp;
	for (ctype = 0; ctype < idf_vec.num_ctype; ctype++) {
	    assert(idf_vec.ctype_len[ctype] == tf_vec.ctype_len[ctype] &&
		   tf_vec.ctype_len[ctype]  == opt_vec.ctype_len[ctype]);
	    for (i = 0; i < idf_vec.ctype_len[ctype]; i++) {
		assert(idf_conwtp->con == tf_conwtp->con &&
		       tf_conwtp->con == opt_conwtp->con);
		opt_conwtp->wt /= tf_conwtp->wt;
		idf_conwtp++;
		opt_conwtp++;
		tf_conwtp++;
	    }
	}
	/* use weight_cos with nnn weighting to cosine normalize */
	if (UNDEF == query_weight->proc (&opt_vec, (VEC *)NULL,
					 query_weight_inst) ||
	    UNDEF == query_weight->proc (&idf_vec, &idf_norm_vec,
					 query_weight_inst))
	    return (UNDEF);

	idf_conwtp = idf_vec.con_wtp;
	idf_norm_conwtp = idf_norm_vec.con_wtp;
	tf_conwtp = tf_vec.con_wtp;
	opt_conwtp = opt_vec.con_wtp;
	for (ctype = 0; ctype < idf_vec.num_ctype; ctype++) {
	    for (i = 0; i < idf_vec.ctype_len[ctype]; i++) {
		if (UNDEF == con_cw_cf (&idf_conwtp->con, &freq,
					con_cw_cf_inst[ctype]))
		    return (UNDEF);
		fprintf(output, "%ld\t%ld\t%ld\t%ld\t%.4f\t%.4f\n",
			idf_vec.id_num.id,
			idf_conwtp->con,
			ctype, (long) freq,
			/* make opt vector same Eucled. len as query vector */
			idf_conwtp->wt,
			opt_conwtp->wt * idf_conwtp->wt/idf_norm_conwtp->wt);
		idf_conwtp++;
		idf_norm_conwtp++;
		opt_conwtp++;
		tf_conwtp++;
	    }
	}
    }

    if (output != stdin)
        (void) fclose (output);

    PRINT_TRACE (2, print_string, "Trace: leaving p_df_stats");
    return (status);
}


int
close_p_df_stats (inst)
int inst;
{
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_p_df_stats");

    if (UNDEF == query_weight->close_proc (query_weight_inst))
	return UNDEF;

    if (UNDEF == close_vector (idf_index) ||
	UNDEF == close_vector (tf_index) ||
	UNDEF == close_vector (opt_index))
        return (UNDEF);

    for (i = 0; i < doc_desc.num_ctypes; i++) {
	if (UNDEF == close_con_cw_cf(con_cw_cf_inst[i]))
	    return UNDEF;
    }

    (void) free ((char *) con_cw_cf_inst);

    PRINT_TRACE (2, print_string, "Trace: leaving close_p_df_stats");
    return (0);
}
