#ifndef RETRIEVEH
#define RETRIEVEH
/*        $Header: /home/smart/release/src/h/retrieve.h,v 11.2 1993/02/03 16:47:41 smart Exp $*/

#include "eid.h"
#include "tr_vec.h"
#include "proc.h"
#include "buf.h"
#include "vector.h"

/* QUERY_VECTOR is designed to accommodate all types of vectors.  Procedures
   to read a query (eg "retrieve.get_query"), and operate on the query
   (eg. "retrieve.coll_sim") must agree on the interpretation of the
   vector sub-field.  node_num indicates the portion of the vector that
   is of interest at the moment (as when querying against a subtree of
   a tree represented as a vector of nodes). */
typedef struct {
    long qid;
    long node_num;
    char *vector;
} QUERY_VECTOR;

/* Q_VEC_PAIR defines the pair of a query_vector (with query_vector being of
   unspecified structure) and pure vector.  The procedure sim_seq compares
   these two arguments, returning a similarity and a possible explanation
   of the similarity */
typedef struct {
    QUERY_VECTOR *query_vec;
    VEC *dvec;
} Q_VEC_PAIR;

typedef struct {
    float sim;
    SM_BUF *explanation;
} Q_VEC_RESULT;

typedef struct {
    long did;
    float sim;
} TOP_RESULT;

/* top_results is the top documents wanted (those a user could see), and is
   kept in decreasing rank order, sim = 0.0 indicates not retrieved */
/* full_results is the sim of all documents and is an array of sims indexed
   by the did */
typedef struct {
    long qid;
    TOP_RESULT *top_results;
    long num_top_results;
    float *full_results;
    long num_full_results;
    float min_top_sim;          /* Minimum sim of top_results */
} RESULT;

/* Conglomeration giving both query and results */
typedef struct {
    QUERY_VECTOR *query;    /* Current query */
    TR_VEC *tr;             /* Rel judgements for top ranked docs this query */
} RETRIEVAL;

/* Alternate result reporting */
typedef struct {
    EID qid;
    EID did;
    float sim;
} RESULT_TUPLE;

typedef struct {
    long num_results;
    RESULT_TUPLE *results;
} ALT_RESULTS;

/* This should probably be a spec parameter, but really shouldn't ever
   get used */
#define MAX_DID_DEFAULT 16000

/* Used to keep track of results for parts of docs (eg paragraphs) */
typedef struct {
        long max_doc;
        long max_parts;
        float *sum_results;
        float *max_results;
        int *num_parts;
} PARTS_RESULTS;

#endif /* RETRIEVEH */
