#include "common.h"
#include "localmem.h"
#include "linefile.h"
#include "raRecord.h"

struct raField *raRecordField(struct raRecord *ra, char *fieldName)
/* Return named field if it exists, otherwise NULL */
{
struct raField *field;
for (field = ra->fieldList; field != NULL; field = field->next)
    {
    if (sameString(field->name, fieldName))
        return field;
    }
return NULL;
}

struct raField *raFieldFromLine(char *line, struct lm *lm)
/* Parse out line and convert it to a raField.  Will return NULL on empty lines. 
 * Will insert some zeroes into the input line as well. */
{
char *word = nextWord(&line);
if (word == NULL)
    return NULL;
struct raField *field;
lmAllocVar(lm, field);
field->name = lmCloneString(lm, word);
char *val = emptyForNull(skipLeadingSpaces(line));
field->val = lmCloneString(lm, val);
return field;
}

struct raRecord *raRecordReadOne(struct lineFile *lf, struct lm *lm)
/* Read next record from file. Returns NULL at end of file. */
{
struct raField *field, *fieldList = NULL;
char *line;

/* Read first line and start fieldList on it. */
if (!lineFileNextReal(lf, &line))
    return NULL;
fieldList = raFieldFromLine(line, lm);

/* Keep going until get a blank line. */
for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
        break;
    field = raFieldFromLine(line, lm);
    if (field == NULL)
        break;
    slAddHead(&fieldList, field);
    }

slReverse(&fieldList);
struct raRecord *record;
lmAllocVar(lm, record);
record->fieldList = fieldList;
return record;
}

