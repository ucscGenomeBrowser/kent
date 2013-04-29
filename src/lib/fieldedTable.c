/* fieldedTable - a table composed of untyped strings in memory.  Includes names for each
 * field. This is a handy way of storing small-to-medium tab-separated files that begin
 * with a "#list of fields" line among other things. */

#include "common.h"
#include "localmem.h"
#include "linefile.h"
#include "fieldedTable.h"

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

struct fieldedRow *fieldedTableAdd(struct fieldedTable *table,  char **row, int rowSize, int id)
/* Create a new row and add it to table.  Return row. */
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

/* Add it to end of list using cursor to avoid slReverse hassles. */
*(table->cursor) = fr;
table->cursor = &fr->next;

return fr;
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
struct lineFile *lf = lineFileOpen(fileName, TRUE);

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
if (line[0] != '#')
   errAbort("%s must start with '#' and field names on first line", reportFileName);
line = skipLeadingSpaces(line+1);
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
while (lineFileRowTab(lf, fields))
    {
    fieldedTableAdd(table, fields, fieldCount, lf->lineIx);
    }

/* Clean up and go home. */
lineFileClose(&lf);
return table;
}

