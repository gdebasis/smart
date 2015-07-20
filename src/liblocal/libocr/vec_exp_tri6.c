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
 *1 local.ocr.expand.trie_6
 *2 vec_exp_trie_6 (vector, exp_vector, inst)
 *3 VEC *vector;
 *3 VEC *exp_vector;
 *3 int inst;
 *4 init_vec_exp_trie_6 (spec, unused)
 *5   "vec_exp_trie.trace"
 *5   "dict_file"
 *5   "vec_exp_trie.trie_file"
 *5   "vec_exp_trie.trie_file"
 *5   "num_expansion_type"
 *5   "expand.N.method"
 *5   "expand.N.ctype"
 *4 close_vec_exp_trie_6 (inst)
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
#include "collstat.h"
#include "vector.h"
#include "dir_array.h"
#include "ltrie.h"

static char *trie_file;
static long num_exp_method;
static long major_ctype;
static long min_orig_length;
static long min_orig_length2;
static float wt_constant;
static float freq_constant;
static float min_weight;

static SPEC_PARAM spec_args[] = {
    {"vec_exp_trie.trie_file",        getspec_dbfile, (char *) &trie_file},
    {"vec_exp_trie.num_expansion_method",getspec_long,(char *)&num_exp_method},
    {"vec_exp_trie.major_ctype",      getspec_long, (char *)&major_ctype},
    {"vec_exp_trie.min_orig_length",getspec_long,(char *)&min_orig_length},
    {"vec_exp_trie.min_orig_length2",getspec_long,(char *)&min_orig_length2},
    {"vec_exp_trie.wt_constant",   getspec_float,(char *)&wt_constant},
    {"vec_exp_trie.freq_constant",   getspec_float,(char *)&freq_constant},
    {"vec_exp_trie.min_weight",   getspec_float,(char *)&min_weight},
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
static char *collstat_file;
static long collstat_mode;
static SPEC_PARAM_PREFIX prefix_spec_args1[] = {
    {&prefix, "dict_file",        getspec_dbfile, (char *) &dict_file},
    {&prefix, "dict_file.rmode",  getspec_filemode, (char *) &dict_mode},
    {&prefix, "collstat_file",      getspec_dbfile, (char *) &collstat_file},
    {&prefix, "collstat_file.rmode",getspec_filemode, (char *) &collstat_mode},
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
static int collstat_fd;

static int triestem_inst;
static int remove_s_inst;

static long num_collstat;
static long *collstat_ptr;

static int comp_con();

int open_dict_noinfo(), seek_dict_noinfo(), read_dict_noinfo(), 
    close_dict_noinfo();
int init_remove_s(), remove_s(), close_remove_s();

int trie_near_exact(), trie_near_sub_1(),trie_near_ins_1(),trie_near_del_1(),
    trie_near_prefix(), trie_near_2();

int
init_vec_exp_trie_6 (spec, unused)
SPEC *spec;
char *unused;
{
    long i;
    int trie_fd;
    int collstat_fd;
    char temp_buf[100];
    DIR_ARRAY dir_array;

    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_vec_exp_trie_6");

    if (NULL == (exp_method = (EXP_METHOD *)
                 malloc ((unsigned) num_exp_method * sizeof (EXP_METHOD))))
        return (UNDEF);
    
    prefix = temp_buf;

    /* get dictionary for the major_ctype */
    (void) sprintf (temp_buf, "vec_exp_trie_6.ctype.%ld.", major_ctype);
    if (UNDEF == lookup_spec_prefix (spec,
                                     &prefix_spec_args1[0],
                                     num_prefix_spec_args1))
        return (UNDEF);
    if (UNDEF == (dict_fd = open_dict_noinfo (dict_file, dict_mode)))
        return (UNDEF);
    if (UNDEF == (collstat_fd = open_dir_array (collstat_file, collstat_mode)))
        return (UNDEF);
    dir_array.id_num = COLLSTAT_COLLFREQ;
    if (1 != seek_dir_array (collstat_fd, &dir_array) ||
        1 != read_dir_array (collstat_fd, &dir_array))
        /* Somehow, file exists and is empty */
        return (UNDEF);
    num_collstat = dir_array.num_list / sizeof (long);
    collstat_ptr = (long *) dir_array.list;

    for (i = 0; i < num_exp_method; i++) {
        (void) sprintf (temp_buf, "vec_exp_trie_6.expand.%ld.", i);
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

    if (UNDEF == (triestem_inst = init_triestem (spec, (char *) NULL)) ||
        UNDEF == (remove_s_inst = init_remove_s (spec, (char *) NULL)))
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

    PRINT_TRACE (2, print_string, "Trace: leaving init_vec_exp_trie_6");

    return (0);
#else
    set_error (SM_ILLPA_ERR, "Must have MMAP defined", "vec_exp_trie_6");
    return (UNDEF);
#endif
}

int
vec_exp_trie_6 (vec, exp_vec, inst)
VEC *vec;
VEC *exp_vec;
int inst;
{
    long i,j;
    CON_WT *current_conwt, *end_conwt;
    CON_WT *good_conwt, *orig_conwt;

    long current_ctype, max_ctype;
    DICT_NOINFO dict, stem_dict;
    int status;
    long num_conwt, start_conwt;
    float exp_freq, orig_freq;
    char temp_buf[1024];
    char *stemmed_token;
    int no_reweight;

    PRINT_TRACE (2, print_string, "Trace: entering vec_exp_trie_6");

    exp_vec->id_num = vec->id_num;
    exp_vec->num_conwt = 0;

    /* Expand only major_ctype */
    current_conwt = vec->con_wtp;
    for (current_ctype = 0;
         current_ctype < major_ctype && current_ctype < vec->num_ctype;
         current_ctype++)
        current_conwt += vec->ctype_len[current_ctype];
    if (current_ctype != major_ctype) {
        set_error (SM_ILLPA_ERR, "Major_ctype illegal", "vec_exp_trie_6");
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
                set_error (SM_INCON_ERR, "Con not in dict", "vec_exp_trie_6");
            return (UNDEF);
        }

        /* Don't expand if query term too short */
        if (strlen (dict.token) < min_orig_length) {
            current_conwt++;
            continue;
        }

        orig_freq = (float) collstat_ptr[current_conwt->con];

        for (i = 0; i < num_exp_method; i++) {
            exp_method[i].trie_result.num_matches = 0;
            no_reweight = 0;
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
            else if (0 == strcmp (exp_method[i].method, "all_2")) {
                /* Don't expand if query term too short */
                if (strlen (dict.token) < min_orig_length2) {
                    continue;
                }
                if (UNDEF == trie_near_2 (dict.token,
                                          root_trie,
                                          &exp_method[i].trie_result))
                    return (UNDEF);
            }
            else if (0 == strcmp (exp_method[i].method, "prefix")) {
                if (UNDEF == trie_near_prefix (dict.token,
                                                   root_trie,
                                                   &exp_method[i].trie_result))
                    return (UNDEF);
            }
            else if (0 == strcmp (exp_method[i].method, "stem_prefix")) {
                (void) strcpy (temp_buf, dict.token);
                if (UNDEF == triestem(temp_buf, &stemmed_token, triestem_inst))
                    return (UNDEF);

                if (UNDEF == trie_near_prefix (stemmed_token,
                                                   root_trie,
                                                   &exp_method[i].trie_result))
                    return (UNDEF);
            }
            else if (0 == strcmp (exp_method[i].method, "sing_prefix")) {
                (void) strcpy (temp_buf, dict.token);
                if (UNDEF == remove_s(temp_buf, &stemmed_token, remove_s_inst))
                    return (UNDEF);

                if (UNDEF == trie_near_prefix (stemmed_token,
                                                   root_trie,
                                                   &exp_method[i].trie_result))
                    return (UNDEF);
            }
            else if (0 == strcmp (exp_method[i].method, "sing")) {
                (void) strcpy (temp_buf, dict.token);
                if (UNDEF == remove_s(temp_buf, &stemmed_token, remove_s_inst))
                    return (UNDEF);
                stem_dict.token = stemmed_token;
                stem_dict.con = UNDEF;
                if (1 == (status = seek_dict_noinfo (dict_fd, &stem_dict)) &&
                    1 == (status = read_dict_noinfo (dict_fd, &stem_dict)) &&
                    0 != strcmp (dict.token, stem_dict.token)) {
                    /* Add the plural_removed token to the query */
                    no_reweight = 1;
                    exp_method[i].trie_result.matches[0] = stem_dict.con;
                    exp_method[i].trie_result.num_matches = 1;
                }
                else if (status == UNDEF)
                    return (UNDEF);
            }
            else {
                set_error (SM_INCON_ERR, "Illegal method", "vec_exp_trie_6");
                return (UNDEF);
            }
            /* Add the new concepts to the con_wt list, with a weight a */
            /* function of the original query weight, the original freq */
            /* and the frequency of this term */
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
                exp_freq = (float)
                    collstat_ptr[exp_method[i].trie_result.matches[j]];
                exp_method[i].conwt[exp_method[i].num_conwt].con =
                    exp_method[i].trie_result.matches[j];
                if (no_reweight) {
                    exp_method[i].conwt[exp_method[i].num_conwt].wt =
                        current_conwt->wt / 2.0;
                    exp_method[i].num_conwt++;
                }
                else {
                    exp_method[i].conwt[exp_method[i].num_conwt].wt =
                        (current_conwt->wt * wt_constant) /
                            (1.0 + freq_constant * exp_freq);
                    /* Throw out terms with insignificant weights */
                    if (exp_method[i].conwt[exp_method[i].num_conwt].wt > 
                        min_weight)
                        exp_method[i].num_conwt++;
                }
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

    /* Remove duplicates */
    /* good_conwt points to last valid (unduplicated) entry. */
    /* orig_conwt points to next entry to consider */
    /* end_conwt points past last entry of current ctype */
    good_conwt = conwt - 1;
    orig_conwt = conwt;
    end_conwt = conwt;
    for (current_ctype = 0; current_ctype <= max_ctype; current_ctype++) {
        if (0 == ctype_len[current_ctype])
            continue;
        end_conwt += ctype_len[current_ctype];
        good_conwt++;
        good_conwt->con = orig_conwt->con;
        good_conwt->wt = orig_conwt->wt;
        orig_conwt++;
        while (orig_conwt < end_conwt) {
            if (good_conwt->con == orig_conwt->con) {
                if (good_conwt->wt < orig_conwt->wt)
                    good_conwt->wt = orig_conwt->wt;
                ctype_len[current_ctype]--;
            }
            else {
                good_conwt++;
                good_conwt->con = orig_conwt->con;
                good_conwt->wt = orig_conwt->wt;
            }
            orig_conwt++;
        }
    }
    num_conwt = (good_conwt - conwt) + 1;
    
    exp_vec->con_wtp = conwt;
    exp_vec->ctype_len = ctype_len;
    exp_vec->num_ctype = max_ctype+1;
    exp_vec->num_conwt = num_conwt;

    PRINT_TRACE (2, print_string, "Trace: leaving vec_exp_trie_6");
    return (1);
}

int
close_vec_exp_trie_6 (inst)
int inst;
{
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_vec_exp_trie_6");

    if (UNDEF == close_dict_noinfo (dict_fd))
        return (UNDEF);
    if (UNDEF == close_dir_array (collstat_fd))
        return (UNDEF);
    for (i = 0; i < num_exp_method; i++) {
        (void) free ((char *) exp_method[i].conwt);
        (void) free ((char *) exp_method[i].trie_result.matches);
    }
    (void) free ((char *) ctype_len);
    (void) free ((char *) conwt);
    (void) free ((char *) num_exp_method);

    if (UNDEF == close_triestem (triestem_inst) ||
        UNDEF == close_remove_s (remove_s_inst))
        return (UNDEF);

#ifdef MMAP
    if (UNDEF == munmap ((char *) root_trie, root_trie_length))
        return (UNDEF);
#endif  /* MMAP */

    PRINT_TRACE (2, print_string, "Trace: leaving close_vec_exp_trie_6");
    
    return (0);
}

static int
comp_con (ptr1, ptr2)
CON_WT *ptr1;
CON_WT *ptr2;
{
    return (ptr1->con - ptr2->con);
}
