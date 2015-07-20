#include <stdio.h>
# define U(x) x
# define NLSTATE ppprevious=YYNEWLINE
# define BEGIN ppbgin = ppsvec + 1 +
# define INITIAL 0
# define YYLERR ppsvec
# define YYSTATE (ppestate-ppsvec-1)
# define YYOPTIM 1
# define YYLMAX BUFSIZ
#ifndef __cplusplus
# define output(c) (void)putc(c,ppout)
#else
# define lex_output(c) (void)putc(c,ppout)
#endif

#if defined(__cplusplus) || defined(__STDC__)

#if defined(__cplusplus) && defined(__EXTERN_C__)
extern "C" {
#endif
	int ppback(int *, int);
	int ppinput(void);
	int pplook(void);
	void ppoutput(int);
	int yyracc(int);
	int yyreject(void);
	void ppunput(int);
	int pplex(void);
#ifdef YYLEX_E
	void yywoutput(wchar_t);
	wchar_t yywinput(void);
#endif
#ifndef yyless
	int yyless(int);
#endif
#ifndef ppwrap
	int ppwrap(void);
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
# define unput(c) {pptchar= (c);if(pptchar=='\n')pplineno--;*ppsptr++=pptchar;}
# define yymore() (ppmorfg=1)
#ifndef __cplusplus
# define input() (((pptchar=ppsptr>ppsbuf?U(*--ppsptr):getc(ppin))==10?(pplineno++,pptchar):pptchar)==EOF?0:pptchar)
#else
# define lex_input() (((pptchar=ppsptr>ppsbuf?U(*--ppsptr):getc(ppin))==10?(pplineno++,pptchar):pptchar)==EOF?0:pptchar)
#endif
#define ECHO fprintf(ppout, "%s",pptext)
# define REJECT { nstr = yyreject(); goto yyfussy;}
int ppleng;
char pptext[YYLMAX];
int ppmorfg;
extern char *ppsptr, ppsbuf[];
int pptchar;
FILE *ppin = {stdin}, *ppout = {stdout};
extern int pplineno;
struct yysvf { 
	struct yywork *yystoff;
	struct yysvf *yyother;
	int *yystops;};
struct yysvf *ppestate;
extern struct yysvf ppsvec[], *ppbgin;
#ifdef RCSID
static char rcsid[] = "$Header: ppre_scan.l,v 8.0 85/06/03 17:38:08 smart Exp $";
#endif


# line 18 "ppre_scan.l"
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
# define YYNEWLINE 10
pplex(){
int nstr; extern int ppprevious;
#ifdef __cplusplus
/* to avoid CC and lint complaining yyfussy not being used ...*/
static int __lex_hack = 0;
if (__lex_hack) goto yyfussy;
#endif
while((nstr = pplook()) >= 0)
yyfussy: switch(nstr){
case 0:
if(ppwrap()) return(0); break;
case 1:

# line 42 "ppre_scan.l"
             ECHO;
break;
case 2:

# line 43 "ppre_scan.l"
        ;
break;
case 3:

# line 44 "ppre_scan.l"
             ECHO;
break;
case 4:

# line 45 "ppre_scan.l"
             ECHO;
break;
case 5:

# line 46 "ppre_scan.l"
              ECHO;
break;
case 6:

# line 47 "ppre_scan.l"
        { char *yy_ptr1, *yy_ptr2;

		      ECHO;

                      for (yy_ptr1=&pptext[0]; *yy_ptr1 != 'q'; yy_ptr1++) ;
                      for (yy_ptr2=yy_ptr1+1; *yy_ptr2 != '='; yy_ptr2++) ;
                      *yy_ptr2 = '\0';
                      if (atoi(yy_ptr1+1) > global_end)
			return(1);
		    }
break;
case 7:

# line 57 "ppre_scan.l"
{ int i;

                      for (i = 0; pptext[i] != '='; i++) ;
		      if (pptext[i+1]=='n') ct_stem[def_ctype] = 0;
		      if (pptext[i+1]=='s') ct_stem[def_ctype] = 2;
                    }
break;
case 8:

# line 64 "ppre_scan.l"
{ int i;

                      for (i = 0; pptext[i] != '='; i++) ;
                      for (i++; i < ppleng && pptext[i]==' '; i++) ;
		      def_ctype = atoi(pptext+i);
		      ct_stem[def_ctype] = 1; /* default stemming option */
            	    }
break;
case 9:

# line 72 "ppre_scan.l"
{ int i,j;

                      for (i=0; pptext[i] != ','; i++)
                        ;
                      for (i++,j=0; (i+j)<ppleng && pptext[i+j] != '='; j++)
                        ctype_spec[no_ctype_spec][j] = pptext[i+j];
		      ctype_spec[no_ctype_spec][j] = '\0';
		      ct_val[no_ctype_spec] = atoi(pptext+i+j+1);
		      ct_stem[ct_val[no_ctype_spec]] = atoi(pptext+i+j+3);
                      no_ctype_spec++;
                    }
break;
case 10:

# line 84 "ppre_scan.l"
       ECHO;
break;
case 11:

# line 85 "ppre_scan.l"
            ECHO;
break;
case 12:

# line 87 "ppre_scan.l"
             { pptext[ppleng-1] = '\0';
                      fprintf(ppout,"<%s#%d>",pptext,def_ctype);
		    }
break;
case 13:

# line 91 "ppre_scan.l"
          {
		      pptext[ppleng-1] = '\0';
                      strcpy(term,lookup(pptext+1,def_ctype));
                      fprintf(ppout,"<%s#%d>",term,def_ctype);
		    }
break;
case 14:

# line 97 "ppre_scan.l"
                ECHO;
break;
case 15:

# line 98 "ppre_scan.l"
                ECHO;
break;
case 16:

# line 99 "ppre_scan.l"
                ECHO;
break;
case 17:

# line 100 "ppre_scan.l"
                ECHO;
break;
case 18:

# line 101 "ppre_scan.l"
               ECHO;
break;
case 19:

# line 102 "ppre_scan.l"
                  {fprintf(stderr,"UNRECOGNIZED CHARACTER '%s'\n",pptext);}
break;
case -1:
break;
default:
(void)fprintf(ppout,"bad switch pplook %d",nstr);
} return(0); }
/* end of pplex */
int ppvstop[] = {
0,

19,
0,

18,
19,
0,

18,
0,

19,
0,

19,
0,

19,
0,

14,
19,
0,

15,
19,
0,

16,
19,
0,

19,
0,

10,
19,
0,

17,
19,
0,

11,
0,

10,
0,

10,
0,

12,
0,

5,
0,

13,
0,

1,
0,

3,
0,

4,
0,

6,
0,

-7,
0,

2,
0,

7,
0,

-9,
0,

9,
0,

-8,
0,

8,
0,
0};
# define YYTYPE unsigned char
struct yywork { YYTYPE verify, advance; } ppcrank[] = {
0,0,	0,0,	1,3,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	1,4,	1,5,	
4,5,	4,5,	26,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	7,16,	1,6,	4,5,	
1,7,	15,30,	48,48,	0,0,	
1,8,	1,9,	1,10,	26,40,	
2,6,	1,11,	2,7,	1,12,	
62,66,	1,13,	38,47,	2,9,	
2,10,	47,47,	53,53,	2,11,	
0,0,	2,12,	0,0,	0,0,	
1,14,	61,65,	64,68,	0,0,	
0,0,	0,0,	1,3,	6,15,	
6,15,	6,15,	6,15,	6,15,	
6,15,	6,15,	6,15,	6,15,	
6,15,	8,26,	0,0,	38,48,	
0,0,	0,0,	47,48,	53,57,	
0,0,	8,26,	8,0,	12,27,	
12,27,	12,27,	12,27,	12,27,	
12,27,	12,27,	12,27,	12,27,	
12,27,	32,42,	7,17,	43,51,	
7,18,	7,19,	7,20,	19,33,	
31,41,	33,43,	7,21,	34,44,	
35,45,	39,49,	44,52,	7,22,	
7,23,	17,31,	7,24,	8,0,	
7,25,	18,32,	20,34,	21,35,	
22,36,	23,37,	25,39,	13,28,	
8,26,	13,29,	13,29,	13,29,	
13,29,	13,29,	13,29,	13,29,	
13,29,	13,29,	13,29,	8,26,	
36,46,	42,50,	49,53,	50,54,	
51,55,	8,26,	24,38,	24,38,	
24,38,	24,38,	24,38,	24,38,	
24,38,	24,38,	24,38,	24,38,	
28,28,	28,28,	28,28,	28,28,	
28,28,	28,28,	28,28,	28,28,	
28,28,	28,28,	52,56,	54,58,	
55,59,	56,60,	57,61,	58,62,	
59,63,	60,64,	63,67,	65,69,	
65,69,	67,72,	57,0,	57,0,	
70,70,	72,75,	73,73,	73,73,	
73,73,	73,73,	73,73,	73,73,	
73,73,	73,73,	73,73,	73,73,	
75,75,	73,76,	0,0,	0,0,	
0,0,	0,0,	65,69,	0,0,	
0,0,	57,57,	66,70,	0,0,	
0,0,	0,0,	0,0,	0,0,	
57,61,	70,71,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	57,61,	0,0,	0,0,	
0,0,	75,77,	0,0,	0,0,	
0,0,	65,69,	0,0,	0,0,	
57,0,	0,0,	0,0,	66,71,	
0,0,	0,0,	57,61,	66,66,	
66,66,	66,66,	66,66,	66,66,	
66,66,	66,66,	66,66,	66,66,	
66,66,	66,66,	66,66,	66,66,	
66,66,	66,66,	66,66,	66,66,	
66,66,	66,66,	66,66,	66,66,	
66,66,	66,66,	66,66,	66,66,	
66,66,	0,0,	0,0,	0,0,	
0,0,	66,66,	0,0,	66,66,	
66,66,	66,66,	66,66,	66,66,	
66,66,	66,66,	66,66,	66,66,	
66,66,	66,66,	66,66,	66,66,	
66,66,	66,66,	66,66,	66,66,	
66,66,	66,66,	66,66,	66,66,	
66,66,	66,66,	66,66,	66,66,	
66,66,	71,71,	0,0,	0,0,	
0,0,	76,78,	76,78,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	71,73,	0,0,	0,0,	
0,0,	71,74,	71,74,	71,74,	
71,74,	71,74,	71,74,	71,74,	
71,74,	71,74,	71,74,	74,73,	
76,78,	77,77,	0,0,	74,74,	
74,74,	74,74,	74,74,	74,74,	
74,74,	74,74,	74,74,	74,74,	
74,74,	0,0,	0,0,	0,0,	
0,0,	77,79,	77,79,	77,79,	
77,79,	77,79,	77,79,	77,79,	
77,79,	77,79,	77,79,	76,78,	
77,80,	79,79,	79,79,	79,79,	
79,79,	79,79,	79,79,	79,79,	
79,79,	79,79,	79,79,	0,0,	
79,80,	80,81,	80,81,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
80,81,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	0,0,	
0,0,	0,0,	0,0,	80,81,	
0,0};
struct yysvf ppsvec[] = {
0,	0,	0,
ppcrank+-1,	0,		0,	
ppcrank+-11,	ppsvec+1,	0,	
ppcrank+0,	0,		ppvstop+1,
ppcrank+3,	0,		ppvstop+3,
ppcrank+0,	ppsvec+4,	ppvstop+6,
ppcrank+19,	0,		ppvstop+8,
ppcrank+1,	0,		ppvstop+10,
ppcrank+-76,	0,		ppvstop+12,
ppcrank+0,	0,		ppvstop+14,
ppcrank+0,	0,		ppvstop+17,
ppcrank+0,	0,		ppvstop+20,
ppcrank+39,	0,		ppvstop+23,
ppcrank+77,	0,		ppvstop+25,
ppcrank+0,	0,		ppvstop+28,
ppcrank+4,	ppsvec+6,	0,	
ppcrank+0,	ppsvec+7,	0,	
ppcrank+3,	0,		0,	
ppcrank+1,	0,		0,	
ppcrank+2,	0,		0,	
ppcrank+8,	0,		0,	
ppcrank+9,	0,		0,	
ppcrank+9,	0,		0,	
ppcrank+7,	0,		0,	
ppcrank+94,	0,		0,	
ppcrank+6,	0,		0,	
ppcrank+-4,	ppsvec+8,	0,	
ppcrank+0,	ppsvec+12,	ppvstop+31,
ppcrank+104,	0,		ppvstop+33,
ppcrank+0,	ppsvec+13,	ppvstop+35,
ppcrank+0,	0,		ppvstop+37,
ppcrank+4,	0,		0,	
ppcrank+2,	0,		0,	
ppcrank+3,	0,		0,	
ppcrank+7,	0,		0,	
ppcrank+6,	0,		0,	
ppcrank+20,	0,		0,	
ppcrank+0,	0,		ppvstop+39,
ppcrank+18,	ppsvec+24,	0,	
ppcrank+8,	0,		0,	
ppcrank+0,	0,		ppvstop+41,
ppcrank+0,	0,		ppvstop+43,
ppcrank+22,	0,		0,	
ppcrank+2,	0,		0,	
ppcrank+11,	0,		0,	
ppcrank+0,	0,		ppvstop+45,
ppcrank+0,	0,		ppvstop+47,
ppcrank+21,	0,		0,	
ppcrank+6,	0,		ppvstop+49,
ppcrank+29,	0,		0,	
ppcrank+27,	0,		0,	
ppcrank+23,	0,		0,	
ppcrank+51,	0,		0,	
ppcrank+22,	0,		0,	
ppcrank+62,	0,		0,	
ppcrank+56,	0,		0,	
ppcrank+57,	0,		0,	
ppcrank+-165,	0,		0,	
ppcrank+68,	0,		0,	
ppcrank+52,	0,		0,	
ppcrank+61,	0,		0,	
ppcrank+2,	0,		0,	
ppcrank+4,	0,		0,	
ppcrank+75,	0,		0,	
ppcrank+3,	0,		0,	
ppcrank+162,	0,		ppvstop+51,
ppcrank+166,	0,		0,	
ppcrank+74,	0,		0,	
ppcrank+0,	0,		ppvstop+53,
ppcrank+0,	0,		ppvstop+55,
ppcrank+144,	0,		0,	
ppcrank+257,	0,		0,	
ppcrank+61,	0,		0,	
ppcrank+130,	0,		0,	
ppcrank+271,	0,		0,	
ppcrank+156,	0,		0,	
ppcrank+284,	0,		ppvstop+57,
ppcrank+285,	0,		0,	
ppcrank+0,	0,		ppvstop+59,
ppcrank+297,	0,		0,	
ppcrank+348,	0,		ppvstop+61,
ppcrank+0,	0,		ppvstop+63,
0,	0,	0};
struct yywork *pptop = ppcrank+407;
struct yysvf *ppbgin = ppsvec+1;
char ppmatch[] = {
  0,   1,   1,   1,   1,   1,   1,   1, 
  1,   9,  10,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1, 
  9,   1,   1,   1,   1,   1,   1,  39, 
  1,   1,   1,   1,   1,   1,   1,   1, 
 48,  48,  48,  48,  48,  48,  48,  48, 
 48,  48,   1,  59,   1,   1,   1,   1, 
  1,  65,  65,  65,  65,  65,  65,  65, 
 65,  65,  65,  65,  65,  65,  65,  65, 
 65,  65,  65,  65,  65,  65,  65,  65, 
 65,  65,  65,   1,   1,   1,   1,  65, 
  1,  65,  65,  65,  65,  65,  65,  65, 
 65,  65,  65,  65,  65,  65,  65,  65, 
 65,  65,  65,  65,  65,  65,  65,  65, 
 65,  65,  65,   1,   1,   1,   1,   1, 
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
char ppextra[] = {
0,0,0,0,0,0,0,1,
1,1,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0};
/*	Copyright (c) 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ncform	6.8	95/02/11 SMI"

int pplineno =1;
# define YYU(x) x
# define NLSTATE ppprevious=YYNEWLINE
struct yysvf *pplstate [YYLMAX], **pplsp, **ppolsp;
char ppsbuf[YYLMAX];
char *ppsptr = ppsbuf;
int *ppfnd;
extern struct yysvf *ppestate;
int ppprevious = YYNEWLINE;
#if defined(__cplusplus) || defined(__STDC__)
int pplook(void)
#else
pplook()
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
	if (!ppmorfg)
		yylastch = pptext;
	else {
		ppmorfg=0;
		yylastch = pptext+ppleng;
		}
	for(;;){
		lsp = pplstate;
		ppestate = yystate = ppbgin;
		if (ppprevious==YYNEWLINE) yystate++;
		for (;;){
# ifdef LEXDEBUG
			if(debug)fprintf(ppout,"state %d\n",yystate-ppsvec-1);
# endif
			yyt = yystate->yystoff;
			if(yyt == ppcrank && !yyfirst){  /* may not be any transitions */
				yyz = yystate->yyother;
				if(yyz == 0)break;
				if(yyz->yystoff == ppcrank)break;
				}
#ifndef __cplusplus
			*yylastch++ = yych = input();
#else
			*yylastch++ = yych = lex_input();
#endif
			if(yylastch > &pptext[YYLMAX]) {
				fprintf(ppout,"Input string too long, limit %d\n",YYLMAX);
				exit(1);
			}
			yyfirst=0;
		tryagain:
# ifdef LEXDEBUG
			if(debug){
				fprintf(ppout,"char ");
				allprint(yych);
				putchar('\n');
				}
# endif
			yyr = yyt;
			if ( (int)yyt > (int)ppcrank){
				yyt = yyr + yych;
				if (yyt <= pptop && yyt->verify+ppsvec == yystate){
					if(yyt->advance+ppsvec == YYLERR)	/* error transitions */
						{unput(*--yylastch);break;}
					*lsp++ = yystate = yyt->advance+ppsvec;
					if(lsp > &pplstate[YYLMAX]) {
						fprintf(ppout,"Input string too long, limit %d\n",YYLMAX);
						exit(1);
					}
					goto contin;
					}
				}
# ifdef YYOPTIM
			else if((int)yyt < (int)ppcrank) {		/* r < ppcrank */
				yyt = yyr = ppcrank+(ppcrank-yyt);
# ifdef LEXDEBUG
				if(debug)fprintf(ppout,"compressed state\n");
# endif
				yyt = yyt + yych;
				if(yyt <= pptop && yyt->verify+ppsvec == yystate){
					if(yyt->advance+ppsvec == YYLERR)	/* error transitions */
						{unput(*--yylastch);break;}
					*lsp++ = yystate = yyt->advance+ppsvec;
					if(lsp > &pplstate[YYLMAX]) {
						fprintf(ppout,"Input string too long, limit %d\n",YYLMAX);
						exit(1);
					}
					goto contin;
					}
				yyt = yyr + YYU(ppmatch[yych]);
# ifdef LEXDEBUG
				if(debug){
					fprintf(ppout,"try fall back character ");
					allprint(YYU(ppmatch[yych]));
					putchar('\n');
					}
# endif
				if(yyt <= pptop && yyt->verify+ppsvec == yystate){
					if(yyt->advance+ppsvec == YYLERR)	/* error transition */
						{unput(*--yylastch);break;}
					*lsp++ = yystate = yyt->advance+ppsvec;
					if(lsp > &pplstate[YYLMAX]) {
						fprintf(ppout,"Input string too long, limit %d\n",YYLMAX);
						exit(1);
					}
					goto contin;
					}
				}
			if ((yystate = yystate->yyother) && (yyt= yystate->yystoff) != ppcrank){
# ifdef LEXDEBUG
				if(debug)fprintf(ppout,"fall back to state %d\n",yystate-ppsvec-1);
# endif
				goto tryagain;
				}
# endif
			else
				{unput(*--yylastch);break;}
		contin:
# ifdef LEXDEBUG
			if(debug){
				fprintf(ppout,"state %d char ",yystate-ppsvec-1);
				allprint(yych);
				putchar('\n');
				}
# endif
			;
			}
# ifdef LEXDEBUG
		if(debug){
			fprintf(ppout,"stopped at %d with ",*(lsp-1)-ppsvec-1);
			allprint(yych);
			putchar('\n');
			}
# endif
		while (lsp-- > pplstate){
			*yylastch-- = 0;
			if (*lsp != 0 && (ppfnd= (*lsp)->yystops) && *ppfnd > 0){
				ppolsp = lsp;
				if(ppextra[*ppfnd]){		/* must backup */
					while(ppback((*lsp)->yystops,-*ppfnd) != 1 && lsp > pplstate){
						lsp--;
						unput(*yylastch--);
						}
					}
				ppprevious = YYU(*yylastch);
				pplsp = lsp;
				ppleng = yylastch-pptext+1;
				pptext[ppleng] = 0;
# ifdef LEXDEBUG
				if(debug){
					fprintf(ppout,"\nmatch ");
					sprint(pptext);
					fprintf(ppout," action %d\n",*ppfnd);
					}
# endif
				return(*ppfnd++);
				}
			unput(*yylastch);
			}
		if (pptext[0] == 0  /* && feof(ppin) */)
			{
			ppsptr=ppsbuf;
			return(0);
			}
#ifndef __cplusplus
		ppprevious = pptext[0] = input();
		if (ppprevious>0)
			output(ppprevious);
#else
		ppprevious = pptext[0] = lex_input();
		if (ppprevious>0)
			lex_output(ppprevious);
#endif
		yylastch=pptext;
# ifdef LEXDEBUG
		if(debug)putchar('\n');
# endif
		}
	}
#if defined(__cplusplus) || defined(__STDC__)
int ppback(int *p, int m)
#else
ppback(p, m)
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
int ppinput(void)
#else
ppinput()
#endif
{
#ifndef __cplusplus
	return(input());
#else
	return(lex_input());
#endif
	}
#if defined(__cplusplus) || defined(__STDC__)
void ppoutput(int c)
#else
ppoutput(c)
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
void ppunput(int c)
#else
ppunput(c)
   int c; 
#endif
{
	unput(c);
	}
