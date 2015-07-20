D      [0-9]
E      " "*=" "*
I      [^\t\n;]
IE     [\t\n;]
L      [_A-Za-z]
P      [^ \t\n;]
PE     [ \t\n;]
S      #" "*
W      [ \t\n]
NDEL   [^'\n]

%{
#ifdef RCSID
static char rcsid[] = "$Header: ppre_scan.l,v 8.0 85/06/03 17:38:08 smart Exp $";
#endif

/* Copyright (c) 1984 - Chris Buckley, Ellen Voorhees, Cornell University */

#include "functions.h"

#define	MAXNOCT		50		/* max number of ctype specified */
#define	MAXCTSPECLEN	20		/* max specified length */
#define	MAXLINE		81

int ct_stem[MAXNOCT] = {1};		/* stemming options for ctypes; 
				           array index is ctype number;
  				           options: 0=none,1=full,2=plurals;
				           default (ct_stem[0]=1)         */
int cur_ctype;
char term[MAXLINE];
int def_ctype = 0;			/* default concept type */
int no_ctype_spec = 0;			/* # of ctypes specified in input */
int ct_val[MAXNOCT];			/* int corresponding to specifier */
char cur_spec[MAXCTSPECLEN];		/* specifier of current concept */
char ctype_spec[MAXNOCT][MAXCTSPECLEN];	/* ctype specs as given by user */
extern long global_end;

extern char *lookup();
%}

%%
{S}and              ECHO;
{S}endcoll;         ;
{S}inf              ECHO;
{S}not              ECHO;
{S}or               ECHO;
{S}q{D}+{E}         { char *yy_ptr1, *yy_ptr2;

		      ECHO;

                      for (yy_ptr1=&yytext[0]; *yy_ptr1 != 'q'; yy_ptr1++) ;
                      for (yy_ptr2=yy_ptr1+1; *yy_ptr2 != '='; yy_ptr2++) ;
                      *yy_ptr2 = '\0';
                      if (atoi(yy_ptr1+1) > global_end)
			return(1);
		    }
{S}stem{E}{P};/{PE} { int i;

                      for (i = 0; yytext[i] != '='; i++) ;
		      if (yytext[i+1]=='n') ct_stem[def_ctype] = 0;
		      if (yytext[i+1]=='s') ct_stem[def_ctype] = 2;
                    }

{S}default_ct{E}{D}*;/{PE} { int i;

                      for (i = 0; yytext[i] != '='; i++) ;
                      for (i++; i < yyleng && yytext[i]==' '; i++) ;
		      def_ctype = atoi(yytext+i);
		      ct_stem[def_ctype] = 1; /* default stemming option */
            	    }

{S}ct_spec","{L}*{E}{D}*","{D}*;/{PE} { int i,j;

                      for (i=0; yytext[i] != ','; i++)
                        ;
                      for (i++,j=0; (i+j)<yyleng && yytext[i+j] != '='; j++)
                        ctype_spec[no_ctype_spec][j] = yytext[i+j];
		      ctype_spec[no_ctype_spec][j] = '\0';
		      ct_val[no_ctype_spec] = atoi(yytext+i+j+1);
		      ct_stem[ct_val[no_ctype_spec]] = atoi(yytext+i+j+3);
                      no_ctype_spec++;
                    }

{D}+"."?{D}*        ECHO;
"."{D}+             ECHO;

!{D}+!              { yytext[yyleng-1] = '\0';
                      fprintf(yyout,"<%s#%d>",yytext,def_ctype);
		    }

'{NDEL}+'           {
		      yytext[yyleng-1] = '\0';
                      strcpy(term,lookup(yytext+1,def_ctype));
                      fprintf(yyout,"<%s#%d>",term,def_ctype);
		    }   

"("                 ECHO;
")"                 ECHO;
","                 ECHO;
";"                 ECHO;
{W}+                ECHO;
.                   {fprintf(stderr,"UNRECOGNIZED CHARACTER '%s'\n",yytext);}

