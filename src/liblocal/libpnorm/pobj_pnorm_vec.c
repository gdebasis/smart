#include <stdio.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "io.h"
#include "rel_header.h"
#include "dict.h"
#include "vector.h"
#include "pnorm_vector.h"
#include "pnorm_common.h"
#include "spec.h"
#include "trace.h"
#include "local_functions.h"


static char *dict_file;
static long dict_mode;
static char *pvec_file;
static long pvec_mode;
static int  weights;	/* if true, print out weights, too */

static SPEC_PARAM spec_args[] = {
    {"print.dict_file",		getspec_dbfile,	  (char *)&dict_file},
    {"print.dict_file.rmode",	getspec_filemode, (char *)&dict_mode},
    {"print.query_file",	getspec_dbfile,	  (char *)&pvec_file},
    {"print.query_file.rmode",	getspec_filemode, (char *)&pvec_mode},
    {"print.query_file.weight", getspec_bool,     (char *)&weights},
    TRACE_PARAM ("print.trace")
};
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static int dict_index, pvec_index;


int
init_print_obj_pnorm_vec (spec, unused)
SPEC	*spec;
char	*unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_print_obj_pnorm_vec");
    PRINT_TRACE (2, print_string, "Trace: leaving init_print_obj_pnorm_vec");
    return (0);
} /* init_print_obj_pnorm_vec */


int
print_obj_pnorm_vec (in_file, out_file, inst)
char	*in_file;
char	*out_file;
int	inst;
{
    char *final_pvec_file;
    FILE *output;
    PNORM_VEC pvec;		/* current query vector */
    int status;
    static void print_in_order();


    PRINT_TRACE (2, print_string, "Trace: entering print_obj_pnorm_vec");

    final_pvec_file = VALID_FILE(in_file) ? in_file : pvec_file;
    output = VALID_FILE(out_file) ? fopen(out_file, "w") : stdout;
    if (NULL == output)
        return (UNDEF);

    if (UNDEF == (dict_index = open_dict(dict_file, dict_mode)))
        return (UNDEF);

    if (UNDEF == (pvec_index = open_pnorm(final_pvec_file, pvec_mode)))
        return (UNDEF);

    /* Get each pnorm query in turn */
    if (global_start > 0) {
        pvec.id_num.id = global_start; EXT_NONE(pvec.id_num.ext);
        if (1 != seek_pnorm(pvec_index, &pvec)) {
            return (UNDEF);
        }
    }

    while (1 == (status = read_pnorm(pvec_index, &pvec)) &&
           pvec.id_num.id <= global_end) {
        printf("***** QUERY %ld *****\n", pvec.id_num.id);
        (void)print_in_order(&pvec, (short)0);
        printf("\n\n");
    }

/*
    ** print each query specified in input **
    while (EOF != scanf("%d",&qid)) {
        query_vector.id_num = qid;
        if (1 != seek_pnorm(vec_index,&query_vector)) {
          fprintf(stderr,"query %d not in collection.\n");
          continue;
        }
        if (1 != read_pnorm(vec_index,&query_vector)) {
          print_error("print_pquery","Quit.");
          exit(1);
        }

        printf("***** QUERY %d *****\n",qid);
        (void)print_in_order(&query_vector,0);
        printf("\n\n");
    }
*/

    close_pnorm(pvec_index);

    PRINT_TRACE (2, print_string, "Trace: leaving print_obj_pnorm_vec");
    return (status);
} /* print_obj_pnorm_vec */


int
close_print_obj_pnorm_vec (inst)
int	inst;
{
    PRINT_TRACE (2, print_string, "Trace: enteringclose_print_obj_vec");
    PRINT_TRACE (2, print_string, "Trace: leaving close_print_obj_vec");
    return (0);
} /* close_print_obj_pnorm_vec */


static void
print_in_order (q_ptr, root)
register PNORM_VEC *q_ptr;
short root;
{
    DICT_ENTRY dict_entry;
    long offset;                        /* position of tuple  */
    register CON_WT *cw_ptr;            /* ptr to current con_wt tuple */
    register OP_P_WT *op_ptr;           /* ptr to current operator tuple */
    short child;
    int printf();

    offset = (q_ptr->tree + root)->info;

    if (UNDEF == (q_ptr->tree + root)->child) { /* a leaf - thus a concept */
        cw_ptr = q_ptr->con_wtp + offset;
        dict_entry.con = cw_ptr->con;
        if (1 != seek_dict(dict_index,&dict_entry) ||
            1 != read_dict(dict_index,&dict_entry)) {
            print_error("print_in_order", "Quit");
            exit(1);
        }
        if (weights) {
            printf("<%s;%.3f>",dict_entry.token,cw_ptr->wt);
        }
        else {
            printf("<%s>",dict_entry.token);
        }
    } /* outer if */
    else { 		/* an operator */
        op_ptr = q_ptr->op_p_wtp + offset;
        switch(op_ptr->op) {
        case AND_OP:
        case OR_OP:
            printf ("\n(");
            for (child = (q_ptr->tree + root)->child; UNDEF != child;
                child = (q_ptr->tree + child)->sibling) {
                (void)print_in_order(q_ptr,child);
                if (UNDEF != (q_ptr->tree + child)->sibling) {
                    if (op_ptr->op == AND_OP) {
                        if (weights) {
      	                    printf(" <AND(%.3f);%.3f> ",op_ptr->p,op_ptr->wt);
                        }
                        else {
                            printf(" AND(%.3f) ",op_ptr->p);
                        }
                    }
                    else {/* == OR_OP */
                        if (weights) {
                            printf(" <OR(%.3f);%.3f> ",op_ptr->p,op_ptr->wt);
                        }
                        else {
                            printf(" OR(%.3f) ",op_ptr->p);
                        }
                    }
                }
            } /* for */
            printf(")\n");
            break;
        case NOT_OP:
            if (UNDEF != (q_ptr->tree+(q_ptr->tree+root)->child)->sibling) {
                fprintf(stderr,
                    "print_in_order: NOT operator has more than one child.\n");
                exit(1);
            }
    	    if (weights) {
                printf(" <NOT;%.3f> (",op_ptr->wt);
            }
            else {
                printf(" NOT (");
            }
            (void)print_in_order(q_ptr,(q_ptr->tree + root)->child);
            printf(")\n");
            break;
        default:	/* error */
            fprintf(stderr,"print_query: illegal operator type %d.",op_ptr->op);
            exit(1);
        } /* switch */
    } /* outer else */

} /* print_in_order */
