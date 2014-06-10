/* Copyright (C) 2010 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


struct tdbFilePos
/* Position in file. */
    {
    struct tdbFilePos *next;	/* Next in list. */
    char *fileName;		/* Name of file, not allocated here. */
    int startLineIx;		/* File start line. */
    int endLineIx;		/* File end line. */
    char *text;			/* Text representation of stanza including preceding comments. 
    				 * Not allocated here. */
    };

struct tdbField
/* A single field. */
    {
    struct tdbField *next;	/* Next in list. */
    char *name;		/* Field name. */
    char *val;	/* Field value. */
    };

struct tdbRecord
/* A single RA record. */
    {
    struct tdbRecord *next;	/* Next in master (non-hierarchical) list. */
    struct tdbRecord *parent;	/* Parent if any. */
    struct tdbRecord *children;	/* Children - youngest to oldest. */
    struct tdbRecord *olderSibling;	/* Parent to older sibling if any. */
    char *key;			/* First word of value of key field. */
    struct tdbField *fieldList;	/* List of fields. */
    struct tdbFilePos *posList;	/* Position of file. */
    char *db;			/* Database if any. */
    boolean override;		/* Override is in key. */
    };

struct tdbField *tdbFieldNew(char *name, char *val, struct lm *lm);
/* Return new tdbField. */

struct tdbField *tdbRecordField(struct tdbRecord *ra, char *fieldName);
/* Return named field if it exists, otherwise NULL */

char *tdbRecordFieldVal(struct tdbRecord *record, char *fieldName);
/* Return value of named field if it exists, otherwise NULL */

struct tdbRecord *tdbRecordReadOne(struct lineFile *lf, char *key, struct lm *lm);
/* Read next record from file. Returns NULL at end of file. */

INLINE int tdbRecordLineIx(struct tdbRecord *record)
/* Return line index of record start. */
{
return record->posList->startLineIx;
}

INLINE char * tdbRecordFileName(struct tdbRecord *record)
/* Return fileName of record */
{
return record->posList->fileName;
}
