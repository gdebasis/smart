#ifndef TRIEH
#define TRIEH
/*        $Header: /home/smart/release/src/h/trie.h,v 11.2 1993/02/03 16:47:53 smart Exp $*/

/* #define DEBUG */

typedef unsigned long TRIE;
#define MAX_SIZE_STRING 100


/* Trie is represented by an array of longs which contain a compact
   hierarchy interspersed with the values for the string key. In a hierarchy
   node, the right 7 bits represent the ascii char trie component, the next
   rightmost bit tells whether this node has a value, the next bit tells
   whether this node has any children, the next bit tells whether this is
   the last child in the list of it's parent.  The leftmost 22 bits give
   an integer offset into the trie structure, giving the starting
   location for the children and/or value of this node.  If this node
   has a value, the integer offset points to that value, with any children
   being in the longs following the value.
*/
/*
31                    9876      0
_________________________________
|                     ||||      |
---------------------------------
|                     ||||ascii value (7 bit)
|                     ||| has_value (1 bit)
|                     ||  has_children (1 bit)
|                     |   last_child (1 bit)
|                         children_pointer    

*/

#define ascii_char(node)   (*(node) & 0x7f)
#define has_value(node)    (*(node) & 0x80)
#define has_children(node) (*(node) & 0x100)
#define last_child(node)   (*(node) & 0x200)
#define children_ptr(node) (*(node) >> 10)

#define set_ascii_char(node, value)   *node = (*node & ~0x7f) | (value & 0x7f)
#define set_has_value(node, value)    *node = (value) ? *node | (1 << 7) \
                                                      : *node & ~(1 << 7)
#define set_has_children(node, value) *node = (value) ? *node | (1 << 8) \
                                                      : *node & ~(1 << 8)
#define set_last_child(node, value)   *node = (value) ? *node | (1 << 9) \
                                                      : *node & ~(1 << 9)
#define set_children_ptr(node, value) *node = ((value)<< 10) | (*node & 0x3ff)


#define set_value(node, value)        *node = value
#define get_value(node)               (*(node))
/* #define read_value(value)             scanf ("%lu\n", (NODE *) &value)*/

#define put_ascii(node)               (void) printf ("%lu", *node)

#define set_null(node)                *node = 0;

/* The following definitions are used for approximate matching of strings
   in trie */

typedef struct {
    char *string;        /* input string, NULL terminated */
    TRIE *trie;          /* trie (or portion of trie) to be searched */
} TRIE_NEAR_INPUT;

typedef struct {
    long *matches;       /* matches[0:num_matches] is array of near matches 
                            seen so far */
    long num_matches;
    long max_num_matches; /* Current space reserved (via malloc) for matches */
} TRIE_NEAR_OUTPUT;

#endif /* TRIEH */
