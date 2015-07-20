#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/pp_pnorm.c,v 11.0 1992/07/21 18:21:42 chrisb Exp $";
#endif
/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Pnorm pre-parser for pnorm queries.
 *1 index.preparse.pnorm
 *2 pp_pnorm (unused1, unused2, unused3)
 *3   char *unused1;
 *3   char *unused2;
 *3   int unused3;
***********************************************************************/

#include "functions.h"
#include "common.h"
#include "spec.h"
#include "trace.h"

static char *pp_outfile;	/* file name to store preparsed result */

static SPEC_PARAM spec_args[] = {
    {"index.preparse.pp_outfile",getspec_dbfile,(char *) &pp_outfile},
    TRACE_PARAM ("index.trace")
    };
static int num_spec_args = sizeof(spec_args) / sizeof(spec_args[0]);

static FILE	*pp_infile_fp;
static FILE	*queryfile_fp;
static FILE	*pp_outfile_fp;

extern FILE	*ppin;		/* equivalent to yyin */
extern FILE	*ppout;		/* equivalent to yyout */
extern int	pplex();	/* equivalent to yylex */

int
init_pp_pnorm (spec, pp_infile)
SPEC	*spec;
char	*pp_infile;
{
    char	queryfile[1024];

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_pp_pnorm");

    /* open file giving list of files to be parsed */
    if (NULL == (pp_infile_fp = fopen(pp_infile, "r"))) {
        return (UNDEF);
    }

    /* get a new query file for the next query */
    if (NULL == fgets(queryfile, 1024, pp_infile_fp)) {
        return (UNDEF);
    }

    /* Remove trailing \n from query file name */
    queryfile[strlen(queryfile) - 1] = '\0';

    if (NULL == (queryfile_fp = fopen(queryfile, "r"))) {
        fprintf (stderr, "can't find the query file : %s\n", queryfile);
        return(UNDEF);
    }

    /* open preparsed output file */
    pp_outfile_fp = fopen(pp_outfile, "w");

    PRINT_TRACE (2, print_string, "Trace: leaving init_pp_pnorm");
    return(0);
} /* init_pp_pnorm */


int
pp_pnorm (unused1, unused2, unused3)
char	*unused1;
char	*unused2;
int	unused3;
{
    PRINT_TRACE (2, print_string, "Trace: entering pp_pnorm");

    ppin = queryfile_fp;
    ppout = pp_outfile_fp;
    pplex();

    PRINT_TRACE (2, print_string, "Trace: leaving pp_pnorm");
    return (1);
} /* pp_pnorm */


int
close_pp_pnorm (inst)
int	inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_pp_pnorm");

    fclose(pp_infile_fp);
    fclose(queryfile_fp);
    fclose(pp_outfile_fp);

    PRINT_TRACE (2, print_string, "Trace: leaving close_pp_pnorm");
    return (0);
} /* close_pp_pnorm */


int
ppwrap()
{
    char    queryfile[1024];

    if (NULL == fgets(queryfile, 1024, pp_infile_fp))
        return(1);      /* no more query file */

    /* Remove trailing \n from query file name */
    queryfile[strlen(queryfile) - 1] = '\0';

    /* close the file opend right before */
    fclose(queryfile_fp);

    /* open a new query file */
    if (NULL == (queryfile_fp = fopen(queryfile, "r"))) {
        fprintf (stderr, "can't find the query file : %s\n", queryfile);
        exit(1);
    }

    ppin = queryfile_fp;
    return(0);

} /* ppwrap */

