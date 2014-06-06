/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "localmem.h"
#include "hash.h"
#include "linefile.h"
#include "obscure.h"
#include "ra.h"
#include "dystring.h"
#include "trackDb.h"
#include "tdbRecord.h"


static struct tdbFilePos *tdbFilePosNew(struct lm *lm, 
    char *fileName, 		/* File name. */
    int startLineIx,		/* File start line. */
    int endLineIx,		/* File end line. */
    char *text)			/* Text of stanza. */
/* Create new tdbFilePos record. */
{
struct tdbFilePos *fp;
lmAllocVar(lm, fp);
fp->fileName = lmCloneString(lm, fileName);
fp->startLineIx = startLineIx;
fp->endLineIx = endLineIx;
fp->text = lmCloneString(lm, text);
return fp;
}

struct tdbField *tdbRecordField(struct tdbRecord *record, char *fieldName)
/* Return named field if it exists, otherwise NULL */
{
struct tdbField *field;
for (field = record->fieldList; field != NULL; field = field->next)
    {
    if (sameString(field->name, fieldName))
        return field;
    }
return NULL;
}

char *tdbRecordFieldVal(struct tdbRecord *record, char *fieldName)
/* Return value of named field if it exists, otherwise NULL */
{
struct tdbField *field = tdbRecordField(record, fieldName);
if (field != NULL)
    return field->val;
else
    return NULL;
}

struct tdbField *tdbFieldNew(char *name, char *val, struct lm *lm)
/* Return new tdbField. */
{
struct tdbField *field;
lmAllocVar(lm, field);
field->name = lmCloneString(lm, name);
val = emptyForNull(skipLeadingSpaces(val));
field->val = lmCloneString(lm, val);
return field;
}

struct tdbRecord *tdbRecordReadOne(struct lineFile *lf, char *key, struct lm *lm)
/* Read next record from file. Returns NULL at end of file. */
{
struct tdbField *fieldList = NULL;
char *keyVal = NULL;
boolean override = FALSE;
struct dyString *dy = dyStringNew(0);

if (!raSkipLeadingEmptyLines(lf,dy))
    return NULL;
int startLineIx = lf->lineIx;

char *tag, *val;
while (raNextTagVal(lf, &tag, &val,dy))
    {
    trackDbUpdateOldTag(&tag, &val);
    struct tdbField *field = tdbFieldNew(tag, val, lm);
    if (sameString(field->name, key))
	{
	keyVal = lmCloneFirstWord(lm, field->val);
	if (endsWith(field->val, "override") && !sameString("override", field->val))
	    {
	    override = TRUE;
	    field->val = lmCloneString(lm, keyVal);
	    }
	}
    slAddHead(&fieldList, field);
    }

if (fieldList == NULL)
    return NULL;
slReverse(&fieldList);

struct tdbRecord *record;
lmAllocVar(lm, record);
record->fieldList = fieldList;
record->key = keyVal;
record->override = override;
record->posList = tdbFilePosNew(lm, lf->fileName, startLineIx, lf->lineIx, dy->string);
dyStringFree(&dy);
return record;
}

