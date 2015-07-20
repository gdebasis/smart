#ifndef LTRIEH
#define LTRIEH
/*        $Header: /home/smart/release/src/h/ltrie.h,v 11.2 1993/02/03 16:47:53 smart Exp $*/

/* #define DEBUG */

typedef struct {
    long long1;
    long long2;
} LTRIE;
#define MAX_SIZE_STRING 100


/* Ltrie is represented by an array of longs which contain a compact
   hierarchy interspersed with the values for the string key. In a hierarchy
   node, the right 7 bits represent the ascii char ltrie component, the next
   rightmost bit tells whether this node has a value, the next bit tells
   whether this node has any children, the next bit tells whether this is
   the last child in the list of it's parent.  The leftmost 22 bits give
   an integer offset into the ltrie structure, giving the starting
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
/*
31                   10987      0
_________________________________
|                     ||||      |
---------------------------------
|                     ||||ascii value (8 bit)
|                     ||| has_value (1 bit)
|                     ||  has_children (1 bit)
|                     |   last_child (1 bit)


*/



#define ascii_char(node)   ((node)->long1 & 0xff)
#define has_value(node)    ((node)->long1 & 0x100)
#define has_children(node) ((node)->long1 & 0x200)
#define last_child(node)   ((node)->long1 & 0x400)
#define children_ptr(node) ((node)->long2)

#define set_ascii_char(node, value)   \
    (node)->long1 = ((node)->long1 & ~0xff) | (value & 0xff)
#define set_has_value(node, value)    \
    (node)->long1 = (value) ? (node)->long1 | (1 << 8) \
                              : (node)->long1 & ~(1 << 8)
#define set_has_children(node, value)  \
    (node)->long1 = (value) ? (node)->long1 | (1 << 9) \
                              : (node)->long1 & ~(1 << 9)
#define set_last_child(node, value)   \
    (node)->long1 = (value) ? (node)->long1 | (1 << 10) \
                              : (node)->long1 & ~(1 << 10)
#define set_children_ptr(node, value) \
    (node)->long2 = (value);


#define set_value(node, value)        (node)->long1 = value
#define get_value(node)               ((node)->long1)

#define put_ascii(node)               (void) printf ("%lu,%lu", node->long1, node->long2)

#define set_null(node)                node->long1 = 0; node->long2=0

/* The following definitions are used for approximate matching of strings
   in trie */

typedef struct {
    long *matches;       /* matches[0:num_matches] is array of near matches 
                            seen so far */
    long num_matches;
    long max_num_matches; /* Current space reserved (via malloc) for matches */
} TRIE_NEAR_OUTPUT;

#endif /* LTRIEH */
