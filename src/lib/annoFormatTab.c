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
    boolean **columnFlags;              // 2D array[sources][columns] of flags for whether each
                                        // column of each source is to be included in output.
    int *columnFlagLengths;             // array[sources] of column counts (lengths of subarrays),
                                        // for bounds-checking
    int sourceCount;                    // Number of sources (primary + grators)
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

static boolean columnNameIsIncluded(struct annoFormatTab *self, char *sourceName, char *colName)
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

static void addColumnFlagsForSource(struct annoFormatTab *self, struct annoStreamer *streamer)
/* Make an array of flags for whether to include each column from streamer. */
{
uint sourceIx = self->sourceCount;
int colCount = slCount(streamer->asObj->columnList);
AllocArray(self->columnFlags[sourceIx], colCount);
self->columnFlagLengths[sourceIx] = colCount;
struct asColumn *col;
uint colIx;
for (colIx = 0, col = streamer->asObj->columnList;  col != NULL;  col = col->next, colIx++)
    self->columnFlags[sourceIx][colIx] = columnNameIsIncluded(self, streamer->name, col->name);
self->sourceCount++;
}

static void makeColumnFlags(struct annoFormatTab *self, struct annoStreamer *primary,
                            struct annoStreamer *integrators)
/* Build arrays of flags for whether each column from each source is to be included
 * in the output.  */
{
int sourceCount = 1 + slCount(integrators);
AllocArray(self->columnFlags, sourceCount);
AllocArray(self->columnFlagLengths, sourceCount);
// self->sourceCount is incremented by each call to addColumnFlags
self->sourceCount = 0;
addColumnFlagsForSource(self, primary);
struct annoStreamer *grator;
for (grator = integrators;  grator != NULL;  grator = grator->next)
    addColumnFlagsForSource(self, grator);
assert(self->sourceCount == sourceCount);
}

INLINE boolean columnIsIncluded(struct annoFormatTab *self, uint sourceIx, uint colIx)
/* Look up flag for whether this source's column is included in output. */
{
if (sourceIx >= self->sourceCount)
    errAbort("annoFormatTab: illegal sourceIx %d (count is %d)", sourceIx, self->sourceCount);
if (colIx >= self->columnFlagLengths[sourceIx])
    errAbort("annoFormatTab: illegal colIx %d (lengths[%d] is %d) -- out-of-order sources?",
             colIx, sourceIx, self->columnFlagLengths[sourceIx]);
return self->columnFlags[sourceIx][colIx];
}

static void printHeaderColumns(struct annoFormatTab *self, struct annoStreamer *source,
                               uint sourceIx, boolean *pIsFirst)
/* Print names of included columns from this source. */
{
FILE *f = self->f;
char *sourceName = source->name;
boolean isFirst = (pIsFirst && *pIsFirst);
uint colIx;
struct asColumn *col;
for (colIx = 0, col = source->asObj->columnList;  col != NULL;  col = col->next, colIx++)
    {
    if (columnIsIncluded(self, sourceIx, colIx))
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
/* Print header, regardless of whether we get any data after this.
 * Also build arrays of flags for whether each column from each source is to be included
 * in the output.  */
{
struct annoFormatTab *self = (struct annoFormatTab *)vSelf;
makeColumnFlags(self, primary, integrators);
if (self->needHeader)
    {
    char *primaryHeader = primary->getHeader(primary);
    boolean isFirst = TRUE;
    char *newlineAtEnd = (lastChar(primaryHeader) == '\n') ? "" : "\n";
    if (isNotEmpty(primaryHeader))
	fprintf(self->f, "# Header from primary input:\n%s%s", primaryHeader, newlineAtEnd);
    fputc('#', self->f);
    printHeaderColumns(self, primary, 0, &isFirst);
    uint sourceIx;
    struct annoStreamer *grator;
    for (sourceIx = 1, grator = integrators;  grator != NULL;  grator = grator->next, sourceIx++)
	printHeaderColumns(self, grator, sourceIx, &isFirst);
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
			 uint sourceIx, struct annoRow *row, boolean isFirst)
/* Print columns in streamer's row (if NULL, print the right number of empty fields). */
{
FILE *f = self->f;
boolean freeWhenDone = FALSE;
char **words = wordsFromRow(row, streamer, &freeWhenDone);
struct asColumn *col;
uint colIx;
for (col = streamer->asObj->columnList, colIx = 0;  col != NULL;  col = col->next, colIx++)
    {
    if (columnIsIncluded(self, sourceIx, colIx))
        {
        if (isFirst)
            isFirst = FALSE;
        else
            fputc('\t', f);
        if (words != NULL)
            fputs((words[colIx] ? words[colIx] : ""), f);
        }
    }
int wordCount = colIx;
if (freeWhenDone)
    {
    for (colIx = 0;  colIx < wordCount;  colIx++)
        freeMem(words[colIx]);
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
uint iG;
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
    printColumns(self, primaryData->streamer, 0, primaryData->rowList, TRUE);
    for (iG = 0;  iG < gratorCount;  iG++)
	{
	struct annoRow *gratorRow = slElementFromIx(gratorData[iG].rowList, iR);
	printColumns(self, gratorData[iG].streamer, 1+iG, gratorRow, FALSE);
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
