#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/show_vecs.c,v 11.2 1993/02/03 16:51:32 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "docindex.h"
#include "functions.h"
#include "local_eid.h"
#include "inter.h"
#include "io.h"
#include "local_inter.h"
#include "param.h"
#include "spec.h"
#include "vector.h"

static long docid_width;
static int eid_inst, pr_vec_dict_inst;
static char *comline[MAX_COMMAND_LINE];

static SPEC_PARAM spec_args[] = {
    {"inter.docid_width",  getspec_long, (char *) &docid_width},
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int num_inst = 0; 


int
init_show_part_vec (spec, unused)
SPEC *spec;
char *unused;
{
    if (num_inst++ > 0) {
        return (num_inst);
    }
    if(UNDEF == lookup_spec(spec, &spec_args[0], num_spec_args))
      return (UNDEF);
    if(UNDEF == (eid_inst = init_eid_util( spec, (char *)NULL )) ||
       UNDEF == (pr_vec_dict_inst = init_print_vec_dict(spec, (char *)NULL)))
      return UNDEF;

    return (num_inst);
}


int
close_show_part_vec (inst)
int inst;
{
    if (--num_inst > 0)
        return (0);

    if(UNDEF == close_eid_util( eid_inst ) ||
       UNDEF == close_print_vec_dict(pr_vec_dict_inst))
      return UNDEF;

    return (1);
}


int
show_part_vec (is, unused)
INTER_STATE *is;
char *unused;
{
    char temp_buf[PATH_LEN], *str;
    int i;
    EID_LIST elist;
    STRING_LIST strlist;
    VEC vec;

    strlist.string = comline;
    for (i = 1; i < is->num_command_line; i++)
	strlist.string[i-1] = is->command_line[i];
    strlist.num_strings = is->num_command_line - 1;

    if (UNDEF == stringlist_to_eidlist(&strlist, &elist, eid_inst)) {
	if (UNDEF == add_buf_string ("Bad document\n", &is->err_buf))
            return (UNDEF);
	return(0);
    }

    vec.num_ctype = vec.num_conwt = 0;

    for (i=0; i < elist.num_eids; i++) {
	if(UNDEF == eid_vec(elist.eid[i], &vec, eid_inst)) {
	    if (UNDEF==add_buf_string ("Couldn't get vector\n", &is->err_buf))
		return (UNDEF);
	    return(0);
	}

	if (0 == vec.num_conwt) {
	    if(UNDEF==add_buf_string("No indexed terms in this vector\n",
				     &is->err_buf))
		return (UNDEF);
	    continue;
	}

	eid_to_string(elist.eid[i], &str);
	(void) sprintf(temp_buf,
		       "Document vector %s\n%*s Ctype\tCon\tWeight\t\tToken\n",
		       str, (int) -docid_width, "Docid" );
	if (UNDEF == add_buf_string (temp_buf, &is->output_buf))
	    return (UNDEF);
	if (UNDEF==print_vec_dict(&vec, &is->output_buf,
				  pr_vec_dict_inst))
	    return (UNDEF);
	if(UNDEF == add_buf_string ("\n---\n\n", &is->output_buf))
	    return (UNDEF);
    }
    
    return (1);
}
