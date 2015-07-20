#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/utility/make_trie.c,v 11.2 1993/02/03 16:48:38 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Expand a vector by lexically similar terms from a trie
 *1 local.ocr.expand.trie
 *2 vec_exp_trie (vector, exp_vector, inst)
 *3 VEC *vector;
 *3 VEC *exp_vector;
 *3 int inst;
 *4 init_vec_exp_trie (spec, unused)
 *5   "dict_file"
 *5   "vec_exp_trie.trie_file"
 *5   "vec_exp_trie.trace"
 *5   "num_expansion_type"
 *5   "expand.N.method"
 *5   "expand.N.ctype"
 *4 close_vec_exp_trie (inst)
 *7 Expand the input vector by considering lexically near-by terms
 *7 (terms that are same as query terms except for 1 (or 2?) changes).
 *7 Expand only ctype vec_exp_trie.major_ctype.
 *7 Each type of expansion method (expand.N.method) will be put in its 
 *7 own ctype (expand.N.ctype).
 *7 Current valid values for expansion method are 
 *7 exact, sub_1, del_1, ins_1
 *7 Use dict_file to get original term.  Lookup nearby terms in trie_file,
 *7 getting back a list of concepts.
 *7 Assign the original query weight to each new concept.
 *7 Return UNDEF if error, else 1;
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "io.h"
#include "spec.h"
#include "trace.h"
#include "smart_error.h"
#include "dict_noinfo.h"
#include "vector.h"
#include "ltrie.h"

static char *trie_file;
static long num_exp_method;
static long major_ctype;

static SPEC_PARAM spec_args[] = {
    {"vec_exp_trie.trie_file",        getspec_dbfile, (char *) &trie_file},
    {"vec_exp_trie.num_expansion_method",getspec_long,(char *)&num_exp_method},
    {"vec_exp_trie.major_ctype",      getspec_long, (char *)&major_ctype},
    TRACE_PARAM ("vec_exp_trie.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *prefix;
static char *temp_exp_method;
static long temp_exp_ctype;
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    {&prefix, "exp_method",     getspec_string,   (char *) &temp_exp_method},
    {&prefix, "exp_ctype",      getspec_long,     (char *) &temp_exp_ctype},
    };
static int num_prefix_spec_args = sizeof (prefix_spec_args) /
         sizeof (prefix_spec_args[0]);

static char *dict_file;
static long dict_mode;
static SPEC_PARAM_PREFIX prefix_spec_args1[] = {
    {&prefix, "dict_file",        getspec_dbfile, (char *) &dict_file},
    {&prefix, "dict_file.rmode",  getspec_filemode, (char *) &dict_mode},
    };
static int num_prefix_spec_args1 = sizeof (prefix_spec_args1) /
         sizeof (prefix_spec_args1[0]);


typedef struct {
    char *method;
    long ctype;
    CON_WT *conwt;
    long num_conwt;
    long max_num_conwt;
    TRIE_NEAR_OUTPUT trie_result;
} EXP_METHOD;

static EXP_METHOD *exp_method;

static LTRIE *root_trie;
static long root_trie_length;


/* Storage for final vector to be returned */
static long *ctype_len;
static long max_ctype_len;
static CON_WT *conwt;
static long max_num_conwt;
#define INIT_NUM_CONWT 1000
#define INIT_CTYPE_LEN 10

static int dict_fd;

static int comp_con();

int open_dict_noinfo(), seek_dict_noinfo(), read_dict_noinfo(), 
    close_dict_noinfo();

int trie_near_exact(), trie_near_sub_1(),trie_near_ins_1(),trie_near_del_1();

int
init_vec_exp_trie (spec, unused)
SPEC *spec;
char *unused;
{
    long i;
    int trie_fd;
    char temp_buf[100];

    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_vec_exp_trie");

    if (NULL == (exp_method = (EXP_METHOD *)
                 malloc ((unsigned) num_exp_method * sizeof (EXP_METHOD))))
        return (UNDEF);
    
    prefix = temp_buf;

    /* get dictionary for the major_ctype */
    (void) sprintf (temp_buf, "vec_exp_trie.ctype.%ld.", major_ctype);
    if (UNDEF == lookup_spec_prefix (spec,
                                     &prefix_spec_args1[0],
                                     num_prefix_spec_args1))
        return (UNDEF);
    if (UNDEF == (dict_fd = open_dict_noinfo (dict_file, dict_mode)))
        return (UNDEF);


    for (i = 0; i < num_exp_method; i++) {
        (void) sprintf (temp_buf, "vec_exp_trie.expand.%ld.", i);
        if (UNDEF == lookup_spec_prefix (spec,
                                         &prefix_spec_args[0],
                                         num_prefix_spec_args))
            return (UNDEF);
        exp_method[i].method = temp_exp_method;
        exp_method[i].ctype = temp_exp_ctype;
        exp_method[i].num_conwt = 0;
        exp_method[i].max_num_conwt = INIT_NUM_CONWT;
        if (NULL == (exp_method[i].conwt = (CON_WT *)
                     malloc (INIT_NUM_CONWT * sizeof (CON_WT))))
            return (UNDEF);
        exp_method[i].trie_result.num_matches = 0;
        exp_method[i].trie_result.max_num_matches = INIT_NUM_CONWT;
        if (NULL == (exp_method[i].trie_result.matches = (long *)
                     malloc (INIT_NUM_CONWT * sizeof (long))))
            return (UNDEF);
    }

    max_ctype_len = INIT_CTYPE_LEN;
    if (NULL == (ctype_len = (long *) malloc (INIT_CTYPE_LEN * sizeof (long))))
        return (UNDEF);

    max_num_conwt = INIT_NUM_CONWT;
    if (NULL == (conwt = (CON_WT *)
                     malloc (INIT_NUM_CONWT * sizeof (CON_WT))))
        return (UNDEF);


#ifdef MMAP
    if (-1 == (trie_fd = open (trie_file, O_RDONLY)) ||
        -1 == (root_trie_length = lseek (trie_fd, 0L, 2)) ||
        -1 == lseek (trie_fd, 0L, 0) ||
        root_trie_length == 0) {
        return (UNDEF);
    }
    if ((LTRIE *) -1 == (root_trie = (LTRIE *)
                         mmap ((caddr_t) 0,
                               (size_t) root_trie_length,
                               PROT_READ,
                               MAP_SHARED,
                               trie_fd,
                               (off_t) 0)))
        return (UNDEF);
    if (-1 == close (trie_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_vec_exp_trie");

    return (0);
#else
    set_error (SM_ILLPA_ERR, "Must have MMAP defined", "vec_exp_trie");
    return (UNDEF);
#endif
}

int
vec_exp_trie (vec, exp_vec, inst)
VEC *vec;
VEC *exp_vec;
int inst;
{
    long i,j;
    CON_WT *current_conwt, *end_conwt;
    long current_ctype, max_ctype;
    DICT_NOINFO dict;
    int status;
    long num_conwt, start_conwt;

    PRINT_TRACE (2, print_string, "Trace: entering vec_exp_trie");

    exp_vec->id_num = vec->id_num;
    exp_vec->num_conwt = 0;

    /* Expand only major_ctype */
    current_conwt = vec->con_wtp;
    for (current_ctype = 0;
         current_ctype < major_ctype && current_ctype < vec->num_ctype;
         current_ctype++)
        current_conwt += vec->ctype_len[current_ctype];
    if (current_ctype != major_ctype) {
        set_error (SM_ILLPA_ERR, "Major_ctype illegal", "vec_exp_trie");
        return (UNDEF);
    }
    
    for (i = 0; i < num_exp_method; i++) {
        exp_method[i].num_conwt = 0;
    }

    end_conwt = current_conwt + vec->ctype_len[current_ctype];
    /* Go through each concept of major_ctype and expand appropriately */
    while (current_conwt < end_conwt) {
        /* find text of concept */
        dict.con = current_conwt->con;
        dict.token = NULL;
        if (1 != (status = seek_dict_noinfo (dict_fd, &dict)) ||
            1 != (status = read_dict_noinfo (dict_fd, &dict))) {
            if (status == 0)
                set_error (SM_INCON_ERR, "Con not in dict", "vec_exp_trie");
            return (UNDEF);
        }

        for (i = 0; i < num_exp_method; i++) {
            exp_method[i].trie_result.num_matches = 0;
            if (0 == strcmp (exp_method[i].method, "exact")) {
                if (UNDEF == trie_near_exact (dict.token,
                                                   root_trie,
                                                   &exp_method[i].trie_result))
                    return (UNDEF);
            }
            else if (0 == strcmp (exp_method[i].method, "sub_1")) {
                if (UNDEF == trie_near_sub_1 (dict.token,
                                                   root_trie,
                                                   &exp_method[i].trie_result))
                    return (UNDEF);
            }
            else if (0 == strcmp (exp_method[i].method, "ins_1")) {
                if (UNDEF == trie_near_ins_1 (dict.token,
                                                   root_trie,
                                                   &exp_method[i].trie_result))
                    return (UNDEF);
            }
            else if (0 == strcmp (exp_method[i].method, "del_1")) {
                if (UNDEF == trie_near_del_1 (dict.token,
                                                   root_trie,
                                                   &exp_method[i].trie_result))
                    return (UNDEF);
            }
            else {
                set_error (SM_INCON_ERR, "Illegal method", "vec_exp_trie");
                return (UNDEF);
            }
            /* Add the new concepts to the con_wt list, with a weight the */
            /* same as the original query */
            if (exp_method[i].trie_result.num_matches +
                exp_method[i].num_conwt >
                exp_method[i].max_num_conwt) {
                exp_method[i].max_num_conwt +=
                    exp_method[i].trie_result.num_matches +
                        exp_method[i].num_conwt;
                if (NULL == (exp_method[i].conwt = (CON_WT *)
                     realloc (exp_method[i].conwt,
                              (unsigned) exp_method[i].max_num_conwt *
                              sizeof (CON_WT))))
                    return (UNDEF);
            }
            for (j = 0; j < exp_method[i].trie_result.num_matches; j++) {
                exp_method[i].conwt[exp_method[i].num_conwt].con =
                    exp_method[i].trie_result.matches[j];
                exp_method[i].conwt[exp_method[i].num_conwt].wt =
                    current_conwt->wt;
                exp_method[i].num_conwt++;
            }
        }
        current_conwt++;
    }

    /* Construct final con_wt relation */

    /* Reserve space for conwts and ctypes */
    num_conwt = vec->num_conwt;
    for (i = 0; i < num_exp_method; i++) {
        num_conwt += exp_method[i].num_conwt;
    }
    if (num_conwt > max_num_conwt) {
        max_num_conwt += num_conwt;
        (void) free ((char *) conwt);
        if (NULL == (conwt = (CON_WT *)
                     malloc ((unsigned) max_num_conwt * sizeof (CON_WT))))
            return (UNDEF);
    }
    max_ctype = 0;
    for (i = 0; i < num_exp_method; i++) {
        if (exp_method[i].ctype > max_ctype)
            max_ctype = exp_method[i].ctype;
    }
    if (vec->num_ctype - 1 > max_ctype)
        max_ctype = vec->num_ctype - 1;
    if (max_ctype >= max_ctype_len) {
        max_ctype_len = max_ctype + 1;
        (void) free ((char *) ctype_len);
        if (NULL == (ctype_len = (long *)
                     malloc ((unsigned) max_ctype_len * sizeof (long))))
            return (UNDEF);
    }

    current_conwt = vec->con_wtp;
    num_conwt = 0;
    for (current_ctype = 0; current_ctype <= max_ctype; current_ctype++) {
        start_conwt = num_conwt;
        /* Add any subvector from original query */
        if (current_ctype < vec->num_ctype &&
            vec->ctype_len[current_ctype] > 0) {
            (void) bcopy ((char *) current_conwt,
                          (char *) &conwt[num_conwt],
                          vec->ctype_len[current_ctype] * sizeof (CON_WT));
            num_conwt += vec->ctype_len[current_ctype];
            current_conwt += vec->ctype_len[current_ctype];
        }
        /* Add subvectors from each expansion method */
        for (i = 0; i < num_exp_method; i++) {
            if (exp_method[i].ctype == current_ctype &&
                exp_method[i].num_conwt > 0) {
                (void) bcopy ((char *)  exp_method[i].conwt,
                              (char *) &conwt[num_conwt],
                              exp_method[i].num_conwt * sizeof (CON_WT));
                num_conwt += exp_method[i].num_conwt;
            }
        }
        /* Sort final result */
        (void) qsort ((char *) &conwt[start_conwt],
                      num_conwt - start_conwt,
                      sizeof (CON_WT),
                      comp_con);
        ctype_len[current_ctype] = num_conwt - start_conwt;
    }

    exp_vec->con_wtp = conwt;
    exp_vec->ctype_len = ctype_len;
    exp_vec->num_ctype = max_ctype+1;
    exp_vec->num_conwt = num_conwt;

    PRINT_TRACE (2, print_string, "Trace: leaving vec_exp_trie");
    return (1);
}

int
close_vec_exp_trie (inst)
int inst;
{
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_vec_exp_trie");

    if (UNDEF == close_dict_noinfo (dict_fd))
        return (UNDEF);
    for (i = 0; i < num_exp_method; i++) {
        (void) free ((char *) exp_method[i].conwt);
        (void) free ((char *) exp_method[i].trie_result.matches);
    }
    (void) free ((char *) ctype_len);
    (void) free ((char *) conwt);
    (void) free ((char *) num_exp_method);
#ifdef MMAP
    if (UNDEF == munmap ((char *) root_trie, root_trie_length))
        return (UNDEF);
#endif  /* MMAP */

    PRINT_TRACE (2, print_string, "Trace: leaving close_vec_exp_trie");
    
    return (0);
}

static int
comp_con (ptr1, ptr2)
CON_WT *ptr1;
CON_WT *ptr2;
{
    return (ptr1->con - ptr2->con);
}
