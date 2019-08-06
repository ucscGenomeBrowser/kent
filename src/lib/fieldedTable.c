/* fieldedTable - a table composed of untyped strings in memory.  Includes names for each
 * field. This is a handy way of storing small-to-medium tab-separated files that begin
 * with a "#list of fields" line among other things. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "localmem.h"
#include "linefile.h"
#include "hash.h"
#include "fieldedTable.h"
#include "net.h"

struct fieldedTable *fieldedTableNew(char *name, char **fields, int fieldCount)
/* Create a new empty fieldedTable with given name, often a file name. */
{
struct fieldedTable *table;
AllocVar(table);
struct lm *lm = table->lm = lmInit(0);
table->name = lmCloneString(lm, name);
table->cursor = &table->rowList;
table->fieldCount = fieldCount;
int i;
char **row = lmAllocArray(lm, table->fields, fieldCount);
for (i=0; i<fieldCount; ++i)
    {
    row[i] = lmCloneString(lm, fields[i]);
    }
return table;
}

void fieldedTableFree(struct fieldedTable **pTable)
/* Free up memory resources associated with table. */
{
struct fieldedTable *table = *pTable;
if (table != NULL)
    {
    lmCleanup(&table->lm);
    freez(pTable);
    }
}


static struct fieldedRow *fieldedTableNewRow(struct fieldedTable *table,  
    char **row, int rowSize, int id)
/* Create a new row and populate it, but don't add it to table yet. */
{
/* Make sure we got right number of fields. */
if (table->fieldCount != rowSize)
    errAbort("%s starts with %d fields, but at line %d has %d fields instead",
	    table->name, table->fieldCount, id, rowSize);

/* Allocate field from local memory and start filling it in. */
struct lm *lm = table->lm;
struct fieldedRow *fr;
lmAllocVar(lm, fr);
lmAllocArray(lm, fr->row, rowSize);
fr->id = id;
int i;
for (i=0; i<rowSize; ++i)
    fr->row[i] = lmCloneString(lm, row[i]);

return fr;
}

struct fieldedRow *fieldedTableAdd(struct fieldedTable *table,  char **row, int rowSize, int id)
/* Create a new row and add it to table.  Return row. */
{
struct fieldedRow *fr = fieldedTableNewRow(table, row, rowSize, id);

/* Add it to end of list using cursor to avoid slReverse hassles. */
*(table->cursor) = fr;
table->cursor = &fr->next;
table->rowCount += 1;

return fr;
}

struct fieldedRow *fieldedTableAddHead(struct fieldedTable *table, char **row, int rowSize, int id)
/* Create a new row and add it to start of table.  Return row. */
{
if (table->rowCount == 0)
    {
    // Let fieldedTableAdd() handle the edges of the empty case
    return fieldedTableAdd(table, row, rowSize, id);
    }
struct fieldedRow *fr = fieldedTableNewRow(table, row, rowSize, id);
slAddHead(&table->rowList, fr);
table->rowCount += 1;
return fr;
}


int fieldedTableMaxColChars(struct fieldedTable *table, int colIx)
/* Calculate the maximum number of characters in a cell for a column */
{
if (colIx >= table->fieldCount)
    errAbort("fieldedTableMaxColChars on %d, but only have %d columns", colIx, table->fieldCount);
int max = strlen(table->fields[colIx]) + 1;
struct fieldedRow *fr;
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    char *val = fr->row[colIx];
    if (val != NULL)
        {
	int len = strlen(val);
	if (len > max)
	   max = len;
	}
    }
return max;
}

boolean fieldedTableColumnIsNumeric(struct fieldedTable *table, int fieldIx)
/* Return TRUE if field has numeric values wherever non-null */
{
struct fieldedRow *fr;
boolean anyVals = FALSE;
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    char *s = fr->row[fieldIx];
    if (s != NULL)
        {
	anyVals = TRUE;
	if (!isNumericString(s))
	    return FALSE;
	}
    }
return anyVals;
}

static int slPairCmpNumbers(const void *va, const void *vb)
/* Compare slPairs where name is interpreted as floating point number */
{
const struct slPair *a = *((struct slPair **)va);
const struct slPair *b = *((struct slPair **)vb);
double aVal = atof(a->name);
double bVal = atof(b->name);
double diff = aVal - bVal;
if (diff < 0)
    return -1;
else if (diff > 0)
    return 1;
else
    return 0;
}


void fieldedTableSortOnField(struct fieldedTable *table, char *field, boolean doReverse)
/* Sort on field.  Distinguishes between numerical and text fields appropriately.  */
{
/* Figure out field position */
int fieldIx = stringArrayIx(field, table->fields, table->fieldCount);
if (fieldIx < 0)
    fieldIx = 0;
boolean fieldIsNumeric = fieldedTableColumnIsNumeric(table, fieldIx);

/* Make up pair list in local memory which points to rows */
struct lm *lm = lmInit(0);
struct slPair *pairList=NULL, *pair;
struct fieldedRow *fr;
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    char *val = emptyForNull(fr->row[fieldIx]);
    lmAllocVar(lm, pair);
    pair->name = val;
    pair->val = fr;
    slAddHead(&pairList, pair);
    }
slReverse(&pairList);  

/* Sort this list. */
if (fieldIsNumeric)
    slSort(&pairList, slPairCmpNumbers);
else
    slSort(&pairList, slPairCmpCase);
if (doReverse)
    slReverse(&pairList);

/* Convert rowList to have same order. */
struct fieldedRow *newList = NULL;
for (pair = pairList; pair != NULL; pair = pair->next)
    {
    fr = pair->val;
    slAddHead(&newList, fr);
    }
slReverse(&newList);
table->rowList = newList;
lmCleanup(&lm);
}

struct fieldedTable *fieldedTableFromTabFile(char *fileName, char *reportFileName, 
    char *requiredFields[], int requiredCount)
/* Read table from tab-separated file with a #header line that defines the fields.  Ensures
 * all requiredFields (if any) are present.  The reportFileName is just used for error reporting and 
 * should be NULL for most purposes.  This is used by edwSubmit though which
 * first copies to a local file, and we want to report errors from the remote file. 
 * We do know the remote file exists at least, because we just copied it. */
{
/* Open file with fileName */
struct lineFile *lf = netLineFileOpen(fileName);

/* Substitute in reportFileName for error reporting */
if (reportFileName != NULL)
    {
    if (differentString(reportFileName, fileName))
        {
	freeMem(lf->fileName);
	lf->fileName = cloneString(reportFileName);
	}
    }
else
    {
    reportFileName = fileName;
    }

/* Get first line and turn it into field list. */
char *line;
if (!lineFileNext(lf, &line, NULL))
   errAbort("%s is empty", reportFileName);
boolean startsSharp = FALSE;
if (line[0] == '#')
   {
   line = skipLeadingSpaces(line+1);
   startsSharp = TRUE;
   }
   
int fieldCount = chopByChar(line, '\t', NULL, 0);
char *fields[fieldCount];
chopTabs(line, fields);

/* Make sure that all required fields are present. */
int i;
for (i = 0; i < requiredCount; ++i)
    {
    char *required = requiredFields[i];
    int ix = stringArrayIx(required, fields, fieldCount);
    if (ix < 0)
        errAbort("%s is missing required field '%s'", reportFileName, required);
    }

/* Create fieldedTable . */
struct fieldedTable *table = fieldedTableNew(reportFileName, fields, fieldCount);
table->startsSharp = startsSharp;
while (lineFileRowTab(lf, fields))
    {
    fieldedTableAdd(table, fields, fieldCount, lf->lineIx);
    }

/* Clean up and go home. */
lineFileClose(&lf);
return table;
}

void fieldedTableToTabFile(struct fieldedTable *table, char *fileName)
/* Write out a fielded table back to file */
{
FILE *f = mustOpen(fileName, "w");

/* Write out header row with optional leading # */
if (table->startsSharp)
    fputc('#', f);
int i;
fputs(table->fields[0], f);
for (i=1; i<table->fieldCount; ++i)
    {
    fputc('\t', f);
    fputs(table->fields[i], f);
    }
fputc('\n', f);

/* Write out rest. */
struct fieldedRow *fr;
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    fputs(fr->row[0], f);
    for (i=1; i<table->fieldCount; ++i)
	{
	fputc('\t', f);
	fputs(fr->row[i], f);
	}
    fputc('\n', f);
    }

carefulClose(&f);
}

int fieldedTableMustFindFieldIx(struct fieldedTable *table, char *field)
/* Find index of field in table's row.  Abort if field not found. */
{
int ix = stringArrayIx(field, table->fields, table->fieldCount);
if (ix < 0)
    errAbort("Field %s not found in table %s", field, table->name);
return ix;
}

struct hash *fieldedTableIndex(struct fieldedTable *table, char *field)
/* Return hash of fieldedRows keyed by values of given field */
{
int fieldIx = fieldedTableMustFindFieldIx(table, field);
struct hash *hash = hashNew(0);
struct fieldedRow *fr;
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    hashAdd(hash,fr->row[fieldIx], fr);
    }
return hash;
}

struct hash *fieldedTableUniqueIndex(struct fieldedTable *table, char *field)
/* Return hash of fieldedRows keyed by values of given field, which must be unique. */
{
int fieldIx = fieldedTableMustFindFieldIx(table, field);
struct hash *hash = hashNew(0);
struct fieldedRow *fr;
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    char *key = fr->row[fieldIx];
    if (hashLookup(hash, key))
        errAbort("%s duplicated in %s field of %s", key, field, table->name);
    hashAdd(hash,fr->row[fieldIx], fr);
    }
return hash;
}

