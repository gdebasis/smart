#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_vector.c,v 11.2 1993/02/03 16:53:58 smart Exp $";
#endif
/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "buf.h"
#include "vector.h"
#include "pnorm_vector.h"
#include "pnorm_common.h"

static SM_BUF internal_output = {0, 0, (char *) 0};

/* Print a PNORM_VEC relation to stdout */

static void print_in_order();

void
print_pnorm_vector (vec, output)
PNORM_VEC *vec;
SM_BUF *output;
{
    char temp_buf[PATH_LEN];
    SM_BUF *out_p;

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    (void) sprintf (temp_buf, "***** QUERY %ld *****\n", vec->id_num.id);
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    (void) print_in_order (vec, 0, out_p);
    if (UNDEF == add_buf_string ("\n\n", out_p))
        return;

    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}

static void
print_in_order (q_ptr, root, out_p)
PNORM_VEC *q_ptr;
short root;
SM_BUF *out_p;
{
    char temp_buf[PATH_LEN];

    long offset;                        /* position of tuple  */
    CON_WT *cw_ptr;            /* ptr to current con_wt tuple */
    OP_P_WT *op_ptr;           /* ptr to current operator tuple */
    short child;

    offset = (q_ptr->tree + root)->info;

    if (UNDEF == (q_ptr->tree + root)->child) { /* a leaf - thus a concept */
        cw_ptr = q_ptr->con_wtp + offset;
        sprintf (temp_buf,"<%ld;%.3f>",cw_ptr->con,cw_ptr->wt);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    } 
    else { 		/* an operator */
        op_ptr = q_ptr->op_p_wtp + offset;
        switch(op_ptr->op) {
          case AND_OP:
          case OR_OP:
            if (UNDEF == add_buf_string ("\n(", out_p))
                return;
            for (child = (q_ptr->tree + root)->child; UNDEF != child;
                 child = (q_ptr->tree + child)->sibling) {
                (void)print_in_order (q_ptr, child, out_p);
                if (UNDEF != (q_ptr->tree + child)->sibling) {
                    if (op_ptr->op == AND_OP) {
                        sprintf (temp_buf," <AND(%.3f);%.3f> ",op_ptr->p,op_ptr->wt);
                        if (UNDEF == add_buf_string (temp_buf, out_p))
                            return;
                    }
                    else {/* == OR_OP */
                        sprintf (temp_buf," <OR(%.3f);%.3f> ",op_ptr->p,op_ptr->wt);
                        if (UNDEF == add_buf_string (temp_buf, out_p))
                            return;
                    }
                }
            } /* for */
            if (UNDEF == add_buf_string (")\n", out_p))
                return;
            break;
          case NOT_OP:
            if (UNDEF != (q_ptr->tree+(q_ptr->tree+root)->child)->sibling) {
                sprintf (temp_buf,
                    "print_pnorm_vec: print_in_order: NOT operator has more than one child.\n");
                if (UNDEF == add_buf_string (temp_buf, out_p))
                    return;
                return;
            }
            sprintf (temp_buf," <NOT;%.3f> (",op_ptr->wt);
            if (UNDEF == add_buf_string (temp_buf, out_p))
                return;
            (void)print_in_order (q_ptr,(q_ptr->tree + root)->child, out_p);
            if (UNDEF == add_buf_string (")\n", out_p))
                return;
            break;
          default:	/* error */
            sprintf (temp_buf,
                     "print_pnorm_vec: illegal operator type %d.",op_ptr->op);
            if (UNDEF == add_buf_string (temp_buf, out_p))
                return;
            return;
        } /* switch */
    } /* outer else */

} /* print_in_order */
