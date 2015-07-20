#include <stdio.h>
# define U(x) x
# define NLSTATE yyprevious=YYNEWLINE
# define BEGIN yybgin = yysvec + 1 +
# define INITIAL 0
# define YYLERR yysvec
# define YYSTATE (yyestate-yysvec-1)
# define YYOPTIM 1
# define YYLMAX BUFSIZ
#ifndef __cplusplus
# define output(c) (void)putc(c,yyout)
#else
# define lex_output(c) (void)putc(c,yyout)
#endif

#if defined(__cplusplus) || defined(__STDC__)

#if defined(__cplusplus) && defined(__EXTERN_C__)
extern "C" {
#endif
	int yyback(int *, int);
	int yyinput(void);
	int yylook(void);
	void yyoutput(int);
	int yyracc(int);
	int yyreject(void);
	void yyunput(int);
	int yylex(void);
#ifdef YYLEX_E
	void yywoutput(wchar_t);
	wchar_t yywinput(void);
#endif
#ifndef yyless
	int yyless(int);
#endif
#ifndef yywrap
	int yywrap(void);
#endif
#ifdef LEXDEBUG
	void allprint(char);
	void sprint(char *);
#endif
#if defined(__cplusplus) && defined(__EXTERN_C__)
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
	void exit(int);
#ifdef __cplusplus
}
#endif

#endif
# define unput(c) {yytchar= (c);if(yytchar=='\n')yylineno--;*yysptr++=yytchar;}
# define yymore() (yymorfg=1)
#ifndef __cplusplus
# define input() (((yytchar=yysptr>yysbuf?U(*--yysptr):getc(yyin))==10?(yylineno++,yytchar):yytchar)==EOF?0:yytchar)
#else
# define lex_input() (((yytchar=yysptr>yysbuf?U(*--yysptr):getc(yyin))==10?(yylineno++,yytchar):yytchar)==EOF?0:yytchar)
#endif
#define ECHO fprintf(yyout, "%s",yytext)
# define REJECT { nstr = yyreject(); goto yyfussy;}
int yyleng;
char yytext[YYLMAX];
int yymorfg;
extern char *yysptr, yysbuf[];
int yytchar;
FILE *yyin = {stdin}, *yyout = {stdout};
extern int yylineno;
struct yysvf { 
	struct yywork *yystoff;
	struct yysvf *yyother;
	int *yystops;};
struct yysvf *yyestate;
extern struct yysvf yysvec[], *yybgin;
#ifdef RCSID
static char rcsid[] = "$Header: pscan.l,v 8.0 85/06/03 17:38:12 smart Exp $";
#endif


# line 13 "pscan.l"
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
# define YYNEWLINE 10
yylex(){
int nstr; extern int yyprevious;
#ifdef __cplusplus
/* to avoid CC and lint complaining yyfussy not being used ...*/
static int __lex_hack = 0;
if (__lex_hack) goto yyfussy;
#endif
while((nstr = yylook()) >= 0)
yyfussy: switch(nstr){
case 0:
if(yywrap()) return(0); break;
case 1:

# line 40 "pscan.l"
        { 
                      char *yy_ptr1, *yy_ptr2;

                      for (yy_ptr1= &yytext[0]; *yy_ptr1 != 'q'; yy_ptr1++)
                        ;
                      for (yy_ptr2=yy_ptr1+1; *yy_ptr2 != '='; yy_ptr2++)
                        ;
                      *yy_ptr2 = '\0';
                      yylval.ival = atoi(yy_ptr1+1);
                      return(QID);
		    }
break;
case 2:

# line 52 "pscan.l"
             {
		      yylval.ival = AND_OP;
		      return(OP);
                    }
break;
case 3:

# line 57 "pscan.l"
              {
		      yylval.ival = OR_OP;
		      return(OP);
		    }
break;
case 4:

# line 62 "pscan.l"
             {
		      yylval.ival = NOT_OP;
		      return(NOT);
		    }
break;
case 5:

# line 67 "pscan.l"
             { yylval.dval = 0.0;
                      return(INF);
                    }
break;
case 6:

# line 71 "pscan.l"
case 7:

# line 72 "pscan.l"
            return(NUMBER);
break;
case 8:

# line 74 "pscan.l"
{ 
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
break;
case 9:

# line 87 "pscan.l"
                return(LP);
break;
case 10:

# line 89 "pscan.l"
                return(RP);
break;
case 11:

# line 91 "pscan.l"
                return(COMMA);
break;
case 12:

# line 93 "pscan.l"
                return(SEMI);
break;
case 13:

# line 95 "pscan.l"
               ;
break;
case 14:

# line 96 "pscan.l"
          ;
break;
case 15:

# line 98 "pscan.l"
                  {
                      fprintf(stderr,"pscan: unrecognized character <%s>\n",
                        yytext);
                      return(ERROR);
                    }
break;
case -1:
break;
default:
(void)fprintf(yyout,"bad switch yylook %d",nstr);
} return(0); }
/* end of yylex */
yywrap()
{
    return(1);
}
int yyvstop[] = {
0,

15,
0,

13,
15,
0,

14,
0,

15,
0,

9,
15,
0,

10,
15,
0,

11,
15,
0,

15,
0,

6,
15,
0,

12,
15,
0,

15,
0,

13,
0,

7,
0,

6,
0,

6,
0,

3,
0,

2,
0,

5,
0,

4,
0,

1,
0,

8,
0,
0};
# define YYTYPE unsigned char
struct yywork { YYTYPE verify, advance; } yycrank[] = {
0,0,	0,0,	1,3,	0,0,	
0,0,	13,24,	0,0,	0,0,	
0,0,	0,0,	1,4,	1,5,	
4,14,	13,24,	13,24,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	6,15,	29,34,	4,14,	
1,6,	24,30,	34,34,	13,0,	
35,35,	1,7,	1,8,	36,37,	
0,0,	1,9,	0,0,	1,10,	
0,0,	1,11,	2,7,	2,8,	
13,24,	0,0,	2,9,	0,0,	
2,10,	0,0,	0,0,	0,0,	
1,12,	1,13,	0,0,	29,35,	
0,0,	0,0,	36,37,	34,35,	
0,0,	2,12,	2,13,	10,21,	
10,21,	10,21,	10,21,	10,21,	
10,21,	10,21,	10,21,	10,21,	
10,21,	11,22,	0,0,	11,23,	
11,23,	11,23,	11,23,	11,23,	
11,23,	11,23,	11,23,	11,23,	
11,23,	0,0,	0,0,	0,0,	
36,38,	0,0,	6,16,	0,0,	
0,0,	25,31,	0,0,	26,32,	
0,0,	0,0,	6,17,	0,0,	
0,0,	0,0,	0,0,	6,18,	
6,19,	16,25,	6,20,	17,26,	
18,27,	19,28,	20,29,	20,29,	
20,29,	20,29,	20,29,	20,29,	
20,29,	20,29,	20,29,	20,29,	
22,22,	22,22,	22,22,	22,22,	
22,22,	22,22,	22,22,	22,22,	
22,22,	22,22,	27,33,	30,30,	
37,37,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	30,30,	37,37,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	30,36,	30,36,	
30,36,	30,36,	30,36,	30,36,	
30,36,	30,36,	30,36,	30,36,	
0,0,	0,0,	0,0,	0,0,	
0,0,	37,38,	0,0,	0,0,	
0,0};
struct yysvf yysvec[] = {
0,	0,	0,
yycrank+-1,	0,		0,	
yycrank+-10,	yysvec+1,	0,	
yycrank+0,	0,		yyvstop+1,
yycrank+3,	0,		yyvstop+3,
yycrank+0,	0,		yyvstop+6,
yycrank+1,	0,		yyvstop+8,
yycrank+0,	0,		yyvstop+10,
yycrank+0,	0,		yyvstop+13,
yycrank+0,	0,		yyvstop+16,
yycrank+23,	0,		yyvstop+19,
yycrank+35,	0,		yyvstop+21,
yycrank+0,	0,		yyvstop+24,
yycrank+-4,	0,		yyvstop+27,
yycrank+0,	yysvec+4,	yyvstop+29,
yycrank+0,	yysvec+6,	0,	
yycrank+3,	0,		0,	
yycrank+5,	0,		0,	
yycrank+5,	0,		0,	
yycrank+3,	0,		0,	
yycrank+70,	0,		0,	
yycrank+0,	yysvec+10,	yyvstop+31,
yycrank+80,	0,		yyvstop+33,
yycrank+0,	yysvec+11,	yyvstop+35,
yycrank+-2,	yysvec+13,	0,	
yycrank+1,	0,		0,	
yycrank+1,	0,		0,	
yycrank+22,	0,		0,	
yycrank+0,	0,		yyvstop+37,
yycrank+2,	yysvec+20,	0,	
yycrank+130,	0,		0,	
yycrank+0,	0,		yyvstop+39,
yycrank+0,	0,		yyvstop+41,
yycrank+0,	0,		yyvstop+43,
yycrank+6,	0,		0,	
yycrank+8,	0,		yyvstop+45,
yycrank+34,	yysvec+30,	0,	
yycrank+131,	0,		0,	
yycrank+0,	0,		yyvstop+47,
0,	0,	0};
struct yywork *yytop = yycrank+193;
struct yysvf *yybgin = yysvec+1;
char yymatch[] = {
  0,   1,   1,   1,   1,   1,   1,   1, 
  1,   9,  10,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  9,   1,   1,  35,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
 48,  48,  48,  48,  48,  48,  48,  48, 
 48,  48,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
0};
char yyextra[] = {
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0};
/*	Copyright (c) 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ncform	6.8	95/02/11 SMI"

int yylineno =1;
# define YYU(x) x
# define NLSTATE yyprevious=YYNEWLINE
struct yysvf *yylstate [YYLMAX], **yylsp, **yyolsp;
char yysbuf[YYLMAX];
char *yysptr = yysbuf;
int *yyfnd;
extern struct yysvf *yyestate;
int yyprevious = YYNEWLINE;
#if defined(__cplusplus) || defined(__STDC__)
int yylook(void)
#else
yylook()
#endif
{
	register struct yysvf *yystate, **lsp;
	register struct yywork *yyt;
	struct yysvf *yyz;
	int yych, yyfirst;
	struct yywork *yyr;
# ifdef LEXDEBUG
	int debug;
# endif
	char *yylastch;
	/* start off machines */
# ifdef LEXDEBUG
	debug = 0;
# endif
	yyfirst=1;
	if (!yymorfg)
		yylastch = yytext;
	else {
		yymorfg=0;
		yylastch = yytext+yyleng;
		}
	for(;;){
		lsp = yylstate;
		yyestate = yystate = yybgin;
		if (yyprevious==YYNEWLINE) yystate++;
		for (;;){
# ifdef LEXDEBUG
			if(debug)fprintf(yyout,"state %d\n",yystate-yysvec-1);
# endif
			yyt = yystate->yystoff;
			if(yyt == yycrank && !yyfirst){  /* may not be any transitions */
				yyz = yystate->yyother;
				if(yyz == 0)break;
				if(yyz->yystoff == yycrank)break;
				}
#ifndef __cplusplus
			*yylastch++ = yych = input();
#else
			*yylastch++ = yych = lex_input();
#endif
			if(yylastch > &yytext[YYLMAX]) {
				fprintf(yyout,"Input string too long, limit %d\n",YYLMAX);
				exit(1);
			}
			yyfirst=0;
		tryagain:
# ifdef LEXDEBUG
			if(debug){
				fprintf(yyout,"char ");
				allprint(yych);
				putchar('\n');
				}
# endif
			yyr = yyt;
			if ( (int)yyt > (int)yycrank){
				yyt = yyr + yych;
				if (yyt <= yytop && yyt->verify+yysvec == yystate){
					if(yyt->advance+yysvec == YYLERR)	/* error transitions */
						{unput(*--yylastch);break;}
					*lsp++ = yystate = yyt->advance+yysvec;
					if(lsp > &yylstate[YYLMAX]) {
						fprintf(yyout,"Input string too long, limit %d\n",YYLMAX);
						exit(1);
					}
					goto contin;
					}
				}
# ifdef YYOPTIM
			else if((int)yyt < (int)yycrank) {		/* r < yycrank */
				yyt = yyr = yycrank+(yycrank-yyt);
# ifdef LEXDEBUG
				if(debug)fprintf(yyout,"compressed state\n");
# endif
				yyt = yyt + yych;
				if(yyt <= yytop && yyt->verify+yysvec == yystate){
					if(yyt->advance+yysvec == YYLERR)	/* error transitions */
						{unput(*--yylastch);break;}
					*lsp++ = yystate = yyt->advance+yysvec;
					if(lsp > &yylstate[YYLMAX]) {
						fprintf(yyout,"Input string too long, limit %d\n",YYLMAX);
						exit(1);
					}
					goto contin;
					}
				yyt = yyr + YYU(yymatch[yych]);
# ifdef LEXDEBUG
				if(debug){
					fprintf(yyout,"try fall back character ");
					allprint(YYU(yymatch[yych]));
					putchar('\n');
					}
# endif
				if(yyt <= yytop && yyt->verify+yysvec == yystate){
					if(yyt->advance+yysvec == YYLERR)	/* error transition */
						{unput(*--yylastch);break;}
					*lsp++ = yystate = yyt->advance+yysvec;
					if(lsp > &yylstate[YYLMAX]) {
						fprintf(yyout,"Input string too long, limit %d\n",YYLMAX);
						exit(1);
					}
					goto contin;
					}
				}
			if ((yystate = yystate->yyother) && (yyt= yystate->yystoff) != yycrank){
# ifdef LEXDEBUG
				if(debug)fprintf(yyout,"fall back to state %d\n",yystate-yysvec-1);
# endif
				goto tryagain;
				}
# endif
			else
				{unput(*--yylastch);break;}
		contin:
# ifdef LEXDEBUG
			if(debug){
				fprintf(yyout,"state %d char ",yystate-yysvec-1);
				allprint(yych);
				putchar('\n');
				}
# endif
			;
			}
# ifdef LEXDEBUG
		if(debug){
			fprintf(yyout,"stopped at %d with ",*(lsp-1)-yysvec-1);
			allprint(yych);
			putchar('\n');
			}
# endif
		while (lsp-- > yylstate){
			*yylastch-- = 0;
			if (*lsp != 0 && (yyfnd= (*lsp)->yystops) && *yyfnd > 0){
				yyolsp = lsp;
				if(yyextra[*yyfnd]){		/* must backup */
					while(yyback((*lsp)->yystops,-*yyfnd) != 1 && lsp > yylstate){
						lsp--;
						unput(*yylastch--);
						}
					}
				yyprevious = YYU(*yylastch);
				yylsp = lsp;
				yyleng = yylastch-yytext+1;
				yytext[yyleng] = 0;
# ifdef LEXDEBUG
				if(debug){
					fprintf(yyout,"\nmatch ");
					sprint(yytext);
					fprintf(yyout," action %d\n",*yyfnd);
					}
# endif
				return(*yyfnd++);
				}
			unput(*yylastch);
			}
		if (yytext[0] == 0  /* && feof(yyin) */)
			{
			yysptr=yysbuf;
			return(0);
			}
#ifndef __cplusplus
		yyprevious = yytext[0] = input();
		if (yyprevious>0)
			output(yyprevious);
#else
		yyprevious = yytext[0] = lex_input();
		if (yyprevious>0)
			lex_output(yyprevious);
#endif
		yylastch=yytext;
# ifdef LEXDEBUG
		if(debug)putchar('\n');
# endif
		}
	}
#if defined(__cplusplus) || defined(__STDC__)
int yyback(int *p, int m)
#else
yyback(p, m)
	int *p;
#endif
{
	if (p==0) return(0);
	while (*p) {
		if (*p++ == m)
			return(1);
	}
	return(0);
}
	/* the following are only used in the lex library */
#if defined(__cplusplus) || defined(__STDC__)
int yyinput(void)
#else
yyinput()
#endif
{
#ifndef __cplusplus
	return(input());
#else
	return(lex_input());
#endif
	}
#if defined(__cplusplus) || defined(__STDC__)
void yyoutput(int c)
#else
yyoutput(c)
  int c; 
#endif
{
#ifndef __cplusplus
	output(c);
#else
	lex_output(c);
#endif
	}
#if defined(__cplusplus) || defined(__STDC__)
void yyunput(int c)
#else
yyunput(c)
   int c; 
#endif
{
	unput(c);
	}
