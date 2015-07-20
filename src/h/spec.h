#ifndef SPECH
#define SPECH
/*        $Header: /home/smart/release/src/h/spec.h,v 11.2 1993/02/03 16:47:47 smart Exp $*/
/*        (c) Copyright 1990 Chris Buckley */


typedef struct {
    char *name;
    char *value;
    char *key_field;         /* Last field of name (this must match when
                                looking up parameters) */
    long num_fields;         /* Number of fields within name, counting 
                                key_field */
} SPEC_TUPLE;

typedef struct {
    int num_list;
    SPEC_TUPLE *spec_list;
    int spec_id;             /* Unique nonzero id assigned to each SPEC
                                instantiation to aid in caching values */
} SPEC;

typedef struct {
    char *param;             /* Parameter name to look up in spec file */
    int (*convert) ();       /* Conversion routine to run on value */
                             /* corresponding to param */
    char *result;            /* Location of put result of conversion */
} SPEC_PARAM;


typedef struct {
    char **prefix;           /* Pointer to string to be prepended to */
                             /* param before lookup */
    char *param;             /* Base parameter name to look up in spec file */
    int (*convert) ();       /* Conversion routine to run on value */
                             /* corresponding to param */
    char *result;            /* Location of put result of conversion */
} SPEC_PARAM_PREFIX;


/* A list of pointers to spec_files and their associated file_names */
typedef struct {
    SPEC **spec;             /* spec[i] is a pointer to the i'th spec file */
    char **spec_name;        /* Original filename of spec[i] */
    int num_spec;            /* number of files in the list */
} SPEC_LIST;

#define PARAM_BOOL(param,var)      {param, getspec_bool, (char *)var}
#define PARAM_DBFILE(param,var)    {param, getspec_dbfile, (char *)var}
#define PARAM_DOCDESC(param,var)   {param, getspec_docdesc, (char *)var}
#define PARAM_DOUBLE(param,var)    {param, getspec_double, (char *)var}
#define PARAM_FILEMODE(param,var)  {param, getspec_filemode, (char *)var}
#define PARAM_FLOAT(param,var)     {param, getspec_float, (char *)var}
#define PARAM_FUNC(param,var)      {param, getspec_func, (char *)var}
#define PARAM_INTSTRING(param,var) {param, getspec_intstring, (char *)var}
#define PARAM_LONG(param,var)      {param, getspec_long, (char *)var}
#define PARAM_STRING(param,var)    {param, getspec_string, (char *)var}

#endif /* SPECH */




