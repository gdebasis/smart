#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libprint/pobj_cfdict.c,v 11.2 1993/02/03 16:52:18 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 print dict entries with correct cll freq info.
 *1 local.print.obj.collstat_dict
 *2 print_obj_collstat_dict (in_file, out_file, inst)
 *3   char *in_file;
 *3   char *out_file;
 *3   int inst;
 *4 init_print_obj_collstat_dict (spec, unused)
 *5   "print.collstat_file"
 *5   "print.collstat_file.rmode"
 *5   "print.dict_file"
 *5   "print.dict_file.rmode"
 *5   "print.verbose"
 *5   "print.trace"
 *4 close_print_obj_collstat_dict (inst)
 *6 global_start,global_end used to indicate what range of freq will be printed
 *7 The vec relation "in_file" (if not VALID_FILE, then use collstat_file),
 *7 will be used to print all collstat_freq entries in that file (modulo 
 *7 global_start, global_end).  Output to go into file "out_file".
 *7 If verbose is non-zero then print freq info plus the dict token,
 *7 otherwise just print the dict token.
 *7 (if not VALID_FILE, then stdout).
***********************************************************************/

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "spec.h"
#include "trace.h"
#include "dir_array.h"
#include "collstat.h"
#include "dict.h"

static char *collstat_file;
static long collstat_mode;
static char *dict_file;
static long dict_mode;
static long verbose_flag;

static SPEC_PARAM spec_args[] = {
    {"print.collstat_file",     getspec_dbfile,   (char *) &collstat_file},
    {"print.collstat_file.rmode",getspec_filemode, (char *) &collstat_mode},
    {"print.dict_file",         getspec_dbfile,   (char *) &dict_file},
    {"print.dict_file.rmode",   getspec_filemode, (char *) &dict_mode},
    {"print.verbose",           getspec_bool,     (char *) &verbose_flag},
    TRACE_PARAM ("print.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

int
init_print_obj_collstat_dict (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_print_obj_collstat_dict");

    PRINT_TRACE (2, print_string, "Trace: leaving init_print_obj_collstat_dict");
    return (0);
}

int
print_obj_collstat_dict (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    DIR_ARRAY dir_array;
    DICT_ENTRY dict;
    char *final_collstat_file;
    FILE *output;
    int collstat_index;              /* file descriptor for collstat_file */
    int dict_index;
    long *cf_list;
    long cf_num_list;

    PRINT_TRACE (2, print_string, "Trace: entering print_obj_collstat_dict");

    final_collstat_file = VALID_FILE (in_file) ? in_file : collstat_file;
    output = VALID_FILE (out_file) ? fopen (out_file, "w") : stdout;
    if (NULL == output)
        return (UNDEF);

    if (UNDEF == (collstat_index = open_dir_array (final_collstat_file,
                                                  collstat_mode)))
        return (UNDEF);
    dir_array.id_num = COLLSTAT_COLLFREQ;
    if (1 != (seek_dir_array (collstat_index, &dir_array)) ||
        1 != (read_dir_array (collstat_index, &dir_array))) {
        return (UNDEF);
    }

    cf_num_list = dir_array.num_list / sizeof (long);
    cf_list = (long *) dir_array.list;
    

    if (UNDEF == (dict_index = open_dict (dict_file, dict_mode)))
        return (UNDEF);

    while (1 == read_dict (dict_index, &dict)) {
        if (dict.con > cf_num_list)
            break;
        if (cf_list[dict.con] >= global_start &&
            cf_list[dict.con] <= global_end) {
            if (verbose_flag) 
                (void) fprintf (output, "%ld\t%ld\t%s\n",
                                dict.con,
                                cf_list[dict.con],
                                dict.token);
            else
                (void) fprintf (output, "%s\n",
                                dict.token);
        }
    }

    if (UNDEF == close_dir_array (collstat_index) ||
        UNDEF == close_dict (dict_index))
        return (UNDEF);

    if (output != stdin)
        (void) fclose (output);

    PRINT_TRACE (2, print_string, "Trace: leaving print_obj_collstat_dict");
    return (1);
}


int
close_print_obj_collstat_dict (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_print_obj_collstat_dict");
    

    PRINT_TRACE (2, print_string, "Trace: leaving close_print_obj_collstat_dict");
    return (0);
}
