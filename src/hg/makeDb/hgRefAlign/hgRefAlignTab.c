/*
 * Routines for reading file of tab column into refAlign objects./
 */

#include "hgRefAlignTab.h"
#include <stdio.h>
#include "common.h"
#include "errabort.h"
#include "refAlign.h"

#define MAX_COLS 14
#define MIN_COLS 13

static int parseInt(char* str, char* fname, int lineNum)
/* Convert a string to an integer, checking for error */
{
int num;
if (sscanf(str, "%d", &num) != 1)
    errAbort("invalid integer \"%s\" in %s at %d", str, fname, lineNum);

return num;
}

struct refAlign* parseTabRec(char* fname, int* lineNum, FILE* fh)
/* Read and parse the next record in a tab file.  Return NULL on
 * EOF.*/
{
char* line;
int numCols;
char* cols[MAX_COLS+1];
struct refAlign* refAlign;

/* read until we found a non-comment, no-blank line*/
while (TRUE)
    {
    line = readLine(fh);
    if (line == NULL)
        return NULL; /* EOF */
    (*lineNum)++;
    trimSpaces(line);
    if ((line[0] == '\0') || (line[0] == '#'))
        {
        /* Skip line */
        freez(&line);
        }
    else
        break;  /* got a line */
    }

/* Split string, allow for one extra so we can catch long lines */
numCols = chopString(line, "\t", cols, MAX_COLS+1);

if (!((MIN_COLS <= numCols) && (numCols <= MAX_COLS)))
    {
    errAbort("invalid number of columns in %s at %d", fname, *lineNum);
    }

/* Got it all now fill in the record, converting to [0, end) coordinators */
AllocVar(refAlign);
refAlign->next = NULL;
refAlign->chrom = cloneString(cols[0]);
refAlign->chromStart = parseInt(cols[1], fname, *lineNum);
refAlign->chromEnd = parseInt(cols[2], fname, *lineNum);
refAlign->name = cloneString(cols[3]);
refAlign->score = parseInt(cols[4], fname, *lineNum);
refAlign->matches = parseInt(cols[5], fname, *lineNum);
refAlign->misMatches = parseInt(cols[6], fname, *lineNum);
refAlign->hNumInsert = parseInt(cols[7], fname, *lineNum);
refAlign->hBaseInsert = parseInt(cols[8], fname, *lineNum);
refAlign->aNumInsert = parseInt(cols[9], fname, *lineNum);
refAlign->aBaseInsert = parseInt(cols[10], fname, *lineNum);
refAlign->humanSeq = cloneString(cols[11]);
refAlign->alignSeq = cloneString(cols[12]);
refAlign->attribs = cloneString(cols[13]);

freez(&line);
return refAlign;
}

struct refAlign* parseTabFile(char* fname)
/* Read and parse a file, return head of list */
{
struct refAlign* refAlignList = NULL;
int lineNum = 0;
FILE* fh;
struct refAlign* refAlign;

if (strcmp(fname, "-") == 0)
    fh = stdin;
else
    fh = mustOpen(fname, "r");

while ((refAlign = parseTabRec(fname, &lineNum, fh)) != NULL)
    {
    slAddHead(&refAlignList, refAlign);
    }
if (strcmp(fname, "-") != 0)
    fclose(fh);
if (refAlignList == NULL)
    errAbort("no rows read from %s", fname);
return refAlignList;
}

struct refAlign* parseTabFiles(int nfiles, char** fnames)
/* Read and parse multiple files, return head of list */
{
int fIdx;
struct refAlign* refAlignList = NULL;
for (fIdx = 0; fIdx < nfiles; fIdx++)
    {
    struct refAlign* newRefAlignList = parseTabFile(fnames[fIdx]);
    refAlignList = slCat(refAlignList, newRefAlignList);
    }
return refAlignList;
}
