%{
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

%}

%union {
  short  ival;
  double dval;
  }

%token <ival> QID      1
%token <ival> OP       2
%token <ival> NOT      3
%token        INF      4
%token        NUMBER   5
%token <ival> KEYWORD  6
%token        LP       7
%token        RP       8
%token        COMMA    9
%token        SEMI     10

%type <ival> queries
%type <ival> query
%type <ival> qexp
%type <ival> notexp
%type <ival> opexp
%type <ival> list
%type <dval> pval
%start queries

%%   /* start of rules section */

queries     : query
            | queries query 
            ;

query       : QID {
fprintf(stderr, "QID = %d\n", $1);
                qid = $1;
                qvector.id_num.id = (long)$1;
		EXT_NONE(qvector.id_num.ext);
                ni_ptr = node_info;
                num_used_nodes = 0;
              }
              qexp SEMI { int val;

                if (UNDEF != fix_up($3)) {
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
                $$ = 0;
              }
            | error SEMI {
                /* if there is a syntax error, try to parse next query */
                  fprintf(stderr,"syntax error around qid %d\n",qid);
                  $$ = UNDEF;
              }
            ;

qexp        : KEYWORD { DICT_ENTRY dict_entry;

/*
fprintf(stderr, "KEYWORD = %s\n", &yytext[0]);
*/

                ALLOC_TREE(num_used_nodes);
                $$ = num_used_nodes;
                tree_ptr = tree + num_used_nodes;
                tree_ptr->sibling = UNDEF;
                tree_ptr->child = UNDEF;
                
                dict_entry.con = UNDEF;
		dict_entry.token = &yytext[0];
                switch(seek_dict(dict_index,&dict_entry)) {
                case 0:
                    fprintf(stderr,"(%d: <%s,%d> not in dictionary)\n",
                      qid,yytext,$1);
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
                    ni_ptr->ctype = $1;		/* $1 is ctype */
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
              }
            | notexp {$$ = $1;}
            | opexp  {$$ = $1;}
            ;


notexp      : NOT LP qexp RP {
                ALLOC_TREE(num_used_nodes);
                $$ = num_used_nodes;
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
                tree_ptr->child = $3;
              }
            ;

opexp       : OP pval LP list RP {
                long offset;

                offset = (tree + $4)->info;
                (node_info+offset)->type = $1;
                (node_info+offset)->p = $2;
                (node_info+offset)->wt = 1.0;
                $$ = $4;
              }
            ;

pval        : INF  	  { $$ = 0.0; }
            | NUMBER      { $$ = atof(yytext); }
            | /* empty */ { $$ = 0.0; }
            ;

list        : qexp COMMA qexp { 
                /* first two elements in the list - must allocate parent */
                  ALLOC_TREE(num_used_nodes);
                  $$ = num_used_nodes;
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
                tree_ptr->child = $1;
                tree_ptr->sibling = UNDEF;
                (tree + $1)->sibling = $3;
              }
            | list COMMA qexp {
                (tree + $3)->sibling = (tree + $1)->child;
                (tree + $1)->child = $3;
                $$ = $1;
              } 
            ;
%%
yyerror(s)
  char *s;

  {
    fprintf(stderr,"pparser: syntax error.\n"); 
    fprintf(stderr,"error string: '%s', qid: %d, yytext: '%s'\n",
      s,qid,yytext);
    exit(1);
  }
