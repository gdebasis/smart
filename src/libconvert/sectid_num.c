#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/sectid_num.c,v 11.2 1993/02/03 16:49:27 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Get section number corresponding to first char of sectoin_id
 *1 
 *2 sectid_num (section_id, num, inst)
 *3   char *section_id;
 *3   long *num;
 *3   long inst;

 *4 init_contok_dict (spec, unused)
 *5   "index.sectid_num.trace"
 *5   "index.section.*.name"
 *4 close_sectid_num (inst)

 *7 Lookup sectid in the list of section names obtained from docdesc.
 *7 Return i, the index of the section for which the first character of
 *7 sectid matches the first character of index.section.<i>.name
 *7 UNDEF returned if sectid not in docdesc, else 0.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "spec.h"
#include "docdesc.h"
#include "trace.h"
#include "smart_error.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.sectid_num.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static SM_INDEX_DOCDESC doc_desc;


int
init_sectid_num (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_sectid_num");

    if (UNDEF == lookup_spec_docdesc (spec, &doc_desc))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_sectid_num");

    return (0);
}

int
sectid_num (section_id, num, inst)
char *section_id;
long *num;
int inst;
{
    long i;
    char error_buf[50];
    for (i = 0; 
         i < doc_desc.num_sections &&
            doc_desc.sections[i].name[0] != section_id[0];
         i++)
        ;
    
    if (i >= doc_desc.num_sections) {
        (void) strcpy (error_buf, "Illegal section type ' '");
        error_buf[strlen(error_buf) - 2] = section_id[0];
        set_error (SM_INCON_ERR, error_buf, "sectid_num");
        return (UNDEF);
    }
    *num = i;

    return (1);
}

int
close_sectid_num (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_sectid_num");

    PRINT_TRACE (2, print_string, "Trace: entering close_sectid_num");
    return (0);
}



