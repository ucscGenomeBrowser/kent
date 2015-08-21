/* annoFormatTab -- collect fields from all inputs and print them out, tab-separated. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoFormatTab.h"
#include "annoGratorQuery.h"
#include "dystring.h"

struct annoFormatTab
    {
    struct annoFormatter formatter;     // External interface
    char *fileName;                     // Output file name, can be "stdout"
    FILE *f;                            // Output file handle
    struct hash *columnVis;             // Hash of columns that have been explicitly selected
                                        // or deselected by user.
    boolean needHeader;			// TRUE if we should print out the header
    };

static void makeFullColumnName(char *fullName, size_t size, char *sourceName, char *colName)
/* If sourceName is non-empty, make fullName sourceName.colName, otherwise just colName. */
{
if (isNotEmpty(sourceName))
    safef(fullName, size, "%s.%s", sourceName, colName);
else
    safecpy(fullName, size, colName);
}

void annoFormatTabSetColumnVis(struct annoFormatter *vSelf, char *sourceName, char *colName,
                               boolean enabled)
/* Explicitly include or exclude column in output.  sourceName must be the same
 * as the corresponding annoStreamer source's name. */
{
struct annoFormatTab *self = (struct annoFormatTab *)vSelf;
if (! self->columnVis)
    self->columnVis = hashNew(0);
char fullName[PATH_LEN];
makeFullColumnName(fullName, sizeof(fullName), sourceName, colName);
hashAddInt(self->columnVis, fullName, enabled);
}

static boolean columnIsIncluded(struct annoFormatTab *self, char *sourceName, char *colName)
// Return TRUE if column has not been explicitly deselected.
{
if (self->columnVis)
    {
    char fullName[PATH_LEN];
    makeFullColumnName(fullName, sizeof(fullName), sourceName, colName);
    int vis = hashIntValDefault(self->columnVis, fullName, 1);
    if (vis == 0)
        return FALSE;
    }
return TRUE;
}

static void printHeaderColumns(struct annoFormatTab *self, struct annoStreamer *source,
                               boolean *pIsFirst)
/* Print names of included columns from this source. */
{
FILE *f = self->f;
char *sourceName = source->name;
boolean isFirst = (pIsFirst && *pIsFirst);
struct asColumn *col;
for (col = source->asObj->columnList;  col != NULL;  col = col->next)
    {
    if (columnIsIncluded(self, sourceName, col->name))
        {
        if (isFirst)
            isFirst = FALSE;
        else
            fputc('\t', f);
        char fullName[PATH_LEN];
        makeFullColumnName(fullName, sizeof(fullName), sourceName, col->name);
        fputs(fullName, f);
        }
    }
if (pIsFirst != NULL)
    *pIsFirst = isFirst;
}

static void aftInitialize(struct annoFormatter *vSelf, struct annoStreamer *primary,
			  struct annoStreamer *integrators)
/* Print header, regardless of whether we get any data after this. */
{
struct annoFormatTab *self = (struct annoFormatTab *)vSelf;
if (self->needHeader)
    {
    char *primaryHeader = primary->getHeader(primary);
    boolean isFirst = TRUE;
    if (isNotEmpty(primaryHeader))
	fprintf(self->f, "# Header from primary input:\n%s", primaryHeader);
    fputc('#', self->f);
    printHeaderColumns(self, primary, &isFirst);
    struct annoStreamer *grator;
    for (grator = integrators;  grator != NULL;  grator = grator->next)
	printHeaderColumns(self, grator, &isFirst);
    fputc('\n', self->f);
    self->needHeader = FALSE;
    }
}

static char **bed4WordsFromAnnoRow(struct annoRow *row, char *fourth)
/* Return an array of 4 words with row's chrom, chromStart, and chromEnd, and cloned fourth. */
{
char **words;
AllocArray(words, 4);
words[0] = cloneString(row->chrom);
char buf[PATH_LEN];
safef(buf, sizeof(buf), "%u", row->start);
words[1] = cloneString(buf);
safef(buf, sizeof(buf), "%u", row->end);
words[2] = cloneString(buf);
words[3] = cloneString(fourth);
return words;
}

static char **wordsFromWigRow(struct annoRow *row, enum annoRowType rowType)
/* Return chrom, chromStart, chromEnd and a string containing comma-sep per-base wiggle values
 * if rowType is arWigVec, or one value if rowType is arWigSingle. */
{
struct dyString *dy = NULL;
if (rowType == arWigVec)
    {
    float *vector = row->data;
    int len = row->end - row->start;
    dy = dyStringNew(10*len);
    int i;
    for (i = 0;  i < len;  i++)
        {
        if (i > 0)
            dyStringAppendC(dy, ',');
        dyStringPrintf(dy, "%g", vector[i]);
        }
    }
else if (rowType == arWigSingle)
    {
    double *pValue = row->data;
    dy = dyStringCreate("%lf", *pValue);
    }
else
    errAbort("wordsFromWigRow: invalid rowType %d", rowType);
char **words = bed4WordsFromAnnoRow(row, dy->string);
dyStringFree(&dy);
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
if (source->rowType == arWords)
    words = row->data;
else if (source->rowType == arWigVec || source->rowType == arWigSingle)
    {
    words = wordsFromWigRow(row, source->rowType);
    if (words != NULL)
        freeWhenDone = TRUE;
    }
else
    errAbort("annoFormatTab: unrecognized row type %d from source %s",
	     source->rowType, source->name);
if (retFreeWhenDone != NULL)
    *retFreeWhenDone = freeWhenDone;
return words;
}

static void printColumns(struct annoFormatTab *self, struct annoStreamer *streamer,
			 struct annoRow *row, boolean isFirst)
/* Print columns in streamer's row (if NULL, print the right number of empty fields). */
{
FILE *f = self->f;
char *sourceName = streamer->name;
boolean freeWhenDone = FALSE;
char **words = wordsFromRow(row, streamer, &freeWhenDone);
struct asColumn *col;
int i;
for (col = streamer->asObj->columnList, i = 0;  col != NULL;  col = col->next, i++)
    {
    if (columnIsIncluded(self, sourceName, col->name))
        {
        if (isFirst)
            isFirst = FALSE;
        else
            fputc('\t', f);
        if (words != NULL)
            fputs((words[i] ? words[i] : ""), f);
        }
    }
int wordCount = i;
if (freeWhenDone)
    {
    for (i = 0;  i < wordCount;  i++)
        freeMem(words[i]);
    freeMem(words);
    }
}

static void aftComment(struct annoFormatter *fSelf, char *content)
/* Print out a comment line. */
{
if (strchr(content, '\n'))
    errAbort("aftComment: no multi-line input");
struct annoFormatTab *self = (struct annoFormatTab *)fSelf;
fprintf(self->f, "# %s\n", content);
}

static void aftFormatOne(struct annoFormatter *vSelf, struct annoStreamRows *primaryData,
			 struct annoStreamRows *gratorData, int gratorCount)
/* Print out tab-separated columns that we have gathered in prior calls to aftCollect,
 * and start over fresh for the next line of output. */
{
struct annoFormatTab *self = (struct annoFormatTab *)vSelf;
// Got one row from primary; what's the largest # of rows from any grator?
int maxRows = 1;
int iG;
for (iG = 0;  iG < gratorCount;  iG++)
    {
    int gratorRowCount = slCount(gratorData[iG].rowList);
    if (gratorRowCount > maxRows)
	maxRows = gratorRowCount;
    }
// Print out enough rows to make sure that all grator rows are included.
int iR;
for (iR = 0;  iR < maxRows;  iR++)
    {
    printColumns(self, primaryData->streamer, primaryData->rowList, TRUE);
    for (iG = 0;  iG < gratorCount;  iG++)
	{
	struct annoRow *gratorRow = slElementFromIx(gratorData[iG].rowList, iR);
	printColumns(self, gratorData[iG].streamer, gratorRow, FALSE);
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
formatter->comment = aftComment;
formatter->formatOne = aftFormatOne;
formatter->close = aftClose;
aft->fileName = cloneString(fileName);
aft->f = mustOpen(fileName, "w");
aft->needHeader = TRUE;
return (struct annoFormatter *)aft;
}
