#ifndef DIRECT_TYPEH
#define DIRECT_TYPEH

#define MAXNUMFIELDS 10

typedef struct {
    char *att_name;         /* Name of attribute within object */
    char att_type;          /* Type of attribute (fixed, float etc) */
    char field_size;        /* size of attribute in bytes */
    char field_offset;      /* byte offset of attribute within tuple */
    char associated_field;  /* if var_field, the fixed_field giving the */
                            /* number tuples for instance of attribute */
} ATT_TYPES;


typedef struct  {
    char *name;                 /* Name of the relational object */
    char num_fixed_fields;      /* Number of fixed fields in object */
    char num_var_fields;        /* Number of variable fields in object */

    char primary_key;           /* Not used yet */
    char secondary_key;         /* Not used yet */
    
    ATT_TYPES atts[MAXNUMFIELDS];

} DIRECT_TYPES;

/* The fixed entry of each direct object is composed of */
/*        one long entry_number (it must be the first attribute) */
/*        (num_fixed_fields - 1) entries, each of size specified by */
/*            atts[i].field_size for the i'th entry */
/*        one long var_offset, giving the byte offset of the first */
/*            var entry for this tuple */

/* The variable entry of each direct object is composed of */
/*        num_var_fields variable length records, each of length */
/*            (atts[i].field_size * fixed_field[atts[i].associated_field]) */
/*            rounded up to a multiple of 4. Fixed_field is the value of */
/*            the appropriate field within the fixed entry. */
/*            If atts[i].associated_field is UNDEF, then the field is */
/*            assumed to be a null terminated string */


/* Attribute types */
#define ATT_INT 0               /* integer of length 1, 2, or 4 bytes */
#define ATT_FLT 1               /* floating point number of length 4 bytes */
#define ATT_STR 2               /* Null terminated character string pointer */
#define ATT_GEN 3               /* Generic type, any length (up to 128?) */
#define ATT_UND 4               /* Undefined type - error if encountered */

/* Relational types (see direct.c for definitions of each type) */
/* Each is really defined in the appropriate .h file (eg graph.h) */
/*
#define REL_GRAPH 0
#define REL_INV 1
#define REL_VECTOR 2
#define REL_DISPLAY 3
#define REL_SIMP_INV 4
#define REL_HIERARCHY 5
#define REL_ARRAY 6
#define REL_PNORM_VEC 7
#define REL_PART_VEC 14
#define REL_FDBKSTATS 15
#define REL_ADV_VEC 17
*/
#endif /* DIRECT_TYPEH */
