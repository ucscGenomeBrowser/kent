
struct tdbFilePos
/* Position in file. */
    {
    struct tdbFilePos *next;	/* Next in list. */
    char *fileName;		/* Name of file, not allocated here. */
    int lineIx;			/* File line. */
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
    struct slPair *settingsByView;  /* Parsed out settingsByView field if any. */
    struct hash *subGroups;	/* Parsed out subGroup field if any. */
    char *view;			/* View out of the subGroups if it exists. */
    struct hash *viewHash;	/* Hash of view subgroupN if any. */
    };

struct tdbFilePos *tdbFilePosNew(struct lm *lm, char *fileName, int lineIx);
/* Create new tdbFilePos record. */

struct tdbField *tdbFieldNew(char *name, char *val, struct lm *lm);
/* Return new tdbField. */

struct tdbField *tdbRecordField(struct tdbRecord *ra, char *fieldName);
/* Return named field if it exists, otherwise NULL */

struct tdbRecord *tdbRecordReadOne(struct lineFile *lf, char *key, struct lm *lm);
/* Read next record from file. Returns NULL at end of file. */

