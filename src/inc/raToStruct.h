/* raToStruct - stuff to help read ra files into C structures.  Works with raToStructGen
 * which makes parsers based on .as files. */

#ifndef RATOSTRUCT_H

struct raToStructReader
/* Something to help us parse RAs into C structures. */
    {
    struct raToStructReader *next;
    char *name;		      /* Name of structure */
    int fieldCount;	      /* Number of fields. */
    char **fields;	      /* Names of all fields - not allocated here. */
    char **requiredFields;    /* Names of required fields - not allocated here */
    int requiredFieldCount;   /* Count of required fields. */
    struct hash *fieldIds;    /* So we can do hashLookup/switch instead of strcmp chain */
    int *requiredFieldIds;    /* An array of IDs of required fields. */
    bool *fieldsObserved;  /* An entry for each field we've observed. */
    };

struct raToStructReader *raToStructReaderNew(char *name,  int fieldCount, char **fields,  
    int requiredFieldCount, char **requiredFields);
/* Create a helper object for parsing an ra file into a C structure. */

void raToStructReaderFree(struct raToStructReader **pReader);
/* Free up memory associated with reader. */

void raToStructReaderCheckRequiredFields(struct raToStructReader *reader, struct lineFile *lf);
/* Make sure that all required files have been seen in the stanza we just parsed. */

void raToStructArraySignedSizer(struct lineFile *lf, int curSize, int *pSize, char *fieldName);
/* If *pSize is zero,  set it to curSize,  othersize check that it is the same as curSize.
 * The lf and fieldName are for error reporting. */

void raToStructArrayUnsignedSizer(struct lineFile *lf, unsigned curSize, 
    unsigned *pSize, char *fieldName);
/* If *pSize is zero,  set it to curSize,  othersize check that it is the same as curSize.
 * The lf and fieldName are for error reporting. */

void raToStructArrayShortSizer(struct lineFile *lf, short curSize, 
    short *pSize, char *fieldName);
/* If *pSize is zero,  set it to curSize,  othersize check that it is the same as curSize.
 * The lf and fieldName are for error reporting. */

void raToStructArrayUshortSizer(struct lineFile *lf, unsigned short curSize, 
    unsigned short *pSize, char *fieldName);
/* If *pSize is zero,  set it to curSize,  othersize check that it is the same as curSize.
 * The lf and fieldName are for error reporting. */

void raToStructArrayByteSizer(struct lineFile *lf, signed char curSize, 
    signed char *pSize, char *fieldName);
/* If *pSize is zero,  set it to curSize,  othersize check that it is the same as curSize.
 * The lf and fieldName are for error reporting. */

void raToStructArrayUbyteSizer(struct lineFile *lf, unsigned char curSize, 
    unsigned char *pSize, char *fieldName);
/* If *pSize is zero,  set it to curSize,  othersize check that it is the same as curSize.
 * The lf and fieldName are for error reporting. */

void raToStructArrayLongLongSizer(struct lineFile *lf, long long curSize, 
    long long *pSize, char *fieldName);
/* If *pSize is zero,  set it to curSize,  othersize check that it is the same as curSize.
 * The lf and fieldName are for error reporting. */

#endif /* RATOSTRUCT_H */
