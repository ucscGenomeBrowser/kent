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
    };

static void printHeaderColumns(FILE *f, struct annoStreamer *source, boolean isFirst)
/* Print names of included columns from this source. */
{
if (source->rowType == arWig)
    {
    // Fudge in the row's chrom, start, end as output columns even though they're not in autoSql
    if (isFirst)
	{
	fputs("#chrom", f);
	isFirst = FALSE;
	}
    fputs("\tstart\tend", f);
    }
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
    struct annoStreamer *primary = query->primarySource;
    char *primaryHeader = primary->getHeader(primary);
    if (isNotEmpty(primaryHeader))
	printf("# Header from primary input:\n%s", primaryHeader);
    printHeaderColumns(self->f, primary, TRUE);
    struct annoStreamer *grator = (struct annoStreamer *)(query->integrators);
    for (;  grator != NULL;  grator = grator->next)
	printHeaderColumns(self->f, grator, FALSE);
    fputc('\n', self->f);
    self->needHeader = FALSE;
    }
}

static double wigRowAvg(struct annoRow *row)
/* Return the average value of floats in row->data. */
{
float *vector = row->data;
int len = row->end - row->start;
double sum = 0.0;
int i;
for (i = 0;  i < len;  i++)
    sum += vector[i];
return sum / (double)len;
}

static char **wordsFromWigRowAvg(struct annoRow *row)
/* Return an array of strings with a single string containing the average of values in row. */
{
double avg = wigRowAvg(row);
char **words;
AllocArray(words, 1);
char avgStr[32];
safef(avgStr, sizeof(avgStr), "%lf", avg);
words[0] = cloneString(avgStr);
return words;
}

static char **wordsFromWigRowVals(struct annoRow *row)
/* Return an array of strings with a single string containing comma-sep per-base wiggle values. */
{
float *vector = row->data;
int len = row->end - row->start;
struct dyString *dy = dyStringNew(10*len);
int i;
for (i = 0;  i < len;  i++)
    dyStringPrintf(dy, "%f,", vector[i]);
char **words;
AllocArray(words, 1);
words[0] = dyStringCannibalize(&dy);
return words;
}

static char **wordsFromRow(struct annoRow *row, struct annoStreamer *source,
			   boolean *retFreeWhenDone)
/* If source->rowType is arWords, return its words.  Otherwise translate row->data into words. */
{
if (row == NULL)
    return NULL;
boolean freeWhenDone = FALSE;
char **words = NULL;
if (source->rowType == arWords || source->rowType == arVcf)
    words = row->data;
else if (source->rowType == arWig)
    {
    freeWhenDone = TRUE;
    //#*** config options: avg? more stats? list of values?
    boolean doAvg = FALSE;
    if (doAvg)
	words = wordsFromWigRowAvg(row);
    else
	words = wordsFromWigRowVals(row);
    }
else
    errAbort("annoFormatTab: unrecognized row type %d", source->rowType);
if (retFreeWhenDone != NULL)
    *retFreeWhenDone = freeWhenDone;
return words;
}

static void printColumns(FILE *f, struct annoStreamer *streamer, struct annoRow *row,
			 boolean isFirst)
/* Print columns in streamer's row (if NULL, print the right number of empty fields). */
{
boolean freeWhenDone = FALSE;
char **words = wordsFromRow(row, streamer, &freeWhenDone);
if (streamer->rowType == arWig)
    {
    // Fudge in the row's chrom, start, end as output columns even though they're not in autoSql
    if (isFirst)
	{
	if (row != NULL)
	    fputs(row->chrom, f);
	isFirst = FALSE;
	}
    if (row != NULL)
	fprintf(f, "\t%u\t%u", row->start, row->end);
    else
	fputs("\t\t", f);
    }
struct annoColumn *col;
int i;
for (col = streamer->columns, i = 0;  col != NULL;  col = col->next, i++)
    {
    if (! col->included)
	continue;
    if (!isFirst || i > 0)
	fputc('\t', f);
    if (words != NULL)
	fputs(words[i], f);
    }
if (freeWhenDone)
    {
    freeMem(words[0]);
    freeMem(words);
    }
}

static void aftFormatOne(struct annoFormatter *vSelf, struct annoRow *primaryRow,
			 struct slRef *gratorRowList)
/* Print out tab-separated columns that we have gathered in prior calls to aftCollect,
 * and start over fresh for the next line of output. */
{
struct annoFormatTab *self = (struct annoFormatTab *)vSelf;
// How many rows did each grator give us, and what's the largest # of rows?
int maxRows = 1;
int i;
struct slRef *grRef;
int numGrators = slCount(gratorRowList);
for (i = 0, grRef = gratorRowList;  i < numGrators;  i++, grRef = grRef->next)
    {
    int gratorRowCount = slCount(grRef->val);
    if (gratorRowCount > maxRows)
	maxRows = gratorRowCount;
    }
// Print out enough rows to make sure that all grator rows are included.
struct annoStreamer *primarySource = vSelf->query->primarySource;
for (i = 0;  i < maxRows;  i++)
    {
    printColumns(self->f, primarySource, primaryRow, TRUE);
    struct annoStreamer *grator = (struct annoStreamer *)self->formatter.query->integrators;
    for (grRef = gratorRowList;  grRef != NULL;  grRef = grRef->next, grator = grator->next)
	{
	struct annoRow *gratorRow = slElementFromIx(grRef->val, i);
	printColumns(self->f, grator, gratorRow, FALSE);
	}
    fputc('\n', self->f);
    }
}

static void aftClose(struct annoFormatter **pVSelf)
/* Close file handle, free self. */
{
if (pVSelf == NULL)
    return;
struct annoFormatTab *self = *(struct annoFormatTab **)pVSelf;
freeMem(self->fileName);
carefulClose(&(self->f));
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
formatter->formatOne = aftFormatOne;
formatter->close = aftClose;
aft->fileName = cloneString(fileName);
aft->f = mustOpen(fileName, "w");
aft->needHeader = TRUE;
return (struct annoFormatter *)aft;
}
