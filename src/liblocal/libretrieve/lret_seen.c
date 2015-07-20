#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/retrieve.c,v 11.2 1993/02/03 16:54:27 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Top-level procedure to run query collection against seen docs only
 *1 local.retrieve.top.learn_seendoc_query
 *2 learn_seendoc_query (unused1, unused2, inst)
 *3   char *unused1;
 *3   char *unused2;
 *3   int inst;
 *4 init_learn_seendoc_query (spec, unused)
 *5   "retrieve.seen_docs_file"
 *5   "retrieve.seen_docs_file.rmode"
 *5   "retrieve.num_wanted"
 *5   "retrieve.q_trvec"
 *5   "retrieve.learn_num_param"
 *5   "retrieve.learn_output_file"
 *5   "retrieve.learn_query_output_file"
 *5   "retrieve.*.learn_param"
 *5   "retrieve.*.learn_init_value"
 *5   "retrieve.*.learn_increment"
 *5   "retrieve.*.learn_num_increment"
 *5   "retrieve.trace"
 *4 close_learn_seendoc_query (inst)
 *6 global_start, global_end matched against query_id to determine if
 *6 this query should be run.

 *7 Top-level procedure to re-run a previously run retrieval.  Each
 *7 query's tr_vec is given to q_trvec which returns a new tr_vec
 *7 to be output. q_trvec is responsible for getting the query and
 *7 documents in the form that it needs.
 *7 Evaluate using precision at 8 points from num_wanted down on a log scale,
 *7 and with relative recall precision (assuming only rel docs are those in
 *7 the original retrieval).
 *7 Repeat the entire process with all possible combinations of spec values
 *7 given by *.learn_param (each param takes on *.learn_num_increment values
 *7 starting at *.learn_init_value and incrementing by *.learn_increment.)
 *7 The result of each pass is written in learn_output_file.
 *7 The best rcl_prn result of each individual query is kept track of.  
 *7 After the run is completed, learn_query_output_file is written, 
 *7 giving the query, result, and the parameters used to create that result.

***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "tr_vec.h"
#include "retrieve.h"
#include "rel_header.h"

static PROC_INST q_trvec;        /* proc to run query against all
                                    docs in tr_vec */
static char *seen_docs_file;
static long  seen_docs_mode;
static long  num_wanted;
static char *learn_output_file;
static char *learn_query_output_file;
static long  num_passes;

static SPEC_PARAM spec_args[] = {
    {"retrieve.q_tr",             getspec_func, (char *) &q_trvec.ptab},
    {"retrieve.seen_docs_file",   getspec_dbfile,(char *) &seen_docs_file},
    {"retrieve.seen_docs_file.rmode",getspec_filemode,(char *)&seen_docs_mode},
    {"retrieve.num_wanted",       getspec_long, (char *) &num_wanted},
    {"retrieve.learn_output_file",getspec_string, (char *) &learn_output_file},
    {"retrieve.learn_query_output_file", getspec_string,
         (char *) &learn_query_output_file},
    {"retrieve.learn_num_param",  getspec_long, (char *) &num_passes},
    TRACE_PARAM ("retrieve.top.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *prefix;
static char *learn_param_temp;
static float learn_init_value_temp;
static float learn_increment_temp;
static long  learn_num_increment_temp;

static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    {&prefix, "learn_param",      getspec_string, (char *) &learn_param_temp},
    {&prefix, "learn_init_value", getspec_float, (char *) &learn_init_value_temp},
    {&prefix, "learn_increment",  getspec_float, (char *) &learn_increment_temp},
    {&prefix, "learn_num_increment",getspec_long,(char *)&learn_num_increment_temp},
    };
static int num_prefix_spec_args = sizeof (prefix_spec_args) /
         sizeof (prefix_spec_args[0]);

static int seen_fd;
static int ret_tr_inst;

static SPEC *current_spec;

static char **learn_args;
static char **learn_param;
static float *learn_init_value;
static float *learn_increment;
static long  *learn_num_increment;

static long *pass_array;


static int init_output_result(), close_output_result();
static void output_pass_result(), eval_tr_vec();
static int recursive_pass(), do_work();


int
init_learn_seendoc_query (spec, unused)
SPEC *spec;
char *unused;
{
    long i;
    char temp_buf[30];

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_learn_seendoc_query");

    /* Call all initialization procedures */
    if (UNDEF == (ret_tr_inst = init_ret_tr (spec, (char *) NULL)))
        return (UNDEF);

    /* Open input file */
    if ( ! VALID_FILE (seen_docs_file)) {
        set_error (SM_ILLPA_ERR, "learn_seendoc_query", "retrieve.seen_docs_file");
        return (UNDEF);
    }
    if (UNDEF == (seen_fd = open_tr_vec (seen_docs_file, seen_docs_mode)))
            return (UNDEF);

    if (NULL == (learn_args = (char **)
                 malloc ((unsigned) num_passes * 2 * sizeof (char *))) ||
        NULL == (learn_param = (char **)
                 malloc ((unsigned) num_passes * sizeof (char *))) ||
        NULL == (learn_init_value = (float *)
                 malloc ((unsigned) num_passes * sizeof (float))) ||
        NULL == (learn_increment = (float *)
                 malloc ((unsigned) num_passes * sizeof (float))) ||
        NULL == (learn_num_increment = (long *)
                 malloc ((unsigned) num_passes * sizeof (long))) ||
        NULL == (pass_array = (long *)
                 malloc ((unsigned) num_passes * sizeof (long))))
        return (UNDEF);

    prefix = temp_buf;
    for (i = 0; i < num_passes; i++) {
        (void) sprintf (temp_buf, "retrieve.%ld.", i);
        if (UNDEF == lookup_spec_prefix (spec,
                                         &prefix_spec_args[0],
                                         num_prefix_spec_args))
            return (UNDEF);
        learn_init_value[i] = learn_init_value_temp;
        learn_increment[i] = learn_increment_temp;
        learn_num_increment[i] = learn_num_increment_temp;
        learn_param[i] = learn_param_temp;

        learn_args[2*i] = learn_param[i];
        if (NULL == (learn_args[2*i+1] = malloc(20)))
            return (UNDEF);
    }

    current_spec = spec;

    if (UNDEF == init_output_result())
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_learn_seendoc_query");
    return (0);
}

int
learn_seendoc_query (unused1, unused2, inst)
char *unused1;
char *unused2;
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering learn_seendoc_query");

    if (UNDEF == recursive_pass ((long)0))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving learn_seendoc_query");
    return (1);
}

static int
recursive_pass (pass_no)
long pass_no;
{
    for (pass_array[pass_no] = 0;
         pass_array[pass_no] < learn_num_increment[pass_no];
         pass_array[pass_no]++) {
        (void) sprintf(learn_args[2 * pass_no + 1], "%f",
                       (double) learn_init_value[pass_no] +
                       pass_array[pass_no] * learn_increment[pass_no]);
        if (pass_no+1 < num_passes) {
            if (UNDEF == recursive_pass (pass_no+1))
                return (UNDEF);
        }
        else if (UNDEF == do_work ())
            return (UNDEF);
    }
    return (1);
}

static int
do_work ()
{
    TR_VEC seen_vec;
    RESULT results;
    TR_VEC tr_vec;

    /* Construct new spec file */
    if (UNDEF == mod_spec (current_spec, num_passes*2, learn_args))
        return (UNDEF);
    /* Call all initialization procedures which will be affected by */
    /* parameter change */
    if (UNDEF == (q_trvec.inst = q_trvec.ptab->init_proc (current_spec,
                                                          NULL)))
        return (UNDEF);
        
    if (UNDEF == seek_tr_vec (seen_fd, NULL))
        return (UNDEF);
    /* Get each tr_vec, call q_trvec, and write out results */
    while (1 == read_tr_vec (seen_fd, &seen_vec)) {
        if (seen_vec.qid < global_start)
            continue;
        if (seen_vec.qid > global_end)
            break;
        SET_TRACE (seen_vec.qid);
        if (UNDEF == q_trvec.ptab->proc (&seen_vec, &results, q_trvec.inst))
            return (UNDEF);
            
        tr_vec.num_tr = 0;
        if (UNDEF == ret_tr (&results, &tr_vec, ret_tr_inst))
            return (UNDEF);
            
        (void) eval_tr_vec (&tr_vec);
    }
        
    (void) output_pass_result();
            
    /* Close all pass procedures */
    if (UNDEF == q_trvec.ptab->close_proc(q_trvec.inst))
        return (UNDEF);
    return (1);
}

int
close_learn_seendoc_query (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_learn_seendoc_query");

    if (UNDEF == close_ret_tr (ret_tr_inst))
        return (UNDEF);

    if (UNDEF == close_tr_vec (seen_fd))
        return (UNDEF);

    if (UNDEF == close_output_result())
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_learn_seendoc_query");
    return (0);
}

static float result[8];
static long num_rel[8];
static double rel_prec;
static long rank_cutoff[8];   /* eg 1000, 500, 125, 62, 31, 15, 8, 4 */
static num_queries;
static FILE *output_fd;
static FILE *query_output_fd;
static char *rel_flag;         /* num_rel[0:num_wanted] is whether the doc
                                  retrieved at that rank is relevant */
typedef struct {
    float eval;
    long *pass_info;
} PER_QUERY_STATS;

static PER_QUERY_STATS *per_query_stats;
static long max_query;
static long *pass_info;

static int
init_output_result()
{
    long cutoff;
    long i;

    REL_HEADER *rh;

    if (NULL == (rh = get_rel_header (seen_docs_file)))
        return (UNDEF);

    max_query = rh->max_primary_value + 1;
    if (NULL == (per_query_stats = (PER_QUERY_STATS *)
                 malloc ((unsigned) max_query * sizeof (PER_QUERY_STATS))))
        return (UNDEF);
    if (NULL == (pass_info = (long *)
                 malloc ((unsigned) (max_query * num_passes) * sizeof (long))))
        return (UNDEF);
    for (i = 0; i < max_query; i++) {
        per_query_stats[i].eval = 0.0;
        per_query_stats[i].pass_info = &pass_info[i*num_passes];
    }

    num_queries = 0;
    cutoff = num_wanted;
    for (i = 0; i < 8; i++) {
        result[i] = 0.0;
        rank_cutoff[i] = cutoff;
        cutoff = cutoff / 2;
        if (cutoff == 0)
            cutoff = 1;
    }
    if (NULL == (rel_flag = malloc ((unsigned) num_wanted)))
        return (UNDEF);

    if (NULL == (query_output_fd = fopen (learn_query_output_file, "w")))
        return (UNDEF);
    if (NULL == (output_fd = fopen (learn_output_file, "w")))
        return (UNDEF);
    fprintf (output_fd, "num_passes: %ld, num_wanted %ld",
             num_passes, num_wanted);
    for (i = 0; i < num_passes; i++) {
        fprintf (output_fd, ", %s %f %f",
             learn_param[i],
             learn_init_value[i],
             learn_increment[i]);
    }
    
    fprintf (output_fd,"\ncutoffs:%ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld\n",
             rank_cutoff[7],
             rank_cutoff[6],
             rank_cutoff[5],
             rank_cutoff[4],
             rank_cutoff[3],
             rank_cutoff[2],
             rank_cutoff[1],
             rank_cutoff[0]);
    return (0);
}

static int
close_output_result()
{
    long i,j;

    for (i = 0; i < max_query; i++) {
        if (per_query_stats[i].eval > 0.0) {
            fprintf (query_output_fd, "\n%ld %6.4f",
                     i, (double) per_query_stats[i].eval);
            for (j = 0; j < num_passes; j++) 
                fprintf (query_output_fd, "\t%f",
                         (double) learn_init_value[j] +  
                         per_query_stats[i].pass_info[j] *
                         (double) learn_increment[j]);
        }
    }
    (void) fclose (query_output_fd);
    (void) fclose (output_fd);
    (void) free (rel_flag);
    (void) free ((char *) pass_info);
    (void) free ((char *) per_query_stats);
    return (0);
}
        


static void
eval_tr_vec (tr_vec)
TR_VEC *tr_vec;
{
    long i,j;
    long total_num_rel;    /* relevant in tr_vec */
    long num_rel_seen;     /* relevant with rank < num_wanted */
    double query_rel_prec;

    for (j = 0; j < 8; j++) {
        num_rel[j] = 0;
    }

    (void) bzero (rel_flag, (int) num_wanted);
    total_num_rel = 0;
    for (i = 0; i < tr_vec->num_tr; i++) {
        if (tr_vec->tr[i].rel) {
            total_num_rel++;
            for (j = 0; j < 8; j++) {
                if (tr_vec->tr[i].rank <= rank_cutoff[j])
                    num_rel[j]++;
            }
            if (tr_vec->tr[i].rank <= num_wanted) {
                rel_flag[tr_vec->tr[i].rank-1] = (char) 1;
            }
        }
    }
    for (j = 0; j < 8; j++) {
        result[j] += ((float) num_rel[j] / (float) rank_cutoff[j]);
    }

    num_rel_seen = 0;
    query_rel_prec = 0.0;
    for (i = 0; i < num_wanted; i++) {
        if (rel_flag[i]) {
            num_rel_seen++;
            query_rel_prec += (double) num_rel_seen/(i+1);
        }
    }
    if (total_num_rel > 0) {
        query_rel_prec = query_rel_prec / total_num_rel;
        rel_prec += query_rel_prec;
    }

    /* Check to see if these are best params for this query so far */
    if (query_rel_prec > per_query_stats[tr_vec->qid].eval) {
        per_query_stats[tr_vec->qid].eval = query_rel_prec;
        for (i = 0; i < num_passes; i++) {
            per_query_stats[tr_vec->qid].pass_info[i] = pass_array[i];
        }
    }
    
    num_queries++;
}

static void
output_pass_result()
{
    long i;

    for (i = 0; i < num_passes; i++) 
        fprintf (output_fd, "%f ",
                 learn_init_value[i] + pass_array[i] * learn_increment[i]);
    fprintf (output_fd, "%f %f %f %f %f %f %f %f %f\n",
             rel_prec / num_queries,
             (double) result[7] / num_queries,
             (double) result[6] / num_queries,
             (double) result[5] / num_queries,
             (double) result[4] / num_queries,
             (double) result[3] / num_queries,
             (double) result[2] / num_queries,
             (double) result[1] / num_queries,
             (double) result[0] / num_queries);
    (void) fflush (output_fd);
    num_queries = 0;
    for (i = 0; i < 8; i++) {
        result[i] = 0.0;
    }
    rel_prec = 0.0;
}
