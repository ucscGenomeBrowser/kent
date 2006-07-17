/* asParse - parse out an autoSql .as file. */

#ifndef ASPARSE_H
#define ASPARSE_H

enum asTypes
/* Different low level types (not including lists and objects) */
   {
   t_double,   /* double precision floating point. */
   t_float,    /* single precision floating point. */
   t_char,     /* character or fixed size character array. */
   t_int,      /* signed 32 bit integer */
   t_uint,     /* unsigned 32 bit integer */
   t_short,    /* signed 16 bit integer */
   t_ushort,   /* unsigned 16 bit integer */
   t_byte,     /* signed 8 bit integer */
   t_ubyte,    /* unsigned 8 bit integer */
   t_off,      /* 64 bit integer. */
   t_string,   /* varchar/char * (variable size string up to 255 chars)  */
   t_lstring,     /* variable sized large string. */
   t_object,   /* composite object - object/table - forms lists. */
   t_simple,   /* simple composite object - forms arrays. */
   t_enum,     /* enumerated symbolic values */
   t_set,      /* set of symbolic values */
   };

struct asTypeInfo
    {
    enum asTypes type;		   /* Numeric ID of low level type. */
    char *name;                    /* Text ID of low level type. */
    bool isUnsigned;               /* True if an unsigned int of some type. */
    bool stringy;                  /* True if a string or blob. */
    char *sqlName;                 /* SQL type name. */
    char *cName;                   /* C type name. */
    char *listyName;               /* What functions that load a list are called. */
    char *nummyName;               /* What functions that load a number are called. */
    char *outFormat;		   /* Output format for printf. %d, %u, etc. */
    };

struct asColumn
/* Info on one column/field */
    {
    struct asColumn *next;           /* Next column. */
    char *name;                    /* Column name. */
    char *comment;		   /* Comment string on column. */
    struct asTypeInfo *lowType;   /* Root type info. */
    char *obName;                  /* Name of object or table. */
    struct asObject *obType;       /* Name of composite object. */
    int fixedSize;		   /* 0 if not fixed size, otherwise size of list. */
    char *linkedSizeName;          /* Points to variable that holds size of list. */
    struct asColumn *linkedSize;     /* Column for linked size. */
    bool isSizeLink;               /* Flag to tell if have read link. */
    bool isList;                   /* TRUE if a list. */
    bool isArray;                  /* TRUE if an array. */
    struct slName *values;         /* values for symbolic types */
    };

struct asObject
/* Info on whole asObject. */
    {
    struct asObject *next;
    char *name;			/* Name of object. */
    char *comment;		/* Comment describing object. */
    struct asColumn *columnList;  /* List of columns. */
    bool isTable;	        /* True if a table. */
    bool isSimple;	        /* True if a simple object. */
    };

struct asObject *asParseFile(char *fileName);
/* Parse autoSql .as file. */

struct asObject *asParseText(char *text);
/* Parse autoSql from text (as opposed to file). */

#endif /* ASPARSE_H */
