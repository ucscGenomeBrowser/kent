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

char *asTypesIntSizeDescription(enum asTypes type);
/* Return description of integer size.  Do not free. */

int asTypesIntSize(enum asTypes type);
/* Return size in bytes of any integer type - short, long, unsigned, etc. */

boolean asTypesIsUnsigned(enum asTypes type);
/* Return TRUE if it's any integer type - short, long, unsigned, etc. */

boolean asTypesIsInt(enum asTypes type);
/* Return TRUE if it's any integer type - short, long, unsigned, etc. */

boolean asTypesIsFloating(enum asTypes type);
/* Return TRUE if it's any floating point type - float or double. */

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
    char *djangoName;              /* Django type name */
    };

struct asTypeInfo *asTypeFindLow(char *name);
/* Return asType for a low level type of given name.  (Low level because may be decorated 
 * with array or pointer  stuff at a higher level).  Returns NULL if not found. */

struct asIndex
/* Information about an index */
    {
    struct asIndex *next;   /* In case it needs to be on a list. */
    char *type;	/* 'primary' 'index' or 'uniq' to pass to SQL */
    int size;	/* If nonzero only index prefix of this many chars. */
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
    bool autoIncrement;		   /* TRUE if we want to auto_increment this field. */
    struct slName *values;         /* values for symbolic types */
    struct asIndex *index;	   /* Possibly null index description. */
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

struct dyString *asColumnToSqlType(struct asColumn *col);
/* Convert column to a sql type spec in returned dyString */

char *asTypeNameFromSqlType(char *sqlType);
/* Return the autoSql type name (not enum) for the given SQL type, or NULL.
 * Don't attempt to free result. */

struct asObject *asParseFile(char *fileName);
/* Parse autoSql .as file. */

struct asObject *asParseText(char *text);
/* Parse autoSql from text (as opposed to file). */

void asObjectFree(struct asObject **as);
/* free a single asObject */

void asObjectFreeList(struct asObject **as);
/* free a list of asObject */

void asColumnFree(struct asColumn **as);
/* free a single asColumn */

void asColumnFreeList(struct asColumn **as);
/* free a list of asColumn */

struct asColumn *asColumnFind(struct asObject *as, char *name);
/* Return column of given name from object, or NULL if not found. */

int asColumnFindIx(struct asColumn *list, char *name);
/* Return index of first element of asColumn list that matches name.
 * Return -1 if not found. */

int asColumnMustFindIx(struct asColumn *list, char *name);
/* Return index of first element of asColumn list that matches name.
 * errAbort if not found. */

boolean asCompareObjs(char *name1, struct asObject *as1, char *name2, struct asObject *as2, int numColumnsToCheck,
 int *retNumColumnsSame, boolean abortOnDifference);
/* Compare as-objects as1 and as2 making sure several important fields show they are the same name and type.
 * If difference found, print it to stderr.  If abortOnDifference, errAbort.
 * Othewise, return TRUE if the objects columns match through the first numColumnsToCheck fields. 
 * If retNumColumnsSame is not NULL, then it will be set to the number of contiguous matching columns. */

INLINE boolean asObjectsMatch(struct asObject *as1, struct asObject *as2)
{
int colCount = slCount(as1->columnList);
if (slCount(as2->columnList) != colCount)
    return FALSE;
return asCompareObjs(as1->name, as1, as2->name, as2, colCount, NULL, FALSE);
}

boolean asColumnNamesMatchFirstN(struct asObject *as1, struct asObject *as2, int n);
/* Compare only the column names of as1 and as2, not types because if an asObj has been
 * created from sql type info, longblobs are cast to lstrings but in the proper autoSql
 * might be lists instead (e.g. longblob in sql, uint exonStarts[exonCount] in autoSql. */

#endif /* ASPARSE_H */
