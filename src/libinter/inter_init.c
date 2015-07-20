#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/inter.c,v 11.2 1993/02/03 16:51:23 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "functions.h"
#include "inter.h"
#include "param.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "smart_error.h"


static long verbose;         /* Indicate level of description to user */
static long give_starting_help;
static char *spec_inter_menu;   /* Complete spec file giving interactive menu*/

static SPEC_PARAM spec_args[] = {
    {"inter.verbose",             getspec_long,    (char *) &verbose},
    {"inter.help_at_start",       getspec_bool,    (char *) &give_starting_help},
    {"inter.spec_inter_menu",     getspec_string,  (char *) &spec_inter_menu},
    TRACE_PARAM ("inter.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static long num_menus;
static SPEC_PARAM inter_spec_args[] = {
    {"inter.num_menus",       getspec_long,    (char *) &num_menus},
    };
static int inter_num_spec_args = sizeof (inter_spec_args) /
         sizeof (inter_spec_args[0]);

static char *prefix1;
static char *menu_name;
static long menu_num_entries;
static char *menu_help;

static SPEC_PARAM_PREFIX prefix1_spec_args[] = {
    {&prefix1, "menu_name",       getspec_string,  (char *) &menu_name},
    {&prefix1, "menu_num_entries",getspec_long,    (char *) &menu_num_entries},
    {&prefix1, "menu_help",       getspec_string,  (char *) &menu_help},
    };
static int num_prefix1_spec_args = sizeof (prefix1_spec_args) /
         sizeof (prefix1_spec_args[0]);

static char *prefix2;
static char *entry_name;
static PROC_TAB *entry_proc;
static char *entry_help;
static long entry_save_output;

static SPEC_PARAM_PREFIX prefix2_spec_args[] = {
    {&prefix2, "name",       getspec_string,  (char *) &entry_name},
    {&prefix2, "proc",       getspec_func,    (char *) &entry_proc},
    {&prefix2, "help",       getspec_string,  (char *) &entry_help},
    {&prefix2, "save_output",  getspec_bool,  (char *) &entry_save_output},
    };
static int num_prefix2_spec_args = sizeof (prefix2_spec_args) /
         sizeof (prefix2_spec_args[0]);


int
init_inter_is (spec, is)
SPEC *spec;
INTER_STATE *is;
{
    SPEC inter_spec;
    char temp_buf[128];
    long i,j;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering initialise_is");
    PRINT_TRACE (6, print_string, spec_inter_menu);

    is->output_buf.end = 0;
    is->output_buf.size = 0;
    is->err_buf.end = 0;
    is->err_buf.size = 0;

    if (NULL == (is->retrieval.query = (QUERY_VECTOR *)
                 malloc (sizeof (QUERY_VECTOR))) ||
        NULL == (is->retrieval.tr = (TR_VEC *)
                 malloc (sizeof (TR_VEC))))
        return (UNDEF);
    is->retrieval.query->vector = (char *) NULL;
    is->retrieval.query->qid = UNDEF;
    is->retrieval.tr->num_tr = 0;
    is->query_text.end = 0;
    is->current_spec = spec;
    is->verbose_level = verbose;
    is->give_starting_help = give_starting_help;
    is->num_command_line = 0;

    if (UNDEF == get_spec (spec_inter_menu, &inter_spec)) {
        set_error (SM_ILLPA_ERR, "Illegal spec_inter_menu", "inter_init");
        return (UNDEF);
    }

    if (UNDEF == lookup_spec (&inter_spec,
                              &inter_spec_args[0],
                              inter_num_spec_args))
        return (UNDEF);

    /* Copy and initialize all menus from inter_spec param */
    is->num_menus = num_menus;
    if (NULL == (is->menu = (INTER_MENU *)
                 malloc ((unsigned) num_menus * sizeof (INTER_MENU))))
        return (UNDEF);
    
    for (i = 0; i < num_menus; i++) {
	prefix1 = temp_buf;
        (void) sprintf (prefix1, "inter.%ld.", i);
        if (UNDEF == lookup_spec_prefix (&inter_spec,
                                         &prefix1_spec_args[0],
                                         num_prefix1_spec_args))
            return (UNDEF);
        is->menu[i].menu_name = menu_name;
        is->menu[i].menu_explanation = menu_help;
        /* is->menu[i].beginner_menu = FALSE; */
        is->menu[i].num_commands = menu_num_entries;

        if (NULL == (is->menu[i].commands = (INTER_COMMAND *)
                     malloc ((unsigned) menu_num_entries * sizeof (INTER_COMMAND))))
            return (UNDEF);
        for (j = 0; j < menu_num_entries; j++) {
	    prefix2 = temp_buf;
            (void) sprintf (prefix2, "inter.%ld.%ld.", i, j);
            if (UNDEF == lookup_spec_prefix (&inter_spec,
                                             &prefix2_spec_args[0],
                                             num_prefix2_spec_args))
                return (UNDEF);
            is->menu[i].commands[j].proc.ptab = entry_proc;
            is->menu[i].commands[j].proc.inst = UNDEF;
            is->menu[i].commands[j].explanation = entry_help;
            is->menu[i].commands[j].name = entry_name;
            is->menu[i].commands[j].save_output = entry_save_output;
        }
    }

    /* Freeing SPEC's is the one exception to the rule that the
       allocating routine is responsible for memory management */
    (void) free ((char *) inter_spec.spec_list);

    return (1);
}


int
close_inter_is (is)
INTER_STATE *is;
{
    long i,j;

    PRINT_TRACE (2, print_string, "Trace: entering close_inter_is");

    /* Close all command procedures */
    for (i = 0; i < is->num_menus; i++) {
        for (j = 0; j < is->menu[i].num_commands; j++) {
            if (UNDEF != (is->menu[i].commands[j].proc.inst)) {
                if (UNDEF == is->menu[i].commands[j].proc.ptab->close_proc
                    (is->menu[i].commands[j].proc.inst))
                    return (UNDEF);
                is->menu[i].commands[j].proc.inst = UNDEF;
            }
        }
    }

    if (is->output_buf.size > 0)
        (void) free (is->output_buf.buf);
    if (is->err_buf.size > 0)
        (void) free (is->err_buf.buf);

    (void) free((char *) is->retrieval.query);
    (void) free((char *) is->retrieval.tr);
    /* MEMORY LEAK - Cannot reclaim this space, since top-level */
    /* inter "command" variable may point into this space */
    /* for (i = 0; i < is->num_menus; i++) { */
    /*    (void) free((char *) is->menu[i].commands); */
    /* } */
    /* (void) free((char *) is->menu); */

    return (1);
}

/* Re-initialize commands according to (new) spec file spec */
int
reset_is (spec, is)
SPEC *spec;
INTER_STATE *is;
{
    long i,j;

    /* Close all command procedures */
    for (i = 0; i < is->num_menus; i++) {
        for (j = 0; j < is->menu[i].num_commands; j++) {
            if (UNDEF != (is->menu[i].commands[j].proc.inst)) {
                if (UNDEF == is->menu[i].commands[j].proc.ptab->close_proc
                    (is->menu[i].commands[j].proc.inst))
                    return (UNDEF);
                is->menu[i].commands[j].proc.inst = UNDEF;
            }
        }
    }

    is->current_spec = spec;

    return (1);
}


