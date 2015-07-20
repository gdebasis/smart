#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/text_tr_o.c,v 11.2 1993/02/03 16:49:26 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Run length encoding compression of inverted list.
 *1 convert.tuple.inv_compinv
 *2 inv_compinv (inv, compinv, inst)
 *3 INV *inv;
 *3 IDR_ARRAY *compinv;
 *3 int inst;
 *4 init_inv_compinv (spec, unused)
 *5   "inv_compinv.trace"
 *4 close_inv_compinv (inst)

Assumptions:  inv.listwt.list is only positive integers, sorted in increasing
order, with no duplicates.

Compressed format:
First long is positive (first bit zero), indicates no compression for 
  this list.  List tuple is 32 bit long did, 32 bit float weight.
First long is negative (first bit one), indicates compression.
First 4 bytes is long, initial docid * -1. 
Next 4 bytes is float max_weight_in_list.
Next 4 bytes is float min_weight_in_list.
Next WEIGHT_GRAN bits are weight of initial docid.
Next 2 bits are compression type.

Compression type 0 (4_bit, default).  For each tuple:
First 4 bits give length of difference between previous docid and this one,
followed by the difference itself.
First bit of difference is assumed to be set, and is not included in length.
Eg. if difference is 18, then first 4 bits = 0100 and difference = 0010
(implicit 16 + 2).
Exception: All 4 bits being 1 indicate difference >= 2 ** 15.
The next 4 bits indicate the length over 15 bits. Example:
diff of  101001000100001000001 (1,345,601) would be represented as
1111 0101 01001000100001000001.
The bit_weight is then represented as a WEIGHT_GRAN bit quantity, 
    actual_weight = min_weight + 
        bit_weight * (max_weight-min_weight)/(2**WEIGHT_GRAN-1);

Any unused bits in the last word are set to 1.

Compression type 1 (5_bit).  For each tuple:
First 5 bits give length of difference between previous docid and this one,
followed by the difference itself.
First bit of difference is assumed to be set, and is not included in length.
Eg. if difference is 18, then first 5 bits = 00100 and difference = 0010
(implicit 16 + 2).
The bit_weight is then represented as a WEIGHT_GRAN bit quantity, 
    actual_weight = min_weight + 
        bit_weight * (max_weight-min_weight)/(2**WEIGHT_GRAN-1);

Compression type 2 (3_bit).  For each tuple:
First 3 bits give length of difference between previous docid and this one,
followed by the difference itself.
First bit of difference is assumed to be set, and is not included in length.
Eg. if difference is 18, then first 3 bits = 100 and difference = 0010
(implicit 16 + 2).
Exception: All 3 bits being 1 indicate difference >= 2 ** 7
The next 5 bits indicate the length of difference. Example:
diff of  101001000100001000001 (1,345,601) would be represented as
111 10100 01001000100001000001.
The bit_weight is then represented as a WEIGHT_GRAN bit quantity, 
    actual_weight = min_weight + 
        bit_weight * (max_weight-min_weight)/(2**WEIGHT_GRAN-1);
This type is only efficient if 4/5 of the differences are less than 127.
Not currently implemented.

Any unused bits in the last word are set to 1.


 *7 Return UNDEF if error, else 1;
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "io.h"
#include "functions.h"
#include "spec.h"
#include "trace.h"
#include "smart_error.h"
#include "dir_array.h"
#include "inv.h"


/* Define in param.h eventually */
#define WEIGHT_GRAN 5

/* Add num_bits of value to the current long at position current_long_index */
/* Assumes num_bits <= 32, and value fits in num_bits */

#define ADD(num_bits,value,current_long,current_long_index) \
if ((num_bits) <= current_long_index) { \
    current_long |= ((value) << (current_long_index - (num_bits))); \
    if ((num_bits) == current_long_index) { \
        current_long++; \
        current_long_index = 32; \
    } \
    else \
        current_long_index -= (num_bits); \
else { \
    current_long |= ((value) >> ((num_bits) - current_long_index)); \
    current_long++; \
    current_long |= (((value) & ((1 << current_long_index) - 1)) << (32 - ((num_bits) - current_long_index))); \
    current_long_index = 32 - ((num_bits) - current_long_index); \
}


static long mode;

static SPEC_PARAM spec_args[] = {
/*    {"inv_compinv.rwmode", getspec_filemode, (char *) &mode}, */
    TRACE_PARAM ("inv_compinv.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


int
init_inv_compinv (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_inv_compinv");
    PRINT_TRACE (2, print_string, "Trace: leaving init_inv_compinv");

    return (0);
}

int
inv_compinv (inv, compinv, inst)
INV *inv;
DIR_ARRAY *compinv;
int inst;
{
    DIR_ARRAY da;

    FILE *in_fd;
    int da_fd;
    char line_buf[PATH_LEN];
    SM_TEXTLINE textline;
    char *token_array[64];
    float *float_array;
    float *begin_float_array;
    long size_float_array;
    float *endmark_float_array;  /* set to */
                                 /* &begin_float_array[size_float_array-64] */
    long id_num;
    long i;
    long num_tokens = 0;

    PRINT_TRACE (2, print_string, "Trace: entering inv_compinv");

    /* Ensure space for compressed list. zero it out */


    /* if short inverted list (<= 2), do not compress */


    /* Go through inverted list, finding min_weight, max_weight,
       number of differences over 15 bits */


    /* Set first few bytes of compressed list */
    weight_ratio = (max_weight == min_weight) ? 0.0
                    : ((float) (1 << WEIGHT_GRAN) - 1) /
                        (max_weight - min_weight);
    *((long *)ptr) = - inv->listwt[0].list;
    ptr++;
    *((float *)bit_ptr) = max_weight;
    ptr++;
    *((float *)bit_ptr) = min_weight;
    ptr++;
    bit_weight = (unsigned long)
        ((inv->listwt[0].wt - min_weight) * weight_ratio);
    ptr_index = 32;
    ADD (WEIGHT_GRAN, bit_weight, ptr, ptr_index);

    /* If num_diff_over_15 is high, then use compression type 1 instead of 0 */
    if (num_diff_over_15 * 4 < inv.num_list) {
        /* Compression type 0 (4 bit len) */
        ADD (2, 0, ptr, ptr_index);

        (for i = 1; i < inv.num_list; i++) {
            diff = inv.listwt[i].list - inv.listwt[i-1].list;
            if (diff <= 0) {
                set_error (SM_INCON_ERR, "Illegal inverted list", "inv_compinv");
                return (UNDEF);
            }
            if (diff >= (1 << 15)) {
                for (k = 30; k > 15; k--) 
                    if (diff & (1 << k))
                        break;
                diff = diff ^ (1<<k);
                ADD (4, 15, ptr, ptr_index);
                ADD (4, k - 15, ptr, ptr_index);
                ADD (k, diff, ptr, ptr_index);
            }
            else {
                for (k = 15; k > 0; k--) 
                    if (diff & (1 << k))
                        break;
                diff = diff ^ (1<<k);
                ADD (4, k, ptr, ptr_index);
                ADD (k, diff, ptr, ptr_index);
            }
            bit_weight = (unsigned long)
                ((inv->listwt[0].wt - min_weight) * weight_ratio);
            ADD (WEIGHT_GRAN, bit_weight, ptr, ptr_index);
        }
    }
    else {
        /* Compression type 1 (5 bit len) */
        ADD (2, 1, ptr, ptr_index);
        (for i = 1; i < inv.num_list; i++) {
            diff = inv.listwt[i].list - inv.listwt[i-1].list;
            if (diff < 0) {
                set_error (SM_INCON_ERR, "Illegal inverted list", "inv_compinv");
                return (UNDEF);
            }
            for (k = 30; k > 0; k--) 
                if (diff & (1 << k))
                    break;
            diff = diff ^ (1<<k);
            ADD (5, k, ptr, ptr_index);
            ADD (k, diff, ptr, ptr_index);
            bit_weight = (unsigned long)
                ((inv->listwt[0].wt - min_weight) * weight_ratio);
            ADD (WEIGHT_GRAN, bit_weight, ptr, ptr_index);
        }
    }

    /* set last bits in current long to 1 */
    ADD (ptr_index, (1 << ptr_index) - 1, ptr, ptr_index);




    

    
    PRINT_TRACE (2, print_string, "Trace: leaving inv_compinv");
    return (1);
}

int
close_inv_compinv (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_inv_compinv");
    PRINT_TRACE (2, print_string, "Trace: leaving close_inv_compinv");
    
    return (0);
}
