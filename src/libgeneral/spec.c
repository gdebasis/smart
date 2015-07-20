#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libgeneral/spec.c,v 11.2 1993/02/03 16:50:38 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Read the specification file file_name into the spec object in_spec_ptr
 *2 get_spec (file_name, in_spec_ptr)
 *3   char *file_name;
 *3   SPEC *in_spec_ptr;

 *7 FORMAT:
 *7 The format of the spec file is lines of the form
 *7     parameter_name  parameter_value
 *7  
 *7 Blank lines are ignored.  Lines beginning with a '#' are ignored.
 *7 Leading blanks are ignored.  If the parameter value has a blank or
 *7 a tab in it, the entire parameter value must be enclosed in
 *7 double quotes.
 *7 
 *7 PARAMETER_NAME
 *7 The parameter name is given by a dotted tuple.  Examples would
 *7 be in the spec file lines
 *7         index.query_file.rwmode  SRDWR|SINCORE|SCREATE
 *7         query_file.rmode         SRDONLY|SINCORE
 *7         rmode                    SRDONLY|SINCORE
 *7         rwmode                   SRDWR
 *7 Here, "index.query_file.rwmode" is a parameter name specifying
 *7 what filemode a query file should be stored as when indexing.
 *7 (The parameter value indicates the query file should be opened
 *7 read-write, should be kept in memory while being indexed, and
 *7 will be a new file that will need to be created.)
 *7 
 *7 The key field of the parameter_name is the last field.  The
 *7 preceding fields contain restrictions of when the key field
 *7 matches.
 *7 typedef struct {
 *7     int num_list;
 *7     SPEC_TUPLE *spec_list;
 *7     int spec_id;             * Unique nonzero id assigned to each SPEC
 *7                                 instantiation to aid in caching values *
 *7 } SPEC;
 *7 
 *7 If there is a parameter called "include_file", then the value for that
 *7 parameter is interpreted to be a specification file that should be read
 *7 to obtain more parameter pairs.  These pairs are added BEFORE any other
 *7 parameters from this call are added.  Ie, if a parameter is given a
 *7 value in both the include_file and this call, the parameter value from
 *7 this call will supplant the one from the include file.
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "io.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"

#define INIT_SPEC_ALLOC 150
static int parse_spec(), merge_spec();
static int add_spec();

static int cmp_spec();
static char *lookup_include_file();
static char *get_key_field();
static long get_num_field();
static int get_include_files();

static int current_spec_id = 0;  /* Unique id assigned to each SPEC
                                    instantiation to aid in caching.
                                    Implemented just as a counter set
                                    in get_spec() and mod_spec(); */

int
get_spec (file_name, in_spec_ptr)
char *file_name;
SPEC *in_spec_ptr;
{
    char *buffer;
    int fd;
    unsigned long file_size;
    SPEC new_spec;
    SPEC_TUPLE *temp_spec_list;
    char *include_file;            /* if specified in the current spec file,
                                      a spec file to include before current
                                      one takes effect. */

    /* Open file_name */
    if (-1 == (fd = open (file_name, SRDONLY))) {
        set_error (errno, file_name, "get_spec");
        return (UNDEF);
    }

    /* Allocate space for it (will bring directly into memory) */
    file_size = lseek (fd, 0L, 2);
    (void) lseek (fd, 0L, 0);
    if (NULL == (buffer = malloc ((unsigned) file_size+10))) {
        set_error (errno, file_name, "get_spec");
        return (UNDEF);
    }

    /* Read into memory, ensure it is terminated by NULL */
    if (file_size != read (fd, buffer, (int) file_size)) {
        set_error (errno, file_name, "get_spec");
        return (UNDEF);
    }
    buffer[file_size++] = '\0';

    if (-1 == close (fd)) {
        set_error (errno, file_name, "get_spec");
        return (UNDEF);
    }

    /* Create sorted spec list by assigning pointers into buffer */
    if (UNDEF == parse_spec (buffer, &buffer[file_size], in_spec_ptr)) {
        return (UNDEF);
    }

    /* Check to see if there's a param called "include_file". If so, then
       read the file specified there as an initial spec, and then merge
       the new_spec values in */
    if (NULL != (include_file = lookup_include_file (in_spec_ptr))) {
        if (UNDEF == get_include_files (include_file, &new_spec))
            return (UNDEF);
	temp_spec_list = new_spec.spec_list;
        if (UNDEF == merge_spec (&new_spec, in_spec_ptr)) {
            return (UNDEF);
        }
	(void) free ((char *) in_spec_ptr->spec_list);
	(void) free ((char *) temp_spec_list);
        in_spec_ptr->num_list = new_spec.num_list;
        in_spec_ptr->spec_list = new_spec.spec_list;
    }

    in_spec_ptr->spec_id = ++current_spec_id;
    return (0);
}

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Copy a spec object
 *2 copy_spec (new_spec_ptr, old_spec_ptr)
 *3   SPEC *new_spec_ptr;
 *3   SPEC *old_spec_ptr;
 *7 Copy top-level portions of old_spec into new_spec.  Same actual list can
 *7 be used since forbidden to be freed or modified by application code,
 *7 and not freed or modified by other procs here.
***********************************************************************/

int
copy_spec (new_spec_ptr, old_spec_ptr)
SPEC *new_spec_ptr;
SPEC *old_spec_ptr;
{
    new_spec_ptr->num_list = old_spec_ptr->num_list;
    new_spec_ptr->spec_id = old_spec_ptr->spec_id;
    new_spec_ptr->spec_list = old_spec_ptr->spec_list;
    return (0);
}


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Add additional spec pairs to the existing spec object
 *2 mod_spec (in_spec_ptr, num_args, args)
 *3   SPEC *in_spec_ptr;
 *3   int num_args;
 *3   char *args[];
 *7 args is assumed to be character strings, alternating between parameter
 *7 name and paramter values.  Each pair of strings will be added to, or 
 *7 replace, the previous list of specification pairs found in in_spec_ptr.
 *7 in_spec_ptr->spec_id is changed to uniquely identify this set of spec 
 *7 values.
 *7 If there is a parameter called "include_file", then the value for that
 *7 parameter is interpreted to be a specification file that should be read
 *7 to obtain more parameter pairs.  These pairs are added BEFORE any other
 *7 parameters from this call are added.  Ie, if a parameter is given a
 *7 value in both the include_file and this call, the parameter value from
 *7 this call will supplant the one from the include file.
 ***********************************************************************/
 
int
mod_spec (in_spec_ptr, num_args, args)
SPEC *in_spec_ptr;
int num_args;
char *args[];
{
    SPEC_TUPLE *new_spec_list, *spec_ptr, *temp_spec_list;
    long i;
    SPEC new_spec;
    SPEC inc_spec;
    char *include_file;

    if (NULL == (new_spec_list = (SPEC_TUPLE *) 
                 malloc (sizeof (SPEC_TUPLE) * (unsigned) num_args))) {
        set_error (errno, "malloc", "mod_spec");
        return (UNDEF);
    }
    spec_ptr = new_spec_list;

    for (i = 0; i < num_args - 1; i += 2) {
        spec_ptr->name = args[i];
        spec_ptr->value = args[i+1];
        spec_ptr->key_field = get_key_field (spec_ptr->name);
        spec_ptr->num_fields = get_num_field (spec_ptr->name);
        spec_ptr++;
    }

    qsort ((char *) new_spec_list, spec_ptr - new_spec_list,
           sizeof (SPEC_TUPLE), cmp_spec);

    /* Check to see if there's a param called "include_file". If so, then
       read the file(s) specified there as an initial spec, and then merge
       the new_spec values in */
    inc_spec.spec_list = new_spec_list;
    inc_spec.num_list = spec_ptr - new_spec_list;
    if (NULL != (include_file = lookup_include_file (&inc_spec))) {
        if (UNDEF == get_include_files (include_file, &new_spec))
            return (UNDEF);
	temp_spec_list = new_spec.spec_list;
        if (UNDEF == merge_spec (&new_spec, &inc_spec)) {
            return (UNDEF);
        }
	(void) free ((char *) new_spec_list);
	(void) free ((char *) temp_spec_list);
    }
    else {
        new_spec.spec_list = new_spec_list;
        new_spec.num_list = spec_ptr - new_spec_list;
    }

    if (UNDEF == merge_spec (in_spec_ptr, &new_spec)) {
        return (UNDEF);
    }
    (void) free ((char *) new_spec.spec_list);

    in_spec_ptr->spec_id = ++current_spec_id;
    return (0);
}


/* Merge old_spec and new_spec into old_spec */
/* The spec lists for both are assumed to be sorted by key field and 
   then by name.  If a param is in both
   old_spec and new_spec, the new_spec value only is used. */
/* Don't know the usage of the old_spec list, therefore can't assume it
   can be freed.  New_spec list must be from procedures above, and can be
   freed there */
static int
merge_spec (old_spec, new_spec)
SPEC *old_spec;                
SPEC *new_spec;
{
    SPEC_TUPLE *merged_spec_list, *spec_ptr;
    SPEC_TUPLE *new_ptr, *end_new;
    SPEC_TUPLE *old_ptr, *end_old;
    int cmp_result;

    if (NULL == (merged_spec_list = (SPEC_TUPLE *) 
                 malloc (sizeof (SPEC_TUPLE) *
                         (unsigned)(old_spec->num_list+new_spec->num_list)))) {
        set_error (errno, "malloc", "merge_spec");
        return (UNDEF);
    }
    spec_ptr = merged_spec_list;

    if (old_spec->num_list > 0) {
	old_ptr = old_spec->spec_list;
	end_old = &old_spec->spec_list[old_spec->num_list];
    }
    else
	old_ptr = end_old = (SPEC_TUPLE *) NULL;
    if (new_spec->num_list > 0) {
	new_ptr = new_spec->spec_list;
	end_new = &new_spec->spec_list[new_spec->num_list];
    }
    else
	new_ptr = end_new = (SPEC_TUPLE *) NULL;

    while (old_ptr < end_old && new_ptr < end_new) {
        cmp_result = cmp_spec (old_ptr, new_ptr);
        if (cmp_result < 0) {
            spec_ptr->name = old_ptr->name;
            spec_ptr->value = old_ptr->value;
            spec_ptr->key_field = old_ptr->key_field;
            spec_ptr->num_fields = old_ptr->num_fields;
            old_ptr++;
        }
        else {
            spec_ptr->name = new_ptr->name;
            spec_ptr->value = new_ptr->value;
            spec_ptr->key_field = new_ptr->key_field;
            spec_ptr->num_fields = new_ptr->num_fields;
            new_ptr++;
            if (cmp_result == 0)
                old_ptr++;
        }
        spec_ptr++;
    }
    while (old_ptr < end_old) {
        spec_ptr->name = old_ptr->name;
        spec_ptr->value = old_ptr->value;
        spec_ptr->key_field = old_ptr->key_field;
        spec_ptr->num_fields = old_ptr->num_fields;
        old_ptr++;
        spec_ptr++;
    }
    while (new_ptr < end_new) {
        spec_ptr->name = new_ptr->name;
        spec_ptr->value = new_ptr->value;
        spec_ptr->key_field = new_ptr->key_field;
        spec_ptr->num_fields = new_ptr->num_fields;
        new_ptr++;
        spec_ptr++;
    }

    old_spec->spec_list = merged_spec_list;
    old_spec->num_list = spec_ptr - merged_spec_list;
    return (0);
}

/* Get last field (fields designated by intervening '.') of parameter name */
static char *
get_key_field (name)
char *name;
{
    char *start_ptr, *ptr;

    start_ptr = ptr = name;
    while (*ptr) {
        if (*ptr == '.')
            start_ptr = ptr + 1;
        ptr++;
    }
    return (start_ptr);
}
/* Get num_field (fields designated by intervening '.') of parameter name */
static long
get_num_field (name)
char *name;
{
    long count = 1;
    while (*name) {
        if (*name == '.')
            count++;
        name++;
    }
    return (count);
}

#define INCLUDE_FILE "include_file"

/* Search sorted spec list for occurrence of INCLUDE_FILE as a parameter.
   If found, return the filename associated with it */
/* BUG? Should be relative to database directory? */
static char *
lookup_include_file (spec)
SPEC *spec;
{
    int i = 0;
    while (i < spec->num_list &&
           0 > strcmp (spec->spec_list[i].key_field, INCLUDE_FILE)) {
        i++;
    }
    if (i < spec->num_list &&
        0 == strcmp (spec->spec_list[i].key_field, INCLUDE_FILE)) {
        spec->spec_list[i].key_field = "include_file_old";
        return (spec->spec_list[i].value);
    }
    return (NULL);
}



#define BEGIN_LINE 0
#define PARM_NAME  1
#define BETWEEN    2
#define PARM_VALUE 3
#define PARM_Q_VALUE 4
#define COMMENT    5
    
static int
parse_spec (ptr, end_ptr, in_spec)
char *ptr;
char *end_ptr;
SPEC *in_spec;
{
    SPEC_TUPLE *new_spec_list, *spec_ptr, *end_spec_ptr;
    int state = BEGIN_LINE;
    int current_size;
    int error_occurred = 0;
    int i;

    /* Allocate space for pointers into buffer (may have to realloc later) */
    if (NULL == (new_spec_list = (SPEC_TUPLE *) 
                 malloc (sizeof (SPEC_TUPLE) * INIT_SPEC_ALLOC))) {    
        set_error (errno, "", "get_spec");
        return (UNDEF);
    }
    end_spec_ptr = &new_spec_list[INIT_SPEC_ALLOC];
    spec_ptr = new_spec_list;

    /* Small finite automata recognizing line */
    /* <whitespace>*<name><whitespace>+<value><whitespace>*<newline> */
    /* where <value> contains no whitespace or is surrounded by quotes. */
    /* Comments being with a '#' and continue to the end-of-line */
    /* If <name> occurs on a line, then <value> must also. */
    while (ptr < end_ptr) {
        switch (state) {
            case BEGIN_LINE:
                switch (*ptr) {
                    case ' ':
                    case '\t':
                    case '\0':
                    case '\n':
                        break;
                    case '#':
                        state = COMMENT;
                        break;
                    default:
                        spec_ptr->name = ptr;
                        spec_ptr->key_field = ptr;
                        state = PARM_NAME;
                        break;
                }
                break;
            case PARM_NAME:
                switch (*ptr) {
                    case ' ':
                    case '\t':
                    case '\0':
                        *ptr = '\0';
                        state = BETWEEN;
                        break;
                    case '\n':
                        *ptr = '\0';
                        set_error (SM_INCON_ERR, 
                                   "Empty field value",
                                   spec_ptr->name);
                        error_occurred = 1;
                        state = BEGIN_LINE;
                        break;
                    case '#':
                        *ptr = '\0';
                        set_error (SM_INCON_ERR, 
                                   "Empty field value",
                                   spec_ptr->name);
                        error_occurred = 1;
                        state = COMMENT;
                        break;
                    case '.':
                        /* New field. Reset key field pointer */
                        spec_ptr->key_field = ptr+1;
                        break;
                    default:
                        break;
                }
                break;
            case BETWEEN:
                switch (*ptr) {
                    case ' ':
                    case '\t':
                    case '\0':
                        break;
                    case '\n':
                        *ptr = '\0';
                        set_error (SM_INCON_ERR, 
                                   "Empty field value",
                                   spec_ptr->name);
                        error_occurred = 1;
                        state = BEGIN_LINE;
                        break;
                    case '#':
                        *ptr = '\0';
                        set_error (SM_INCON_ERR, 
                                   "Empty field value",
                                   spec_ptr->name);
                        error_occurred = 1;
                        state = COMMENT;
                        break;
                    case '"':
                        spec_ptr->value = ptr+1;
                        state = PARM_Q_VALUE;
                        break;
                    default:
                        spec_ptr->value = ptr;
                        state = PARM_VALUE;
                        break;
                }
                break;
            case PARM_VALUE:
                switch (*ptr) {
                    case ' ':
                    case '\t':
                        *ptr = '\0';
                        break;
                    case '\0':
                    case '\n':
                        *ptr = '\0';
                        if (++spec_ptr >= end_spec_ptr) {
                            current_size = end_spec_ptr - new_spec_list;
                            if (NULL == (new_spec_list = (SPEC_TUPLE *) 
                                         realloc ((char *) new_spec_list,
                                                  sizeof (SPEC_TUPLE) *
                                                  (unsigned) (current_size +
                                                   INIT_SPEC_ALLOC)))) {
                                set_error (errno, "", "get_spec");
                                return (UNDEF);
                            }
                            spec_ptr = new_spec_list + current_size;
                            end_spec_ptr = spec_ptr + INIT_SPEC_ALLOC;
                        }
                        state = BEGIN_LINE;
                        break;
                    case '#':
                        *ptr = '\0';
                        if (++spec_ptr >= end_spec_ptr) {
                            current_size = end_spec_ptr - new_spec_list;
                            if (NULL == (new_spec_list = (SPEC_TUPLE *) 
                                         realloc ((char *) new_spec_list,
                                                  sizeof (SPEC_TUPLE) *
                                                  (unsigned) (current_size +
                                                   INIT_SPEC_ALLOC)))) {
                                set_error (errno, "", "get_spec");
                                return (UNDEF);
                            }
                            spec_ptr = new_spec_list + current_size;
                            end_spec_ptr = spec_ptr + INIT_SPEC_ALLOC;
                        }
                        state = COMMENT;
                        break;
                    default:
                        break;
                }
                break;
            case PARM_Q_VALUE:
                switch (*ptr) {
                    case '\\':
			*ptr = ' ';
			ptr++;
			break;
                    case '\n':
                    case '#':
                        *ptr = '\0';
                        set_error (SM_INCON_ERR, 
                                   "Unended quoted value (get_spec)",
                                   spec_ptr->name);
                        error_occurred = 1;
                        state = COMMENT;
                        break;
                    case '"':
                        *ptr = '\0';
                        state = PARM_VALUE;
                        break;
                    default:
                        break;
                    }
                break;
            case COMMENT:
                if (*ptr == '\n') {
                    state = BEGIN_LINE;
                }
                break;
            }
        ptr++;
    }

    
    in_spec->spec_list = new_spec_list;
    in_spec->num_list = spec_ptr - new_spec_list;

    for (i = 0; i < in_spec->num_list; i++)
        new_spec_list[i].num_fields = get_num_field (new_spec_list[i].name);

    (void) qsort ((char *) new_spec_list, in_spec->num_list,
           sizeof (SPEC_TUPLE), cmp_spec);


    return (error_occurred ? UNDEF : 0);
}


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Given a list of spec_file names, return the list associated spec values
 *2 get_spec_list (spec_files, spec_list)
 *3   char *spec_files;
 *3   SPEC_LIST *spec_list;
 *7 Spec_files is a list of filenames (possibly empty) separated by 
 *7 whitespace.  Get_spec is called on each filename, with the result
 *7 being placed in spec_list. The number of spec_files read is returned. 
***********************************************************************/

int
get_spec_list (spec_files, spec_list)
char *spec_files;
SPEC_LIST *spec_list;
{
    char *ptr, *start_spec;

    if (spec_files == NULL)
        return (0);

    if (NULL == (spec_list->spec = (SPEC **) malloc (sizeof (SPEC *))) ||
        NULL == (spec_list->spec_name = (char **) malloc (sizeof (char *))))
        return (UNDEF);
    spec_list->num_spec = 0;

    ptr = spec_files;
    while (*ptr) {
        while (*ptr && isspace (*ptr))
            ptr++;
        start_spec = ptr;
        while (*ptr && ! isspace (*ptr))
            ptr++;
        if (UNDEF == add_spec (spec_list, start_spec, ptr - start_spec))
            return (UNDEF);
    }

    return (spec_list->num_spec);
}


static int
add_spec (spec_list, spec_file, len)
SPEC_LIST *spec_list;
char *spec_file;
int len;
{
    char *spec_file_ptr;
    SPEC *spec_ptr;
    if (len <= 0)
        return (0);

    if (NULL == (spec_file_ptr = malloc ((unsigned) len + 1)) ||
        NULL == (spec_ptr = (SPEC *) malloc (sizeof (SPEC))))
        return (UNDEF);

    (void) strncpy (spec_file_ptr, spec_file, len);
    spec_file_ptr[len] = '\0';

    if (NULL == (spec_list->spec = (SPEC **)
                 realloc ((char *) spec_list->spec,
                          (unsigned) (spec_list->num_spec + 1)
                          * sizeof (SPEC *))))
        return (UNDEF);
    if (NULL == (spec_list->spec_name = (char **)
                 realloc ((char *) spec_list->spec_name,
                          (unsigned) (spec_list->num_spec + 1)
                          * sizeof (char *))))
        return (UNDEF);

    if (UNDEF == get_spec (spec_file_ptr, spec_ptr))
        return (UNDEF);

    spec_list->spec_name[spec_list->num_spec] = spec_file_ptr;
    spec_list->spec[spec_list->num_spec] = spec_ptr;
    spec_list->num_spec++;

    return (1);
}

static int
get_include_files (filenames, spec_ptr)
char *filenames;
SPEC *spec_ptr;
{
    char buf[PATH_LEN];
    SPEC new_spec;
    char *buf_ptr;
    SPEC_TUPLE *temp_spec_list;

    spec_ptr->num_list = 0;
    spec_ptr->spec_list = (SPEC_TUPLE *) NULL;

    while (*filenames) {
        buf_ptr = buf;
        while (*filenames && isspace(*filenames))
            filenames++;
        while (*filenames && ! isspace(*filenames))
            *buf_ptr++ = *filenames++;
        *buf_ptr = '\0';
        if (buf[0]) {
            if (UNDEF == get_spec (buf, &new_spec))
                return (UNDEF);
	    temp_spec_list = spec_ptr->spec_list;
            if (UNDEF == merge_spec (spec_ptr, &new_spec))
                return (UNDEF);
	    (void) free ((char *) new_spec.spec_list);
	    if (temp_spec_list != (SPEC_TUPLE *) NULL)
		(void) free ((char *) temp_spec_list);
        }
    }
    return (1);
}

static int
cmp_spec (spec1, spec2)
SPEC_TUPLE *spec1;
SPEC_TUPLE *spec2;
{
    int status;
    status = strcmp (spec1->key_field, spec2->key_field);
    if (status)
        return (status);
    if (spec1->num_fields < spec2->num_fields)
        return (1);
    if (spec1->num_fields > spec2->num_fields)
        return (-1);
    return (strcmp (spec1->name, spec2->name));
}

