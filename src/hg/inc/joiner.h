/* joiner - information about what fields in what tables
 * in what databases can fruitfully be related together
 * or joined.  Another way of looking at it is this
 * defines identifiers shared across tables. */

#ifndef JOINER_H
#define JOINER_H

struct joinerField
/* A field that can be joined on. */
    {
    struct joinerField *next;	/* Next in list. */
    int lineIx;			/* Line index of start for error reporting */
    struct slName *dbList;	/* List of possible databases. */
    char *table;		/* Associated table. */
    char *field;		/* Associated field. */
    struct slName *chopBefore;	/* Chop before strings */
    struct slName *chopAfter;	/* Chop after strings */
    char *separator;		/* Separators for lists or NULL if not list. */
    boolean indexOf;		/* True if id is index in this list. */
    boolean isPrimary;		/* True if is primary key. */
    boolean dupeOk;		/* True if duplication ok (in primary key) */
    boolean oneToOne;		/* Is 1-1 with primary key? */
    float minCheck;		/* Minimum ratio that must hit primary key */
    char *splitPrefix;		/* Prefix for splitting tables. */
    struct slName *exclude;	/* List of keys to exclude from verification */
    };

void joinerFieldFree(struct joinerField **pJf);
/* Free up memory associated with joinerField. */

void joinerFieldFreeList(struct joinerField **pList);
/* Free up memory associated with list of joinerFields. */

struct joinerSet
/* Information on a set of fields that can be joined together. */
    {
    struct joinerSet *next;		/* Next in list. */
    char *name;				/* Name of field set. */
    int lineIx;			/* Line index of start for error reporting */
    struct joinerSet *parent;		/* Parsed-out parent type if any. */
    char *typeOf;			/* Parent type name if any. */
    char *external;			/* External name if any. */
    char *description;			/* Short description. */
    struct joinerField *fieldList;	/* List of fields. */
    boolean isFuzzy;		/* True if no real primary key. */
    boolean expanded;		/* True if an expanded set. */
    };

void joinerSetFree(struct joinerSet **pJs);
/* Free up memory associated with joinerSet. */

void joinerSetFreeList(struct joinerSet **pList);
/* Free up memory associated with list of joinerSets. */

struct joiner
/* Manage joining identifier information across all databases. */
    {
    struct joiner *next;	/* Next in list. */
    char *fileName;		/* Associated file name */
    struct joinerSet *jsList;	/* List of identifiers. */
    struct hash *symHash;	/* Hash of symbols from file. */
    };

void joinerFree(struct joiner **pJoiner);
/* Free up memory associated with joiner */

struct joiner *joinerRead(char *fileName);
/* Read in a .joiner file. */

#endif /* JOINER_H */
