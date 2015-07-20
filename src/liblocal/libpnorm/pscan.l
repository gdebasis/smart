
D       [0-9]
E       " "*=" "*
S       #" "*
W       [ \t]
NEWLINE [\n]
%{
#ifdef RCSID
static char rcsid[] = "$Header: pscan.l,v 8.0 85/06/03 17:38:12 smart Exp $";
#endif

/* Copyright (c) 1984 - Chris Buckley, Ellen Voorhees, Cornell University */

#include "functions.h"
#include "pnorm_common.h"
#include "trace.h"

#define QID     1
#define OP      2
#define NOT     3
#define INF     4
#define NUMBER  5
#define KEYWORD 6
#define LP      7
#define RP      8
#define COMMA   9
#define SEMI    10
#define ERROR   11      /* to parser, this is an illegal token - error
                         * recovery wil be started                    */
typedef union {
  short ival;
  double dval;
} YYSTYPE;

extern YYSTYPE yylval;
extern char yytext[];
%}

%%
{S}q{D}+{E}         { 
                      char *yy_ptr1, *yy_ptr2;

                      for (yy_ptr1= &yytext[0]; *yy_ptr1 != 'q'; yy_ptr1++)
                        ;
                      for (yy_ptr2=yy_ptr1+1; *yy_ptr2 != '='; yy_ptr2++)
                        ;
                      *yy_ptr2 = '\0';
                      yylval.ival = atoi(yy_ptr1+1);
                      return(QID);
		    }          

{S}and              {
		      yylval.ival = AND_OP;
		      return(OP);
                    }          

{S}or               {
		      yylval.ival = OR_OP;
		      return(OP);
		    }

{S}not              {
		      yylval.ival = NOT_OP;
		      return(NOT);
		    }

{S}inf              { yylval.dval = 0.0;
                      return(INF);
                    }

{D}+("."{D}*)? |
"."{D}+             return(NUMBER);

"<"[^#]+"#"{W}*{D}+{W}*">" { 
		      char *yy_ptr;

                      yy_ptr = &yytext[0] + yyleng - 1;
                      *yy_ptr = '\0';
                      for (yy_ptr--; *yy_ptr!='#'; yy_ptr--)
                        ;
                      yylval.ival = atoi(yy_ptr+1);    /* yylval = ctype */
                      *yy_ptr = '\0';
                      strcpy(yytext,yytext+1);    /* yytext = token */
                      return(KEYWORD);
		    }

"("                 return(LP);

")"                 return(RP);

","                 return(COMMA);

";"                 return(SEMI);

{W}+                ;
{NEWLINE}           ;

.                   {
                      fprintf(stderr,"pscan: unrecognized character <%s>\n",
                        yytext);
                      return(ERROR);
                    }
%%
yywrap()
{
    return(1);
}
