#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/show_vecs.c,v 11.2 1993/02/03 16:51:32 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "io.h"
#include "spec.h"
#include "vector.h"
#include "docindex.h"
#include "inter.h"
#include "inst.h"
#include "smart_error.h"
#include "trace.h"

static long docid_width;
static SPEC_PARAM spec_args[] = {
    {"inter.docid_width",  getspec_long, (char *) &docid_width},
    TRACE_PARAM ("inter.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    SPEC *saved_spec;
    int print_vec_inst;
    int util_inst;
    long docid_width;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_show_vec (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;
    long i;

    /* Check to see if this spec file is already initialized (only need
       one initialization per spec file) */
    for (i = 0; i <  max_inst; i++) {
        if (info[i].valid_info && spec->spec_id==info[i].saved_spec->spec_id) {
            info[i].valid_info++;
            return (i);
        }
    }

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_show_vec");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    if (UNDEF == (ip->util_inst = init_inter_util (spec, (char *) NULL)))
        return (UNDEF);

    ip->saved_spec = spec;

    /* WARNING.  docid_width should really get set using is->orig_spec so */
    /* it doesn't change upon an Euse command */
    ip->docid_width = docid_width;
    if (UNDEF == (ip->print_vec_inst =
                 init_print_vec_dict (spec, (char *) NULL)))
        return (UNDEF);

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_show_vec");

    return (new_inst);
}


int
close_show_vec (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_show_vec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.show_vec");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Check to see if still have valid instantiations using this data */
    if (--ip->valid_info > 0)
        return (0);

    if (UNDEF == close_inter_util (ip->util_inst))
        return (UNDEF);
    if (UNDEF == close_print_vec_dict (ip->print_vec_inst))
        return (UNDEF);

    ip->valid_info--;
    PRINT_TRACE (2, print_string, "Trace: leaving close_show_vec");

    return (1);
}

int
show_qvec (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    char temp_buf[PATH_LEN];
    VEC *qvec = (VEC *)is->retrieval.query->vector;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering show_qvec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.show_qvec");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (is->retrieval.query->vector == NULL) {
        if (UNDEF == add_buf_string ("No valid query.  Command ignored\n",
                                     &is->err_buf))
            return (UNDEF);
        return (0);
    }

    (void) sprintf (temp_buf,
                    "Query vector %ld\n%*s Ctype\tCon\tWeight\t\tToken\n",
		    qvec->id_num.id, (int) -ip->docid_width, "Docid" );
    if (UNDEF == add_buf_string (temp_buf, &is->output_buf))
        return (UNDEF);
    if (UNDEF == print_vec_dict (qvec, &is->output_buf, ip->print_vec_inst)) {
        if (UNDEF ==add_buf_string("Query can't be printed. Command ignored\n",
                                    &is->err_buf))
            return (UNDEF);
        return (0);
    }
    
    PRINT_TRACE (2, print_string, "Trace: leaving show_qvec");

    return (1);
}


int
show_dvec (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    char temp_buf[PATH_LEN];
    int i;
    VEC vec;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering show_dvec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.show_dvec");
        return (UNDEF);
    }
    ip  = &info[inst];


    if (is->num_command_line < 2) {
	if (UNDEF == add_buf_string( "Command requires a docid; try again\n",
				    &is->err_buf ))
	    return UNDEF;
	return 0;
    }

    for (i = 1; i < is->num_command_line; i++) { 
	sprintf(temp_buf, "\n-----\n%s\n-----\n", is->command_line[i]);
	if (UNDEF == add_buf_string (temp_buf, &is->output_buf))
            return (UNDEF);

	if (UNDEF == inter_get_vec (is->command_line[i], &vec, ip->util_inst)) {
	    if (UNDEF == add_buf_string("Invalid docid; command ignored\n",
					&is->err_buf))
		return (UNDEF);
	    return (0);
	}

	if (0 == vec.num_conwt) {
	    if (UNDEF==add_buf_string("There are no indexed terms in the vector\n",
				      &is->err_buf))
		return (UNDEF);
	    return (0);
	}

	(void) sprintf (temp_buf,
			"Document vector %s\n%*s Ctype\tCon\tWeight\t\tToken\n",
			is->command_line[i],
			(int) -ip->docid_width, "Docid" );
	if (UNDEF == add_buf_string (temp_buf, &is->output_buf))
	    return (UNDEF);

	if (UNDEF == print_vec_dict( &vec,
				     &is->output_buf,
				     ip->print_vec_inst))
	    return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving show_dvec");

    return (1);
}
