/* Tables - the data structures for holding all the info about
 * a table, it's fields, and it's relationships to other tables. */

/* Copyright (C) 2005 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef TABLES_H
#define TABLES_H

struct table
/* Information about one of the tables we are making. */
    {
    struct table *next;		/* Next in list. */
    char *name;			/* Name of table. */
    struct field *fieldList;	/* Information about each field. */
    struct hash *fieldHash;	/* Fields keyed by field name. */
    int fieldCount;		/* Count of fields, including made up ones. */
    struct hash *fieldMixedHash;/* Fields keyed by field mixed case name. */
    struct elStat *elStat;	/* Associated elStat structure. */
    struct dtdElement *dtdElement; /* Associated dtd element. */
    struct field *primaryKey;	/* Primary key if any. */
    boolean madeUpPrimary;	/* True if we are creating primary key. */
    int lastId;			/* Last id value if we create key. */
    struct assocRef *parentAssocs;  /* Association table linking to parent. */
    struct fieldRef *parentKeys; /* References to possible parents. */
    boolean linkedParents;	/* True if we have linked parents. */
    boolean isAssoc;		/* True if an association we've created. */
    int usesAsChild;		/* Number of times this is a child of another
                                 * table. */
    FILE *tabFile;		/* Tab oriented file associated with table */
    struct hash *uniqHash;	/* Table to insure unique output. */
    struct assoc *assocList;    /* List of pending associations. */
    boolean promoted;		/* If true table is promoted to be field in parent. */
    };

struct table *tableNew(char *name, struct elStat *elStat,
	struct dtdElement *dtdElement);
/* Create a new table structure. */

struct field
/* Information about a field. */
    {
    struct field *next;		/* Next in list. */
    char *name;			/* Name of field as it is in XML. */
    char *mixedCaseName;	/* Mixed case name - as it is in SQL. */
    struct table *table;	/* Table this is part of. */
    int tablePos;		/* Field position in table (zero based) */
    struct attStat *attStat;	/* Associated attStat structure. */
    struct dtdAttribute *dtdAttribute;	/* Associated dtd attribute. */
    boolean isMadeUpKey;	/* True if it's a made up key. */
    boolean isPrimaryKey;	/* True if it's table's primary key. */
    boolean isString;		/* True if it's a string field. */
    };

struct field *addFieldToTable(struct table *table, 
	char *name, char *mixedCaseName,
	struct attStat *att, boolean isMadeUpKey, boolean atTail, 
	boolean isString, char *textFieldName);
/* Add field to end of table.  Use proposedName if it's unique,
 * otherwise proposed name with some numerical suffix. */

struct fieldRef
/* A reference to a field. */
    {
    struct fieldRef *next;	/* Next in list */
    struct field *field;	/* Associated field. */
    };

struct assocRef
/* A reference to a table. */
    {
    struct assocRef *next;	/* Next in list. */
    struct table *assoc;	/* Association table */
    struct table *parent;	/* Parent table we're associated with */
    };

struct dtdAttribute *findDtdAttribute(struct dtdElement *element, char *name);
/* Find named attribute in element, or NULL if no such attribute. */

#endif /* TABLES_H */
