/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


struct raFilePos
/* Position in file. */
    {
    struct raFilePos *next;	/* Next in list. */
    char *fileName;		/* Name of file, not allocated here. */
    int lineIx;			/* File line. */
    };

struct raField
/* A single field. */
    {
    struct raField *next;	/* Next in list. */
    char *name;		/* Field name. */
    char *val;	/* Field value. */
    };

struct raRecord
/* A single RA record. */
    {
    struct raRecord *next;	/* Next in master (non-hierarchical) list. */
    struct raRecord *parent;	/* Parent if any. */
    struct raRecord *children;	/* Children - youngest to oldest. */
    struct raRecord *olderSibling;	/* Parent to older sibling if any. */
    char *key;			/* First word of value of key field. */
    struct raField *fieldList;	/* List of fields. */
    struct raFilePos *posList;	/* Position of file. */
    char *db;			/* Database if any. */
    boolean override;		/* Override is in key. */
    struct slPair *settingsByView;  /* Parsed out settingsByView field if any. */
    struct hash *subGroups;	/* Parsed out subGroup field if any. */
    char *view;			/* View out of the subGroups if it exists. */
    struct hash *viewHash;	/* Hash of view subgroupN if any. */
    };

struct raFilePos *raFilePosNew(struct lm *lm, char *fileName, int lineIx);
/* Create new raFilePos record. */

struct raField *raFieldNew(char *name, char *val, struct lm *lm);
/* Return new raField. */

struct raField *raRecordField(struct raRecord *ra, char *fieldName);
/* Return named field if it exists, otherwise NULL */

struct raField *raFieldFromLine(char *line, struct lm *lm);
/* Parse out line and convert it to a raField.  Will return NULL on empty lines. 
 * Will insert some zeroes into the input line as well. */

struct raRecord *raRecordReadOne(struct lineFile *lf, char *key, struct lm *lm);
/* Read next record from file. Returns NULL at end of file. */

