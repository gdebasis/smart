#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/synt_phrase.c,v 11.2 1993/02/03 16:49:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take an unstemmed indexing of collection, may stems to original words.
 *1 local.convert.obj.destem_obj
 *2 destem_obj (ignored, out_file, inst)
 *3   char *ignored;
 *3   char *out_file;
 *3   int inst;

 *4 init_destem_obj (spec, unused)
 *5   "destem.dict_file"
 *5   "destem.collstats_file"
 *5   "destem.temp_dict_file"
 *5   "destem.trace"
 *4 close_destem_obj (inst)

 *7 Read a dictionary and collstats file of a collection (normally an
 *7 unstemmed version of collection), stem each word in dict and keep track
 *7 of the most frequently occurring unstemmed word resulting in that stem.
 *7 Output a file with lines of form
 *7       stem\tmost_common_unstem
 *9 Doesn't do anything with multiple ctypes.
 *9 
 *9 
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "proc.h"
#include "dir_array.h"
#include "collstat.h"
#include "dict.h"
#include "inst.h"
#include "io.h"

static char *dict_file;
static long dict_file_mode;
static char *temp_dict_file;
static long temp_dict_file_mode;
static char *collstat_file;
static long collstat_mode;
static PROC_TAB *stemmer;

static SPEC_PARAM spec_args[] = {
    {"destem.dict_file", getspec_dbfile, (char *)&dict_file},
    {"destem.dict_file.rmode", getspec_filemode, (char *)&dict_file_mode},
    {"destem.temp_dict_file", getspec_dbfile, (char *)&temp_dict_file},
    {"destem.dict_file.rwmode", getspec_filemode,(char *)&temp_dict_file_mode},
    {"destem.collstat_file", getspec_dbfile, (char *)&collstat_file},
    {"destem.collstat_file.rmode", getspec_filemode, (char *)&collstat_mode},
    {"destem.stemmer", getspec_func, (char *)&stemmer},
    TRACE_PARAM ("destem.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int stemmer_inst;

int
init_destem_obj (spec, unused)
SPEC *spec;
char *unused;
{
    PRINT_TRACE (2, print_string, "Trace: entering init_destem_obj");

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
	return (UNDEF);

    if (UNDEF == (stemmer_inst = stemmer->init_proc (spec, NULL)))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_destem_obj");
    return 0;
}

int
destem_obj (ignored, out_file, inst)
char *ignored;
char *out_file;
int inst;
{
    REL_HEADER *rh;
    DIR_ARRAY dir_array;
    int dict_fd, stem_dict_fd, collstat_fd;
    FILE *out_fd;
    DICT_ENTRY dict_entry;
    DICT_ENTRY stem_dict_entry;
    char token[MAX_TOKEN_LEN];
    long *coll_freq;
    long status;

    PRINT_TRACE (2, print_string, "Trace: entering destem_obj");

    /* Open original dictionary */
    if (UNDEF == (dict_fd = open_dict (dict_file, dict_file_mode)))
        return (UNDEF);

    /* Open original collstats_file */
    if (UNDEF == (collstat_fd = open_dir_array (collstat_file, collstat_mode)))
	return (UNDEF);
    dir_array.id_num = COLLSTAT_COLLFREQ;
    if (1 != seek_dir_array (collstat_fd, &dir_array) ||
	1 != read_dir_array (collstat_fd, &dir_array))
	return (UNDEF);
    coll_freq = (long *) dir_array.list;

    /* Create a new stem dictionary of same size as original */
    if (NULL == (rh = get_rel_header (dict_file))) {
	set_error (SM_ILLPA_ERR, dict_file, "init_destem");
	return (UNDEF);
    }
    if (UNDEF == create_dict (temp_dict_file, rh))
        return (UNDEF);
    if (UNDEF == (stem_dict_fd = open_dict (temp_dict_file, temp_dict_file_mode)))
        return (UNDEF);
    
    while (1 == read_dict (dict_fd, &dict_entry)) {
	/* Make a copy of string of dict_entry */
	(void) strcpy (token, dict_entry.token);

	/* Stem copy of dict_entry*/
	if (UNDEF == stemmer->proc (token, &stem_dict_entry.token,
				    stemmer_inst))
	    return (UNDEF);
	/* See if stem is in temp_dict.  */
	stem_dict_entry.con = UNDEF;
	if (UNDEF == (status = (seek_dict (stem_dict_fd, &stem_dict_entry))))
	    return (UNDEF);
	if (0 == status) {
	    /* If not, enter with info field of the present concept */
	    stem_dict_entry.info = dict_entry.con;
	    if (1 != write_dict (stem_dict_fd, &stem_dict_entry))
		return (UNDEF);
	}
	else {
	    /* If in temp_dict, compare occurrences (from collstat_info) and */
	    /* make info field the most numerous entry */
	    if (1 != read_dict (stem_dict_fd, &stem_dict_entry))
		return (UNDEF);
	    if (coll_freq[stem_dict_entry.info] < coll_freq[dict_entry.con]) {
	    //if (strlen(stem_dict_entry.token) > strlen(dict_entry.token)) {
		stem_dict_entry.info = dict_entry.con;
		if (1 != seek_dict (stem_dict_fd, &stem_dict_entry) ||
		    1 != write_dict (stem_dict_fd, &stem_dict_entry))
		    return (UNDEF);
	    }
	}
    }
    PRINT_TRACE (4, print_string, "Trace: finished building up dict");
    PRINT_TRACE (4, print_string, out_file);
	
    /* Print out temp_dict, in form of "stem\tmost_common_unstem\n" */
    if (NULL == (out_fd = fopen (out_file, "w")))
		return (UNDEF);
    if (UNDEF == seek_dict (stem_dict_fd, NULL))
		return (UNDEF);
    while (1 == read_dict (stem_dict_fd, &stem_dict_entry)) {
	dict_entry.con = stem_dict_entry.info;
	dict_entry.token = NULL;
	if (1 != seek_dict (dict_fd, &dict_entry) ||
	    1 != read_dict (dict_fd, &dict_entry)) {
	    return (UNDEF);
	}

	fprintf (out_fd, "%s|%s\n",
		 stem_dict_entry.token, dict_entry.token);
    }

    (void) fclose (out_fd);

    if (UNDEF == close_dict (dict_fd) ||
	UNDEF == close_dict (stem_dict_fd))
	return (UNDEF);

    /* remove temp_dict */
    if (UNDEF == destroy_dict (temp_dict_file))
	return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving destem_obj");
    return 0;
}

int
close_destem_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_destem_obj");

    if (UNDEF == stemmer->close_proc(stemmer_inst))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_destem_obj");
    return (0);
}

