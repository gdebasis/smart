#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/inter.c,v 11.2 1993/02/03 16:51:23 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "inst.h"
#include "smart_error.h"
#include "spec.h"
#include "proc.h"
#include "trace.h"
#include "inter.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("inter.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


static INTER_COMMAND *parse_command_line();
static int show_output();
static int save_command_name();
int init_inter_is(), close_inter_is();
int help_message();

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    INTER_STATE is;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int
init_inter (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_inter");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    /* Initialize the current state, given the spec file.  Note that
       all of "is" is re-initialized upon an internal reset of the
       spec_file ("Euse" command), with the exception of the spec_list.
       and orig_spec */
    ip->is.spec_list.num_spec = 0;
    ip->is.orig_spec = spec;
    if (UNDEF == init_inter_is (spec, &ip->is))
        return (UNDEF);
    
    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_inter");
    return (new_inst);
}



int
inter (unused1, unused2, inst)
char *unused1, *unused2;
int inst;
{
    char in_buffer[PATH_LEN];   /* current command from user */
    INTER_COMMAND *command;
    STATIC_INFO *ip;
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering inter");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (ip->is.verbose_level && ip->is.give_starting_help) {
        (void) help_message (&ip->is, (char *) NULL, 0);
        (void) show_output (&ip->is, &ip->is.err_buf);
    }

    while (1) {

        if (ip->is.verbose_level) {
            (void) fputs ("\nSmart (ntq?): ", stdout);
            (void) fflush (stdout);
        }
        /* Discover what command to execute next */
        if (NULL == fgets (in_buffer, PATH_LEN, stdin)) {
            (void) fputs ("\n", stdout);
            return (0);
        }

        ip->is.err_buf.end = 0;

        if (NULL == (command = parse_command_line (&ip->is, in_buffer))) {
            (void) show_output (&ip->is, &ip->is.err_buf);
            continue;
        }

        if (command->save_output) {
            if (ip->is.verbose_level) {
                /* Save the current command line to be output later */
                if (UNDEF == save_command_name(&ip->is))
                    return (UNDEF);
            }
            else
                ip->is.output_buf.end = 0;
        }

        /* Make sure command is initialized */
        /* Run the new command.  Output goes to output_buf unless error */
        /* If major error, then exit, else print err_buf */
        if (UNDEF == command->proc.inst &&
            UNDEF == (command->proc.inst = command->proc.ptab->init_proc
                                           (ip->is.current_spec,
                                            (char *) &ip->is))) {
            print_error_buf("inter", "Initialization", &ip->is.err_buf);
            (void) show_output (&ip->is, &ip->is.err_buf);
        }
        else {
	    status = command->proc.ptab->proc (&ip->is, (char *) NULL,
					       command->proc.inst);
	    if (status == UNDEF || status == -2)
		break;
            if (command->save_output == 0 || status == 0) 
                (void) show_output (&ip->is, &ip->is.err_buf);
            else
                (void) show_output (&ip->is, &ip->is.output_buf);
        }
    }

    if (status == -2) {
        PRINT_TRACE (2, print_string, "Trace: leaving inter");
	return (0);
    }
    return (status);
}

int
close_inter (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_inter");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_inter");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (UNDEF == close_inter_is (&ip->is))
        return (UNDEF);

    ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_inter");
    return (0);
}

static int
show_output (is, buf)
INTER_STATE *is;
SM_BUF *buf;
{
    int num_written, newly_written;

    if (is->verbose_level) {
        if (UNDEF == unix_page (buf->buf, buf->end))
            return (UNDEF);
    }
    else {
        num_written = 0;
        while (num_written < buf->end) {
            if (-1 == (newly_written = fwrite (buf->buf + num_written,
                                               1,
                                               buf->end - num_written,
                                               stdout)))
                return (UNDEF);
            num_written += newly_written;
        }
    }
    return (1);
}

static 
INTER_COMMAND *
parse_command_line (is, buf)
INTER_STATE *is;
char *buf;
{
    int len;
    long i,j;
    long save_i, save_j;
    long num_match;
    char temp_buf[PATH_LEN];

    is->num_command_line = 0;
    while (*buf && isspace (*buf))	/* Omit leading space */
	buf++;

    if (*buf == '\0') {
	/* Default command is to show next doc */
	is->command_line[is->num_command_line++] = "next";
    }
    /* if a number was specified, want to print that doc */
    if ( *buf && *buf <= '9' && *buf >= '0')
        is->command_line[is->num_command_line++] = "dtext";

    while (*buf) {
	/* skip leading white space */
	while (*buf && isspace (*buf))
	    buf++;
	if (*buf == '\0')
	    break;

	is->command_line[is->num_command_line++] = buf;
	while (*buf && !isspace (*buf))
	    buf++;
	if (*buf)
	    *buf++ = '\0';
	if (is->num_command_line >= MAX_COMMAND_LINE)
	    break;
    }

    /* Find the desired command */
    len = strlen (is->command_line[0]);
    num_match = 0;
    save_i = save_j = -1;
    for (i = 0; i < is->num_menus; i++) {
        for (j = 0; j < is->menu[i].num_commands; j++) {
            if (0 == strncmp (is->command_line[0],
			      is->menu[i].commands[j].name,
			      len)) {
		if (num_match++ > 0) {
		    /* Error, multiple matches. Already have match */
		    if (num_match == 2) {
			(void) sprintf(temp_buf,
				       "Ambiguous command ignored. Possibilities include\n    %s\n",
				       is->menu[save_i].commands[save_j].name);
			if (UNDEF == add_buf_string (temp_buf, &is->err_buf))
			    return ((INTER_COMMAND *) NULL);
		    }
		    (void) sprintf(temp_buf,
				   "    %s\n",
				   is->menu[i].commands[j].name);
		    if (UNDEF == add_buf_string (temp_buf, &is->err_buf))
			return ((INTER_COMMAND *) NULL);
		    save_j = -1;
		}
		else {
		    save_j = j;
		    save_i = i;
		}
            }
        }
    }

    if (save_j != -1) {
	/* Have the output command name be the full name */
	is->command_line[0] = is->menu[save_i].commands[save_j].name;
	return (&is->menu[save_i].commands[save_j]);
    }

    if (save_i == -1) {
	(void) sprintf (temp_buf,
			"Command not found: '%s'\nLegal standard commands are\n\n",
			is->command_line[0]);
	if (UNDEF == add_buf_string (temp_buf, &is->err_buf))
	    return ((INTER_COMMAND *) NULL);
	(void) help_message (is, (char *) NULL, 0);
    }

    return ((INTER_COMMAND *) NULL);
}

/* Save the current command name and arguments in is->output_buf */
static int
save_command_name (is)
INTER_STATE *is;
{
    char temp_buf[PATH_LEN];
    char *ptr, *buf_ptr;
    long i;

    is->output_buf.end = 0;
    buf_ptr = temp_buf;
    for (ptr = "\n-------------------------\n"; *ptr;)
        *buf_ptr++ = *ptr++;
    for (ptr = is->command_line[0]; *ptr && !(isspace (*ptr));)
        *buf_ptr++ = *ptr++;
    *buf_ptr++ = ' ';
    for (i = 1; i < is->num_command_line; i++) {
        for (ptr = is->command_line[i]; *ptr;)
            *buf_ptr++ = *ptr++;
        *buf_ptr++ = ' ';
    }
    for (ptr = "\n----\n"; *ptr;)
        *buf_ptr++ = *ptr++;
    *buf_ptr++ = '\0';
    if (UNDEF == add_buf_string (temp_buf, &is->output_buf))
        return (UNDEF);
    return (1);
}

int
exit_inter ()
{
    return (-2);
}

int
quit_inter ()
{
    /* For now, no intermediate actions (eventually being collection
       maintainance - delete, edit, update user profile, etc); */
    return (exit_inter ());
}

