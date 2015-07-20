
# line 2 "pparser.y"
#ifdef RCSID
static char rcsid[] = "$Header: pparser.y,v 8.0 85/06/03 17:38:04 smart Exp $";
#endif

/* Copyright (c) 1984 - Gerard Salton, Chris Buckley, Ellen Voorhees */

#include <stdio.h>
#include "functions.h"
#include "param.h"
#include "rel_header.h"
#include "dict.h"
#include "vector.h"
#include "pnorm_vector.h"
#include "pnorm_common.h"
#include "pnorm_indexing.h"
#include "common.h"
#include "spec.h"
#include "trace.h"
#undef MAXLONG

#define ALLOC_NODE_INFO(ni_ptr) { \
          if (ni_ptr-node_info == node_info_size) { \
            node_info_size *= 2; \
            if (NULL == (node_info = (NODE_INFO *) \
               realloc((char *)node_info,node_info_size*sizeof(NODE_INFO)))){\
              print_error("pparser","Quit"); \
              exit(1); \
            } \
	    ni_ptr = node_info + (node_info_size/2); \
          } \
        }

#define ALLOC_TREE(num_used) { \
          if (num_used == num_tree_nodes) { \
            num_tree_nodes *= 2; \
            if (NULL == (char *)(tree = (TREE_NODE *) \
                realloc((char *)tree,num_tree_nodes*sizeof(TREE_NODE)))) { \
              print_error("pparser","Quit"); \
              exit(1); \
            } \
          } \
        }

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.trace")
    };
static int num_spec_args = sizeof(spec_args) / sizeof(spec_args[0]);

static short qid;
static NODE_INFO *ni_ptr;
static TREE_NODE *tree_ptr;
static long num_used_nodes;

extern SPEC *pnorm_coll_spec;
extern NODE_INFO *node_info;
extern unsigned node_info_size;
extern TREE_NODE *tree;
extern unsigned num_tree_nodes;
extern PNORM_VEC qvector;
extern int dict_index;
extern char yytext[];
extern int fix_up();
extern int make_vector();


# line 68 "pparser.y"
typedef union
#ifdef __cplusplus
	YYSTYPE
#endif
 {
  short  ival;
  double dval;
  } YYSTYPE;
# define QID 1
# define OP 2
# define NOT 3
# define INF 4
# define NUMBER 5
# define KEYWORD 6
# define LP 7
# define RP 8
# define COMMA 9
# define SEMI 10

#ifdef __STDC__
#include <stdlib.h>
#include <string.h>
#else
#include <malloc.h>
#include <memory.h>
#endif

#include <values.h>

#ifdef __cplusplus

#ifndef yyerror
	void yyerror(const char *);
#endif

#ifndef yylex
#ifdef __EXTERN_C__
	extern "C" { int yylex(void); }
#else
	int yylex(void);
#endif
#endif
	int yyparse(void);

#endif
#define yyclearin yychar = -1
#define yyerrok yyerrflag = 0
extern int yychar;
extern int yyerrflag;
YYSTYPE yylval;
YYSTYPE yyval;
typedef int yytabelem;
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 150
#endif
#if YYMAXDEPTH > 0
int yy_yys[YYMAXDEPTH], *yys = yy_yys;
YYSTYPE yy_yyv[YYMAXDEPTH], *yyv = yy_yyv;
#else	/* user does initial allocation */
int *yys;
YYSTYPE *yyv;
#endif
static int yymaxdepth = YYMAXDEPTH;
# define YYERRCODE 256

# line 244 "pparser.y"

yyerror(s)
  char *s;

  {
    fprintf(stderr,"pparser: syntax error.\n"); 
    fprintf(stderr,"error string: '%s', qid: %d, yytext: '%s'\n",
      s,qid,yytext);
    exit(1);
  }
yytabelem yyexca[] ={
-1, 1,
	0, -1,
	-2, 0,
	};
# define YYNPROD 16
# define YYLAST 256
yytabelem yyact[]={

     3,     8,    24,    25,    20,    14,     7,    26,    13,    12,
    21,    15,     9,    17,    18,     6,     2,    19,     5,    16,
    22,    11,    23,    10,     1,     0,     0,    27,    28,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     4 };
yytabelem yypact[]={

    -1,    -1,-10000000,-10000000,    -4,-10000000,     6,-10000000,    -5,-10000000,
-10000000,-10000000,     4,     9,-10000000,     6,    -3,-10000000,-10000000,     2,
     6,-10000000,    -6,    -2,-10000000,     6,     6,-10000000,-10000000 };
yytabelem yypgo[]={

     0,    24,    16,     1,    23,    21,    20,    19,    15 };
yytabelem yyr1[]={

     0,     1,     1,     8,     2,     2,     3,     3,     3,     4,
     5,     7,     7,     7,     6,     6 };
yytabelem yyr2[]={

     0,     2,     4,     1,     9,     5,     3,     3,     3,     9,
    11,     3,     3,     1,     7,     7 };
yytabelem yychk[]={

-10000000,    -1,    -2,     1,   256,    -2,    -8,    10,    -3,     6,
    -4,    -5,     3,     2,    10,     7,    -7,     4,     5,    -3,
     7,     8,    -6,    -3,     8,     9,     9,    -3,    -3 };
yytabelem yydef[]={

     0,    -2,     1,     3,     0,     2,     0,     5,     0,     6,
     7,     8,     0,    13,     4,     0,     0,    11,    12,     0,
     0,     9,     0,     0,    10,     0,     0,    15,    14 };
typedef struct
#ifdef __cplusplus
	yytoktype
#endif
{ char *t_name; int t_val; } yytoktype;
#ifndef YYDEBUG
#	define YYDEBUG	0	/* don't allow debugging */
#endif

#if YYDEBUG

yytoktype yytoks[] =
{
	"QID",	1,
	"OP",	2,
	"NOT",	3,
	"INF",	4,
	"NUMBER",	5,
	"KEYWORD",	6,
	"LP",	7,
	"RP",	8,
	"COMMA",	9,
	"SEMI",	10,
	"-unknown-",	-1	/* ends search */
};

char * yyreds[] =
{
	"-no such reduction-",
	"queries : query",
	"queries : queries query",
	"query : QID",
	"query : QID qexp SEMI",
	"query : error SEMI",
	"qexp : KEYWORD",
	"qexp : notexp",
	"qexp : opexp",
	"notexp : NOT LP qexp RP",
	"opexp : OP pval LP list RP",
	"pval : INF",
	"pval : NUMBER",
	"pval : /* empty */",
	"list : qexp COMMA qexp",
	"list : list COMMA qexp",
};
#endif /* YYDEBUG */
# line	1 "/usr/ccs/bin/yaccpar"
/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)yaccpar	6.12	93/06/07 SMI"

/*
** Skeleton parser driver for yacc output
*/

/*
** yacc user known macros and defines
*/
#define YYERROR		goto yyerrlab
#define YYACCEPT	return(0)
#define YYABORT		return(1)
#define YYBACKUP( newtoken, newvalue )\
{\
	if ( yychar >= 0 || ( yyr2[ yytmp ] >> 1 ) != 1 )\
	{\
		yyerror( "syntax error - cannot backup" );\
		goto yyerrlab;\
	}\
	yychar = newtoken;\
	yystate = *yyps;\
	yylval = newvalue;\
	goto yynewstate;\
}
#define YYRECOVERING()	(!!yyerrflag)
#define YYNEW(type)	malloc(sizeof(type) * yynewmax)
#define YYCOPY(to, from, type) \
	(type *) memcpy(to, (char *) from, yynewmax * sizeof(type))
#define YYENLARGE( from, type) \
	(type *) realloc((char *) from, yynewmax * sizeof(type))
#ifndef YYDEBUG
#	define YYDEBUG	1	/* make debugging available */
#endif

/*
** user known globals
*/
int yydebug;			/* set to 1 to get debugging */

/*
** driver internal defines
*/
#define YYFLAG		(-10000000)

/*
** global variables used by the parser
*/
YYSTYPE *yypv;			/* top of value stack */
int *yyps;			/* top of state stack */

int yystate;			/* current state */
int yytmp;			/* extra var (lasts between blocks) */

int yynerrs;			/* number of errors */
int yyerrflag;			/* error recovery flag */
int yychar;			/* current input token number */



#ifdef YYNMBCHARS
#define YYLEX()		yycvtok(yylex())
/*
** yycvtok - return a token if i is a wchar_t value that exceeds 255.
**	If i<255, i itself is the token.  If i>255 but the neither 
**	of the 30th or 31st bit is on, i is already a token.
*/
#if defined(__STDC__) || defined(__cplusplus)
int yycvtok(int i)
#else
int yycvtok(i) int i;
#endif
{
	int first = 0;
	int last = YYNMBCHARS - 1;
	int mid;
	wchar_t j;

	if(i&0x60000000){/*Must convert to a token. */
		if( yymbchars[last].character < i ){
			return i;/*Giving up*/
		}
		while ((last>=first)&&(first>=0)) {/*Binary search loop*/
			mid = (first+last)/2;
			j = yymbchars[mid].character;
			if( j==i ){/*Found*/ 
				return yymbchars[mid].tvalue;
			}else if( j<i ){
				first = mid + 1;
			}else{
				last = mid -1;
			}
		}
		/*No entry in the table.*/
		return i;/* Giving up.*/
	}else{/* i is already a token. */
		return i;
	}
}
#else/*!YYNMBCHARS*/
#define YYLEX()		yylex()
#endif/*!YYNMBCHARS*/

/*
** yyparse - return 0 if worked, 1 if syntax error not recovered from
*/
#if defined(__STDC__) || defined(__cplusplus)
int yyparse(void)
#else
int yyparse()
#endif
{
	register YYSTYPE *yypvt;	/* top of value stack for $vars */

#if defined(__cplusplus) || defined(lint)
/*
	hacks to please C++ and lint - goto's inside switch should never be
	executed; yypvt is set to 0 to avoid "used before set" warning.
*/
	static int __yaccpar_lint_hack__ = 0;
	switch (__yaccpar_lint_hack__)
	{
		case 1: goto yyerrlab;
		case 2: goto yynewstate;
	}
	yypvt = 0;
#endif

	/*
	** Initialize externals - yyparse may be called more than once
	*/
	yypv = &yyv[-1];
	yyps = &yys[-1];
	yystate = 0;
	yytmp = 0;
	yynerrs = 0;
	yyerrflag = 0;
	yychar = -1;

#if YYMAXDEPTH <= 0
	if (yymaxdepth <= 0)
	{
		if ((yymaxdepth = YYEXPAND(0)) <= 0)
		{
			yyerror("yacc initialization error");
			YYABORT;
		}
	}
#endif

	{
		register YYSTYPE *yy_pv;	/* top of value stack */
		register int *yy_ps;		/* top of state stack */
		register int yy_state;		/* current state */
		register int  yy_n;		/* internal state number info */
	goto yystack;	/* moved from 6 lines above to here to please C++ */

		/*
		** get globals into registers.
		** branch to here only if YYBACKUP was called.
		*/
	yynewstate:
		yy_pv = yypv;
		yy_ps = yyps;
		yy_state = yystate;
		goto yy_newstate;

		/*
		** get globals into registers.
		** either we just started, or we just finished a reduction
		*/
	yystack:
		yy_pv = yypv;
		yy_ps = yyps;
		yy_state = yystate;

		/*
		** top of for (;;) loop while no reductions done
		*/
	yy_stack:
		/*
		** put a state and value onto the stacks
		*/
#if YYDEBUG
		/*
		** if debugging, look up token value in list of value vs.
		** name pairs.  0 and negative (-1) are special values.
		** Note: linear search is used since time is not a real
		** consideration while debugging.
		*/
		if ( yydebug )
		{
			register int yy_i;

			printf( "State %d, token ", yy_state );
			if ( yychar == 0 )
				printf( "end-of-file\n" );
			else if ( yychar < 0 )
				printf( "-none-\n" );
			else
			{
				for ( yy_i = 0; yytoks[yy_i].t_val >= 0;
					yy_i++ )
				{
					if ( yytoks[yy_i].t_val == yychar )
						break;
				}
				printf( "%s\n", yytoks[yy_i].t_name );
			}
		}
#endif /* YYDEBUG */
		if ( ++yy_ps >= &yys[ yymaxdepth ] )	/* room on stack? */
		{
			/*
			** reallocate and recover.  Note that pointers
			** have to be reset, or bad things will happen
			*/
			int yyps_index = (yy_ps - yys);
			int yypv_index = (yy_pv - yyv);
			int yypvt_index = (yypvt - yyv);
			int yynewmax;
#ifdef YYEXPAND
			yynewmax = YYEXPAND(yymaxdepth);
#else
			yynewmax = 2 * yymaxdepth;	/* double table size */
			if (yymaxdepth == YYMAXDEPTH)	/* first time growth */
			{
				char *newyys = (char *)YYNEW(int);
				char *newyyv = (char *)YYNEW(YYSTYPE);
				if (newyys != 0 && newyyv != 0)
				{
					yys = YYCOPY(newyys, yys, int);
					yyv = YYCOPY(newyyv, yyv, YYSTYPE);
				}
				else
					yynewmax = 0;	/* failed */
			}
			else				/* not first time */
			{
				yys = YYENLARGE(yys, int);
				yyv = YYENLARGE(yyv, YYSTYPE);
				if (yys == 0 || yyv == 0)
					yynewmax = 0;	/* failed */
			}
#endif
			if (yynewmax <= yymaxdepth)	/* tables not expanded */
			{
				yyerror( "yacc stack overflow" );
				YYABORT;
			}
			yymaxdepth = yynewmax;

			yy_ps = yys + yyps_index;
			yy_pv = yyv + yypv_index;
			yypvt = yyv + yypvt_index;
		}
		*yy_ps = yy_state;
		*++yy_pv = yyval;

		/*
		** we have a new state - find out what to do
		*/
	yy_newstate:
		if ( ( yy_n = yypact[ yy_state ] ) <= YYFLAG )
			goto yydefault;		/* simple state */
#if YYDEBUG
		/*
		** if debugging, need to mark whether new token grabbed
		*/
		yytmp = yychar < 0;
#endif
		if ( ( yychar < 0 ) && ( ( yychar = YYLEX() ) < 0 ) )
			yychar = 0;		/* reached EOF */
#if YYDEBUG
		if ( yydebug && yytmp )
		{
			register int yy_i;

			printf( "Received token " );
			if ( yychar == 0 )
				printf( "end-of-file\n" );
			else if ( yychar < 0 )
				printf( "-none-\n" );
			else
			{
				for ( yy_i = 0; yytoks[yy_i].t_val >= 0;
					yy_i++ )
				{
					if ( yytoks[yy_i].t_val == yychar )
						break;
				}
				printf( "%s\n", yytoks[yy_i].t_name );
			}
		}
#endif /* YYDEBUG */
		if ( ( ( yy_n += yychar ) < 0 ) || ( yy_n >= YYLAST ) )
			goto yydefault;
		if ( yychk[ yy_n = yyact[ yy_n ] ] == yychar )	/*valid shift*/
		{
			yychar = -1;
			yyval = yylval;
			yy_state = yy_n;
			if ( yyerrflag > 0 )
				yyerrflag--;
			goto yy_stack;
		}

	yydefault:
		if ( ( yy_n = yydef[ yy_state ] ) == -2 )
		{
#if YYDEBUG
			yytmp = yychar < 0;
#endif
			if ( ( yychar < 0 ) && ( ( yychar = YYLEX() ) < 0 ) )
				yychar = 0;		/* reached EOF */
#if YYDEBUG
			if ( yydebug && yytmp )
			{
				register int yy_i;

				printf( "Received token " );
				if ( yychar == 0 )
					printf( "end-of-file\n" );
				else if ( yychar < 0 )
					printf( "-none-\n" );
				else
				{
					for ( yy_i = 0;
						yytoks[yy_i].t_val >= 0;
						yy_i++ )
					{
						if ( yytoks[yy_i].t_val
							== yychar )
						{
							break;
						}
					}
					printf( "%s\n", yytoks[yy_i].t_name );
				}
			}
#endif /* YYDEBUG */
			/*
			** look through exception table
			*/
			{
				register int *yyxi = yyexca;

				while ( ( *yyxi != -1 ) ||
					( yyxi[1] != yy_state ) )
				{
					yyxi += 2;
				}
				while ( ( *(yyxi += 2) >= 0 ) &&
					( *yyxi != yychar ) )
					;
				if ( ( yy_n = yyxi[1] ) < 0 )
					YYACCEPT;
			}
		}

		/*
		** check for syntax error
		*/
		if ( yy_n == 0 )	/* have an error */
		{
			/* no worry about speed here! */
			switch ( yyerrflag )
			{
			case 0:		/* new error */
				yyerror( "syntax error" );
				goto skip_init;
			yyerrlab:
				/*
				** get globals into registers.
				** we have a user generated syntax type error
				*/
				yy_pv = yypv;
				yy_ps = yyps;
				yy_state = yystate;
			skip_init:
				yynerrs++;
				/* FALLTHRU */
			case 1:
			case 2:		/* incompletely recovered error */
					/* try again... */
				yyerrflag = 3;
				/*
				** find state where "error" is a legal
				** shift action
				*/
				while ( yy_ps >= yys )
				{
					yy_n = yypact[ *yy_ps ] + YYERRCODE;
					if ( yy_n >= 0 && yy_n < YYLAST &&
						yychk[yyact[yy_n]] == YYERRCODE)					{
						/*
						** simulate shift of "error"
						*/
						yy_state = yyact[ yy_n ];
						goto yy_stack;
					}
					/*
					** current state has no shift on
					** "error", pop stack
					*/
#if YYDEBUG
#	define _POP_ "Error recovery pops state %d, uncovers state %d\n"
					if ( yydebug )
						printf( _POP_, *yy_ps,
							yy_ps[-1] );
#	undef _POP_
#endif
					yy_ps--;
					yy_pv--;
				}
				/*
				** there is no state on stack with "error" as
				** a valid shift.  give up.
				*/
				YYABORT;
			case 3:		/* no shift yet; eat a token */
#if YYDEBUG
				/*
				** if debugging, look up token in list of
				** pairs.  0 and negative shouldn't occur,
				** but since timing doesn't matter when
				** debugging, it doesn't hurt to leave the
				** tests here.
				*/
				if ( yydebug )
				{
					register int yy_i;

					printf( "Error recovery discards " );
					if ( yychar == 0 )
						printf( "token end-of-file\n" );
					else if ( yychar < 0 )
						printf( "token -none-\n" );
					else
					{
						for ( yy_i = 0;
							yytoks[yy_i].t_val >= 0;
							yy_i++ )
						{
							if ( yytoks[yy_i].t_val
								== yychar )
							{
								break;
							}
						}
						printf( "token %s\n",
							yytoks[yy_i].t_name );
					}
				}
#endif /* YYDEBUG */
				if ( yychar == 0 )	/* reached EOF. quit */
					YYABORT;
				yychar = -1;
				goto yy_newstate;
			}
		}/* end if ( yy_n == 0 ) */
		/*
		** reduction by production yy_n
		** put stack tops, etc. so things right after switch
		*/
#if YYDEBUG
		/*
		** if debugging, print the string that is the user's
		** specification of the reduction which is just about
		** to be done.
		*/
		if ( yydebug )
			printf( "Reduce by (%d) \"%s\"\n",
				yy_n, yyreds[ yy_n ] );
#endif
		yytmp = yy_n;			/* value to switch over */
		yypvt = yy_pv;			/* $vars top of value stack */
		/*
		** Look in goto table for next state
		** Sorry about using yy_state here as temporary
		** register variable, but why not, if it works...
		** If yyr2[ yy_n ] doesn't have the low order bit
		** set, then there is no action to be done for
		** this reduction.  So, no saving & unsaving of
		** registers done.  The only difference between the
		** code just after the if and the body of the if is
		** the goto yy_stack in the body.  This way the test
		** can be made before the choice of what to do is needed.
		*/
		{
			/* length of production doubled with extra bit */
			register int yy_len = yyr2[ yy_n ];

			if ( !( yy_len & 01 ) )
			{
				yy_len >>= 1;
				yyval = ( yy_pv -= yy_len )[1];	/* $$ = $1 */
				yy_state = yypgo[ yy_n = yyr1[ yy_n ] ] +
					*( yy_ps -= yy_len ) + 1;
				if ( yy_state >= YYLAST ||
					yychk[ yy_state =
					yyact[ yy_state ] ] != -yy_n )
				{
					yy_state = yyact[ yypgo[ yy_n ] ];
				}
				goto yy_stack;
			}
			yy_len >>= 1;
			yyval = ( yy_pv -= yy_len )[1];	/* $$ = $1 */
			yy_state = yypgo[ yy_n = yyr1[ yy_n ] ] +
				*( yy_ps -= yy_len ) + 1;
			if ( yy_state >= YYLAST ||
				yychk[ yy_state = yyact[ yy_state ] ] != -yy_n )
			{
				yy_state = yyact[ yypgo[ yy_n ] ];
			}
		}
					/* save until reenter driver code */
		yystate = yy_state;
		yyps = yy_ps;
		yypv = yy_pv;
	}
	/*
	** code supplied by user is placed in this switch
	*/
	switch( yytmp )
	{
		
case 3:
# line 99 "pparser.y"
{
fprintf(stderr, "QID = %d\n", yypvt[-0].ival);
                qid = yypvt[-0].ival;
                qvector.id_num.id = (long)yypvt[-0].ival;
		EXT_NONE(qvector.id_num.ext);
                ni_ptr = node_info;
                num_used_nodes = 0;
              } break;
case 4:
# line 107 "pparser.y"
{ int val;

                if (UNDEF != fix_up(yypvt[-1].ival)) {
                  val = make_vector(ni_ptr-node_info);
                  if (0 == val) {
                    fprintf(stderr,
                            "pparser: query %d empty. Ignored.\n",qid);
                  }
                  if (UNDEF == val) {
                    fprintf(stderr,"pparser: Error in query %d.\n",qid);
                  }
                }
                else {
                  fprintf(stderr,"pparser: Error in query %d.\n",qid);
                }
                yyval.ival = 0;
              } break;
case 5:
# line 124 "pparser.y"
{
                /* if there is a syntax error, try to parse next query */
                  fprintf(stderr,"syntax error around qid %d\n",qid);
                  yyval.ival = UNDEF;
              } break;
case 6:
# line 131 "pparser.y"
{ DICT_ENTRY dict_entry;

/*
fprintf(stderr, "KEYWORD = %s\n", &yytext[0]);
*/

                ALLOC_TREE(num_used_nodes);
                yyval.ival = num_used_nodes;
                tree_ptr = tree + num_used_nodes;
                tree_ptr->sibling = UNDEF;
                tree_ptr->child = UNDEF;
                
                dict_entry.con = UNDEF;
		dict_entry.token = &yytext[0];
                switch(seek_dict(dict_index,&dict_entry)) {
                case 0:
                    fprintf(stderr,"(%d: <%s,%d> not in dictionary)\n",
                      qid,yytext,yypvt[-0].ival);
                    ALLOC_NODE_INFO(ni_ptr);
                    ni_ptr->type = UNDEF;
                    ni_ptr->con = UNDEF;
                    ni_ptr->node = num_used_nodes;
                    break;
                case 1:
                    if (1 != read_dict(dict_index,&dict_entry)) {
                      print_error("pparser", "Quit");
                      exit(1);
                    }
                    ALLOC_NODE_INFO(ni_ptr);
                    ni_ptr->type = UNDEF;
                    ni_ptr->con = dict_entry.con;
                    ni_ptr->ctype = yypvt[-0].ival;		/* $1 is ctype */
                    ni_ptr->node = num_used_nodes;
                    ni_ptr->p = FUNDEF;
                    ni_ptr->wt = 1.0;
                    break;
                case UNDEF:
                    print_error("pparser", "Quit");
                    exit(1);
                default:
                    fprintf(stderr,"pparser: faulty seek_dict return.\n");
                    print_error("pparser", "Quit");
                    exit(1);
                } /* switch */

                tree_ptr->info = (long)(ni_ptr - node_info);
                num_used_nodes++;
                ni_ptr++;
              } break;
case 7:
# line 180 "pparser.y"
{yyval.ival = yypvt[-0].ival;} break;
case 8:
# line 181 "pparser.y"
{yyval.ival = yypvt[-0].ival;} break;
case 9:
# line 185 "pparser.y"
{
                ALLOC_TREE(num_used_nodes);
                yyval.ival = num_used_nodes;
                tree_ptr = tree + num_used_nodes;
                ALLOC_NODE_INFO(ni_ptr);
                ni_ptr->type = NOT_OP;
                ni_ptr->con = UNDEF;
                ni_ptr->ctype = UNDEF;
                ni_ptr->node = num_used_nodes;
                ni_ptr->p = FUNDEF;
                ni_ptr->wt = 1.0;
                tree_ptr->info = (long)(ni_ptr - node_info);
                num_used_nodes++;
                ni_ptr++;
                tree_ptr->sibling = UNDEF;
                tree_ptr->child = yypvt[-1].ival;
              } break;
case 10:
# line 204 "pparser.y"
{
                long offset;

                offset = (tree + yypvt[-1].ival)->info;
                (node_info+offset)->type = yypvt[-4].ival;
                (node_info+offset)->p = yypvt[-3].dval;
                (node_info+offset)->wt = 1.0;
                yyval.ival = yypvt[-1].ival;
              } break;
case 11:
# line 215 "pparser.y"
{ yyval.dval = 0.0; } break;
case 12:
# line 216 "pparser.y"
{ yyval.dval = atof(yytext); } break;
case 13:
# line 217 "pparser.y"
{ yyval.dval = 0.0; } break;
case 14:
# line 220 "pparser.y"
{ 
                /* first two elements in the list - must allocate parent */
                  ALLOC_TREE(num_used_nodes);
                  yyval.ival = num_used_nodes;
                  tree_ptr = tree + num_used_nodes;
                  ALLOC_NODE_INFO(ni_ptr);
                  ni_ptr->type = ANDOR;
                  ni_ptr->con = UNDEF;
                  ni_ptr->ctype = UNDEF;
                  ni_ptr->node = num_used_nodes;
                  ni_ptr->p = FUNDEF;
                  tree_ptr->info = (long)(ni_ptr-node_info);
                  num_used_nodes++;
                  ni_ptr++;
                tree_ptr->child = yypvt[-2].ival;
                tree_ptr->sibling = UNDEF;
                (tree + yypvt[-2].ival)->sibling = yypvt[-0].ival;
              } break;
case 15:
# line 238 "pparser.y"
{
                (tree + yypvt[-0].ival)->sibling = (tree + yypvt[-2].ival)->child;
                (tree + yypvt[-2].ival)->child = yypvt[-0].ival;
                yyval.ival = yypvt[-2].ival;
              } break;
# line	532 "/usr/ccs/bin/yaccpar"
	}
	goto yystack;		/* reset registers in driver code */
}

