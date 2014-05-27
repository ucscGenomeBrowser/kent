/* raToStruct - stuff to help read ra files into C structures.  Works with raToStructGen
 * which makes parsers based on .as files. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "raToStruct.h"
#include "obscure.h"

struct raToStructReader *raToStructReaderNew(char *name,  int fieldCount, char **fields,  
    int requiredFieldCount, char **requiredFields)
/* Create a helper object for parsing an ra file into a C structure.  This structure will
 * contain */
{
struct raToStructReader *reader;
AllocVar(reader);
reader->name = cloneString(name);
reader->fieldCount = fieldCount;
reader->fields = fields;
reader->requiredFieldCount = requiredFieldCount;
reader->requiredFields = requiredFields;
struct hash *fieldIds = reader->fieldIds = hashNew(4);
int i;
for (i=0; i<fieldCount; ++i)
    hashAddInt(fieldIds, fields[i], i);
if (requiredFieldCount > 0)
    {
    AllocArray(reader->requiredFieldIds, requiredFieldCount);
    for (i=0; i<requiredFieldCount; ++i)
        {
	char *required = requiredFields[i];
	struct hashEl *hel = hashLookup(fieldIds, required);
	if (hel == NULL)
	    errAbort("Required field %s is not in field list", required);
	reader->requiredFieldIds[i] = ptToInt(hel->val);
	}
    }
AllocArray(reader->fieldsObserved, fieldCount);
return reader;
}

void raToStructReaderFree(struct raToStructReader **pReader)
/* Free up memory associated with reader. */
{
struct raToStructReader *reader = *pReader;
if (reader != NULL)
    {
    freeMem(reader->name);
    freeHash(&reader->fieldIds);
    freeMem(reader->fieldIds);
    freeMem(reader->fieldsObserved);
    freez(pReader);
    }
}

void raToStructReaderCheckRequiredFields(struct raToStructReader *reader, struct lineFile *lf)
/* Make sure that all required files have been seen in the stanza we just parsed. */
{
int *requiredFieldIds = reader->requiredFieldIds;
bool *fieldsObserved = reader->fieldsObserved;
int i;
for (i=0; i<reader->requiredFieldCount; ++i)
    {
    if (!fieldsObserved[requiredFieldIds[i]])
	{
	errAbort("Required field %s not found line %d of %s", reader->requiredFields[i],
	    lf->lineIx, lf->fileName);
	}
    }
}

void raToStructArraySignedSizer(struct lineFile *lf, int curSize, int *pSize, char *fieldName)
/* If *pSize is zero,  set it to curSize,  othersize check that it is the same as curSize.
 * The lf and fieldName are for error reporting. */
{
int oldSize = *pSize;
if (oldSize == 0)
    *pSize = curSize;
else if (curSize != oldSize)
    errAbort("Array dimension mismatch between %s and another field in stanza ending line %d of %s",
	fieldName, lf->lineIx, lf->fileName);
}

void raToStructArrayUnsignedSizer(struct lineFile *lf, unsigned curSize, 
    unsigned *pSize, char *fieldName)
/* If *pSize is zero,  set it to curSize,  othersize check that it is the same as curSize.
 * The lf and fieldName are for error reporting. */
{
unsigned oldSize = *pSize;
if (oldSize == 0)
    *pSize = curSize;
else if (curSize != oldSize)
    errAbort("Array dimension mismatch between %s and another field in stanza ending line %d of %s",
	fieldName, lf->lineIx, lf->fileName);
}

void raToStructArrayShortSizer(struct lineFile *lf, short curSize, 
    short *pSize, char *fieldName)
/* If *pSize is zero,  set it to curSize,  othersize check that it is the same as curSize.
 * The lf and fieldName are for error reporting. */
{
short oldSize = *pSize;
if (oldSize == 0)
    *pSize = curSize;
else if (curSize != oldSize)
    errAbort("Array dimension mismatch between %s and another field in stanza ending line %d of %s",
	fieldName, lf->lineIx, lf->fileName);
}

void raToStructArrayUshortSizer(struct lineFile *lf, unsigned short curSize, 
    unsigned short *pSize, char *fieldName)
/* If *pSize is zero,  set it to curSize,  othersize check that it is the same as curSize.
 * The lf and fieldName are for error reporting. */
{
unsigned short oldSize = *pSize;
if (oldSize == 0)
    *pSize = curSize;
else if (curSize != oldSize)
    errAbort("Array dimension mismatch between %s and another field in stanza ending line %d of %s",
	fieldName, lf->lineIx, lf->fileName);
}

void raToStructArrayByteSizer(struct lineFile *lf, signed char curSize, 
    signed char *pSize, char *fieldName)
/* If *pSize is zero,  set it to curSize,  othersize check that it is the same as curSize.
 * The lf and fieldName are for error reporting. */
{
signed char oldSize = *pSize;
if (oldSize == 0)
    *pSize = curSize;
else if (curSize != oldSize)
    errAbort("Array dimension mismatch between %s and another field in stanza ending line %d of %s",
	fieldName, lf->lineIx, lf->fileName);
}

void raToStructArrayUbyteSizer(struct lineFile *lf, unsigned char curSize, 
    unsigned char *pSize, char *fieldName)
/* If *pSize is zero,  set it to curSize,  othersize check that it is the same as curSize.
 * The lf and fieldName are for error reporting. */
{
unsigned char oldSize = *pSize;
if (oldSize == 0)
    *pSize = curSize;
else if (curSize != oldSize)
    errAbort("Array dimension mismatch between %s and another field in stanza ending line %d of %s",
	fieldName, lf->lineIx, lf->fileName);
}

void raToStructArrayLongLongSizer(struct lineFile *lf, long long curSize, 
    long long *pSize, char *fieldName)
/* If *pSize is zero,  set it to curSize,  othersize check that it is the same as curSize.
 * The lf and fieldName are for error reporting. */
{
long long oldSize = *pSize;
if (oldSize == 0)
    *pSize = curSize;
else if (curSize != oldSize)
    errAbort("Array dimension mismatch between %s and another field in stanza ending line %d of %s",
	fieldName, lf->lineIx, lf->fileName);
}

#ifdef NEVER
void raToStructArrayXyzSizer(struct lineFile *lf, xyz curSize, 
    xyz *pSize, char *fieldName)
/* If *pSize is zero,  set it to curSize,  othersize check that it is the same as curSize.
 * The lf and fieldName are for error reporting. */
{
xyz oldSize = *pSize;
if (oldSize == 0)
    *pSize = curSize;
else if (curSize != oldSize)
    errAbort("Array dimension mismatch between %s and another field in stanza ending line %d of %s",
	fieldName, lf->lineIx, lf->fileName);
}
#endif /* NEVER */
