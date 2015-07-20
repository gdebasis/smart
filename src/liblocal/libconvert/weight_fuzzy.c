#include <math.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "vector.h"
#include "io.h"
#include "inst.h"
#include "trace.h"
#include "docdesc.h"
#include "vector.h"
#include "dir_array.h"

/* WARNING: Only works for the 1st ctype 
 * Convert an nnn document vector into a fuzzy vector where each term is replaced by all possible
 * senses with an associated degree of membership. Do this transformation only for ctype=0, i.e. words
 * Reads in a vector file where for each term the associated list contains (concept number of a sense, belief in that sense).
 */

static char* wsFileName;
static long wsFileMode;
static long num_ctypes;
int    		ws_vec_fd; 
DIR_ARRAY 	dir_array;
CON_WT *conwt_buf;
long   num_conwt_buf;
long   *ctype_len_buf;
long   maxNumRealisations;

static SPEC_PARAM spec_args[] = {
    {"local.convert.obj.weight_fuzzy.ws_vec_file", getspec_dbfile, (char *) &wsFileName}, /* The word sense file (vec formatted) */
    {"local.convert.obj.weight_fuzzy.ws_vec_file.rmode", getspec_filemode, (char *) &wsFileMode}, /* The word sense file mode */
	{"weight.num_ctypes", getspec_long,   (char *) &num_ctypes},
	{"local.convert.obj.weight_fuzzy.max_num_realisations", getspec_long,   (char *) &maxNumRealisations},
    TRACE_PARAM ("local.convert.obj.weight_fuzzy.trace")
} ;
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

int init_weight_fuzzy(SPEC* spec, char* unused)
{
	char conceptName[256];

    PRINT_TRACE (2, print_string, "Trace: entering init_weight_fuzzy");

	// Intialize buffer to copy invec's term weights into outvec's ones
    num_conwt_buf = 8092;

	if (NULL == (conwt_buf = (CON_WT *)
	       		 malloc (num_conwt_buf * sizeof (CON_WT))))
        return (UNDEF);
    if (NULL == (ctype_len_buf = (long *)
	             malloc ((unsigned) num_ctypes * sizeof (long))))
        return (UNDEF);

    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

	/* Open the word sense list file */
	if (UNDEF == (ws_vec_fd = open_vector(wsFileName, wsFileMode)))
		return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving init_weight_fuzzy");
    return (1);
}

/* In this procedure we replace each term with its possible realizations with degrees of memberships.
 * Thus the term frequencies are no more integers after this conversion.  
*/
int weight_fuzzy(VEC* invec, VEC* outvec, int unused)
{
    CON_WT *invec_conwt_ptr, *outvec_conwt_ptr;
	long con, newTermCon, numRealisations;
	VEC ws_vec;
	register CON_WT *ws_vec_conwt_ptr, *end_ws_vec_conwt_ptr;
	static char msg[1024];
	int wsFound;
	long maxBufferSizeEstimate;

    PRINT_TRACE (2, print_string, "Trace: entering weight_fuzzy");

   /* If no outvec is passed, we'll reweight the invec in place */
    if (outvec == NULL)
    	outvec = invec;
    else {
	    bcopy ((char *) invec->ctype_len,
           (char *) ctype_len_buf,
           (int) invec->num_ctype * sizeof (long));

		/* Estimate the buffer size required to store the outvec */
		maxBufferSizeEstimate = invec->num_conwt * maxNumRealisations;
		if (maxBufferSizeEstimate > num_conwt_buf) {
	        if (NULL == (conwt_buf = Realloc(conwt_buf,
                         maxBufferSizeEstimate, CON_WT )))
		        return (UNDEF);
	        num_conwt_buf = maxBufferSizeEstimate;
	    }

	    outvec->id_num = invec->id_num;
    	outvec->num_ctype = invec->num_ctype;
	    outvec->con_wtp   = conwt_buf;
	    outvec->ctype_len = ctype_len_buf;
		outvec->num_conwt = invec->num_conwt;
    }
                                                                                  
	for (	invec_conwt_ptr = invec->con_wtp, outvec_conwt_ptr = conwt_buf;
			invec_conwt_ptr < invec->con_wtp + invec->ctype_len[0];
			invec_conwt_ptr++) {

		/* Sanity check for the buffer size overflow */
		if (outvec_conwt_ptr >= conwt_buf + num_conwt_buf) {
			set_error(SM_ILLPA_ERR, "weight_fuzzy", "Buffer overflow");
			outvec = invec;
			return 1;
		}

		con = invec_conwt_ptr->con;
		ws_vec.id_num.id = con;
		
		/* Read in the word sense fuzziness from the vector file */
		wsFound = seek_vector (ws_vec_fd, &ws_vec);
        if (UNDEF == wsFound) {
			set_error(SM_ILLPA_ERR, "vec file seek", "Could not seek into the ws vector file");
			outvec_conwt_ptr->con = con;
			outvec_conwt_ptr->wt = invec_conwt_ptr->wt;
			outvec_conwt_ptr++;
			continue;
		}
		else if (0 == wsFound) {
			outvec_conwt_ptr->con = con;
			outvec_conwt_ptr->wt = invec_conwt_ptr->wt;
			outvec_conwt_ptr++;
			continue;
		}

		if (UNDEF == read_vector (ws_vec_fd, &ws_vec)) {
			snprintf(msg, sizeof(msg), "Did not find term %d in the wsvec file", con);
			PRINT_TRACE(4, print_string, msg); 	
			outvec_conwt_ptr->con = con;
			outvec_conwt_ptr->wt = invec_conwt_ptr->wt;
			outvec_conwt_ptr++;
			continue;
        }

		numRealisations = ws_vec.num_conwt - 1;	// The invec is going to be expanded by these many additional terms
		if (numRealisations <= 0) {
			outvec_conwt_ptr->con = con;
			outvec_conwt_ptr->wt = invec_conwt_ptr->wt;
			outvec_conwt_ptr++;
			continue;
		}

		outvec->num_conwt += numRealisations;	
		ctype_len_buf[0] = outvec->num_conwt;

        end_ws_vec_conwt_ptr = &ws_vec.con_wtp[ws_vec.num_conwt];

        for (ws_vec_conwt_ptr = ws_vec.con_wtp;
             ws_vec_conwt_ptr < end_ws_vec_conwt_ptr;
             ws_vec_conwt_ptr++) {

			newTermCon = ws_vec_conwt_ptr->con;
			if (newTermCon == UNDEF)
				continue;
			outvec_conwt_ptr->con = newTermCon;	// concept number of the new word (a sense of the original word)

			outvec_conwt_ptr->wt = invec_conwt_ptr->wt * ws_vec_conwt_ptr->wt;		// its' fuzzy membership 
			outvec_conwt_ptr++;
		}
	}

    PRINT_TRACE (4, print_vector, outvec);
    PRINT_TRACE (2, print_string, "Trace: leaving weight_fuzzy");
    return 1 ;
}

int close_weight_fuzzy(int unused)
{
    PRINT_TRACE (2, print_string, "Trace: entering close_weight_fuzzy");

	/* Deallocate the df buffer and close the files */
    if (UNDEF == close_vector (ws_vec_fd))
    	return (UNDEF);

	if (conwt_buf) {
		free(conwt_buf);
		conwt_buf = NULL;
	}

	if (ctype_len_buf) {
		free(ctype_len_buf);
		ctype_len_buf = NULL;
	}

    PRINT_TRACE (2, print_string, "Trace: leaving close_weight_fuzzy");
    return (1);
}
