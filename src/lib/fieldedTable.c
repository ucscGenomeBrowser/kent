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

struct fieldedTable *fieldedTableFromTabFile(char *url, char *requiredFields[], int requiredCount)
/* Read table from tab-separated file with a #header line that defines the fields.  Ensures
 * all requiredFields (if any) are present. */
{
struct lineFile *lf = lineFileOpen(url, TRUE);
char *line;

/* Get first line and turn it into field list. */
if (!lineFileNext(lf, &line, NULL))
   errAbort("%s is empty", url);
if (line[0] != '#')
   errAbort("%s must start with '#' and field names on first line", url);
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
        errAbort("%s is missing required field '%s'", url, required);
    }

/* Create fieldedTable . */
struct fieldedTable *table = fieldedTableNew(url, fields, fieldCount);
while (lineFileRowTab(lf, fields))
    {
    fieldedTableAdd(table, fields, fieldCount, lf->lineIx);
    }

/* Clean up and go home. */
lineFileClose(&lf);
return table;
}

