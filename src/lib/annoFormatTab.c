/* annoFormatTab -- collect fields from all inputs and print them out, tab-separated. */

#include "annoFormatTab.h"
#include "annoGratorQuery.h"
#include "dystring.h"

struct annoFormatTab
    {
    struct annoFormatter formatter;
    char *fileName;
    FILE *f;
    boolean needHeader;			// TRUE if we should print out the header
    struct annoRow *primaryRow;		// Single row from primary source
    struct slRef *gratorRowLists;	// Accumulated from each grator (0 or more annoRows each)
    };

static void printHeaderColumns(FILE *f, struct annoStreamer *source, boolean isFirst)
/* Print names of included columns from this source. */
{
struct annoColumn *col;
int i;
for (col = source->columns, i = 0;  col != NULL;  col = col->next, i++)
    {
    if (! col->included)
	continue;
    if (isFirst && i == 0)
	fputc('#', f);
    else
	fputc('\t', f);
    fputs(col->def->name, f);
    }
}

static void aftInitialize(struct annoFormatter *vSelf, struct annoGratorQuery *query)
/* Print header, regardless of whether we get any data after this. */
{
vSelf->query = query;
struct annoFormatTab *self = (struct annoFormatTab *)vSelf;
if (self->needHeader)
    {
    printHeaderColumns(self->f, query->primarySource, TRUE);
    struct annoStreamer *grator = (struct annoStreamer *)(query->integrators);
    for (;  grator != NULL;  grator = grator->next)
	printHeaderColumns(self->f, grator, FALSE);
    fputc('\n', self->f);
    self->needHeader = FALSE;
    }
}

static void aftCollect(struct annoFormatter *vSelf, struct annoStreamer *stub, struct annoRow *rows)
/* Gather columns from a single source's row(s). */
{
struct annoFormatTab *self = (struct annoFormatTab *)vSelf;
if (self->primaryRow == NULL)
    self->primaryRow = rows;
else
    slAddHead(&(self->gratorRowLists), slRefNew(rows));
}

static void aftDiscard(struct annoFormatter *vSelf)
/* Forget what we've collected so far, time to start over. */
{
struct annoFormatTab *self = (struct annoFormatTab *)vSelf;
self->primaryRow = NULL;
slFreeList(&(self->gratorRowLists));
return;
}

static void printColumns(FILE *f, struct annoStreamer *streamer, char **row, boolean isFirst)
/* Print columns in streamer's row (if NULL, print the right number of empty fields). */
{
struct annoColumn *col;
int i;
for (col = streamer->columns, i = 0;  col != NULL;  col = col->next, i++)
    {
    if (! col->included)
	continue;
    if (!isFirst || i > 0)
	fputc('\t', f);
    if (row != NULL)
	fputs(row[i], f);
    }
}

static void aftFormatOne(struct annoFormatter *vSelf)
/* Print out tab-separated columns that we have gathered in prior calls to aftCollect,
 * and start over fresh for the next line of output. */
{
struct annoFormatTab *self = (struct annoFormatTab *)vSelf;
slReverse(&(self->gratorRowLists));
int maxRows = 1;
int i;
struct slRef *grRef;
// How many rows did each grator give us, and what's the largest # of rows?
int numGrators = slCount(self->gratorRowLists);
if (numGrators > 1)
    {
    for (i = 0, grRef = self->gratorRowLists;  i < numGrators;  i++, grRef = grRef->next)
	{
	int gratorRowCount = slCount(grRef->val);
	if (gratorRowCount > maxRows)
	    maxRows = gratorRowCount;
	}
    }
// Print out enough rows to make sure that all grator rows are included.
for (i = 0;  i < maxRows;  i++)
    {
    printColumns(self->f, vSelf->query->primarySource, self->primaryRow->words, TRUE);
    struct annoStreamer *grator = (struct annoStreamer *)self->formatter.query->integrators;
    for (grRef = self->gratorRowLists;  grRef != NULL;  grRef = grRef->next,
	     grator = grator->next)
	{
	struct annoRow *gratorRow = slElementFromIx(grRef->val, i);
	char **row = (gratorRow == NULL) ? NULL : gratorRow->words;
	printColumns(self->f, grator, row, FALSE);
	}
    }
fputc('\n', self->f);
self->primaryRow = NULL;
slFreeList(&(self->gratorRowLists));
}

static void aftClose(struct annoFormatter **pVSelf)
/* Close file handle, free self. */
{
if (pVSelf == NULL)
    return;
struct annoFormatTab *self = *(struct annoFormatTab **)pVSelf;
freeMem(self->fileName);
carefulClose(&(self->f));
slFreeList(&(self->gratorRowLists));
annoFormatterFree(pVSelf);
}

struct annoFormatter *annoFormatTabNew(char *fileName)
/* Return a formatter that will write its tab-separated output to fileName. */
{
struct annoFormatTab *aft;
AllocVar(aft);
struct annoFormatter *formatter = &(aft->formatter);
formatter->getOptions = annoFormatterGetOptions;
formatter->setOptions = annoFormatterSetOptions;
formatter->initialize = aftInitialize;
formatter->collect = aftCollect;
formatter->discard = aftDiscard;
formatter->formatOne = aftFormatOne;
formatter->close = aftClose;
aft->fileName = cloneString(fileName);
aft->f = mustOpen(fileName, "w");
aft->needHeader = TRUE;
return (struct annoFormatter *)aft;
}
