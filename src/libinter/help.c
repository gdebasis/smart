#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/set_query.c,v 11.2 1993/02/03 16:51:26 smart Exp $";
#endif
 
/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 
 
   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/


#include "common.h"
#include "param.h"
#include "functions.h"
#include "spec.h"
#include "inter.h"
#include "smart_error.h"
#include "inst.h"
/* #include "trace.h" */

static int
show_help_menu (is, menu)
INTER_STATE *is;
INTER_MENU *menu;
{
    char temp_buf[PATH_LEN];
    long i;


    (void) sprintf (temp_buf, "\n    Commands for '%s' Menu: %s\n",
		    menu->menu_name,
		    menu->menu_explanation);
    if (UNDEF == add_buf_string (temp_buf, &is->err_buf))
	return (UNDEF);

    for (i = 0; i < menu->num_commands; i++ ) {
        (void) sprintf (temp_buf,
                        "%-12s: %s\n",
                        menu->commands[i].name,
                        menu->commands[i].explanation);
        if (UNDEF == add_buf_string (temp_buf, &is->err_buf))
            return (UNDEF);
    }
    return (0);
}

int
help_message (is, unused, unused2)
INTER_STATE *is;
char *unused;
int unused2;
{
    char temp_buf[PATH_LEN];
    long i;

    if (is->num_menus <= 0) {
        if (UNDEF == add_buf_string ("No Menus defined\n", &is->err_buf))
            return (UNDEF);
	return (0);
    }
    if (is->num_command_line <= 1 || is->num_menus == 1) {
	if (UNDEF == show_help_menu (is, &is->menu[0]))
            return (UNDEF);
    }
    else {
	if (0 == strcmp (is->command_line[1], "all")) {
	    for (i = 0; i < is->num_menus; i++) {
		if (UNDEF == show_help_menu (is, &is->menu[i]))
		    return (UNDEF);
	    }
	}
	else {
	    for (i = 0; i < is->num_menus; i++) {
		if (0 == strcmp (is->command_line[1], is->menu[i].menu_name)) {
		    if (UNDEF == show_help_menu (is, &is->menu[i]))
			return (UNDEF);
		    break;
		}
	    }
	    if (i >= is->num_menus) {
		if (UNDEF == show_help_menu (is, &is->menu[0]))
		    return (UNDEF);
	    }
	}
    }	    

    if (is->num_menus > 1) {
	if (UNDEF == add_buf_string ("\nHelp is available for menus",
				     &is->err_buf))
	    return (UNDEF);
	for (i = 0; i < is->num_menus; i++ ) {
	    (void) sprintf (temp_buf,
			    " %s",
			    is->menu[i].menu_name);
	    if (UNDEF == add_buf_string (temp_buf, &is->err_buf))
		return (UNDEF);
	}
    }
    return (0);
}
