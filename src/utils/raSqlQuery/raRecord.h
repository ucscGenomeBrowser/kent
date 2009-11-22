
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
    struct raRecord *next;	/* Next in list. */
    char *key;			/* First word of value of key field. */
    struct raField *fieldList;	/* List of fields. */
    struct raFilePos *posList;	/* Position of file. */
    boolean override;		/* Override is in key. */
    };

struct raFilePos *raFilePosNew(struct lm *lm, char *fileName, int lineIx);
/* Create new raFilePos record. */

struct raField *raRecordField(struct raRecord *ra, char *fieldName);
/* Return named field if it exists, otherwise NULL */

struct raField *raFieldFromLine(char *line, struct lm *lm);
/* Parse out line and convert it to a raField.  Will return NULL on empty lines. 
 * Will insert some zeroes into the input line as well. */

struct raRecord *raRecordReadOne(struct lineFile *lf, char *key, struct lm *lm);
/* Read next record from file. Returns NULL at end of file. */

