#ifndef INTERH
#define INTERH
/*        $Header: /home/smart/release/src/h/inter.h,v 11.2 1993/02/03 16:47:32 smart Exp $*/

#include "proc.h"
#include "buf.h"
#include "retrieve.h"
#include "spec.h"
#include "textloc.h"

/* Maximum number of tokens on user supplied command line */
#define MAX_COMMAND_LINE 50

/*
 * An interactive command structure.
 * All interactive commands have the calling format,
 *
 *	proc( INTER_STATE *, char *unused, int inst )
 */
typedef struct {
    PROC_INST proc;           /* Procedure to do action. All procedures
                                return 1 upon success, UNDEF upon
                                system failure, 0 upon minor failure */
    char *name;              /* Name of command */
    int save_output;         /* If false, do not keep output of command
                                (cannot "save" it to a file) */
    char *explanation;       /* Description of action taken by name */
} INTER_COMMAND;

typedef struct {
    char *menu_name;
    char *menu_explanation;  /* Description of menu */
    long num_commands;
    INTER_COMMAND *commands;
} INTER_MENU;


/* Variables describing current state of interactive procedure */
typedef struct {
    SPEC *current_spec;      /* Current spec */
    SPEC *orig_spec;         /* Original specification file for this
                                instantiation of inter.  May not be the
                                same as current_spec if user has run the
                                Euse command */
    int verbose_level;       /* How much output to show user */
    int give_starting_help;  /* Display help to user upon startup */
    SM_BUF output_buf;       /* Valid output to be shown user */
    SM_BUF err_buf;          /* Error output to be shown user */

    char *command_line[MAX_COMMAND_LINE];  /* Current user command, broken 
                                down into tokens. */
    long num_command_line;   /* number of tokens in current command_line */

    SM_BUF query_text;       /* Current query text */
    RETRIEVAL retrieval;     /* Pointer to current query vector and
                                top docs retrieved for it */
    long current_doc;        /* doc_id last shown to user */
    long current_query;      /* query_id last shown to user */
    SPEC_LIST spec_list;     /* When examining experimental results, the
                                 list of runs */

    INTER_MENU *menu;        /* Lists of commands available to user */
    long num_menus;          /* Number of lists of commands */

} INTER_STATE;

int reset_is();
int init_show_titles(), close_show_titles(), inter_prepare_titles();
int inter_show_titles();

int init_inter_util(), close_inter_util(), inter_get_title(), inter_get_vec(),
    inter_get_textloc();

#endif /* INTERH */
