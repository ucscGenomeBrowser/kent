/*
 * Routines for reading Webb Miller format alignments.
 *
 * webb format:
 *  Chr22 13005062-13005547 Chr22:0 attrib
 *  TCAGTGGAGCTGGC--TCTGACGATGCCTCTCTTT
 *  TTAGTGTAGTCTGCAGCCA--GGTGGCCACCTTGC
 *  <blank line>
 * Where attrib is a optional, comma-seperate list of symbolic attributes.
 */

#include "hgRefAlignWebb.h"
#include <stdio.h>
#include "common.h"
#include "errabort.h"
#include "refAlign.h"

static char* readLine(FILE* fh)
/* Read a line of any size into dynamic memory, return null on EOF */
{
int bufCapacity = 256;
int bufSize = 0;
char* buf = needMem(bufCapacity);
int ch;

/* loop until EOF of EOLN */
while (((ch = getc(fh)) != EOF) && (ch != '\n'))
    {
    /* expand if almost full, always keep one extra char for
     * zero termination */
    if (bufSize >= bufCapacity-2)
        {
        bufCapacity *= 2;
        buf = realloc(buf, bufCapacity);
        if (buf == NULL)
            {
            errAbort("Out of memory - request size %d bytes", bufCapacity);
            }
        }
    buf[bufSize++] = ch;
    }

/* only return EOF if no data was read */
if ((ch == EOF) && (bufSize == 0))
    {
    freeMem(buf);
    return NULL;
    }
buf[bufSize] = '\0';
return buf;
}

static void cleanValidateSeq(char* seq, char* fname, int lineNum)
/* cleanup and validate a sequence that was read */
{
char* ch;
trimSpaces(seq);
touppers(seq);
/* check for illegal characters */
for (ch = seq; *ch != '\0'; ch++)
    if (!(((*ch >= 'A') && (*ch <= 'Z')) || (*ch == '-')))
        errAbort("invalid seq character `%c' in %s at %d", *ch,
                 fname, lineNum);
}

struct refAlign* parseWebbRec(char* fname, int* lineNum, FILE* fh)
/* Read and parse the next record in a Webb file.  Return NULL on
 * EOF.*/
{
#define WEBB_MAX_HEADER 512
char buf[WEBB_MAX_HEADER];
char chrom[WEBB_MAX_HEADER]; /* max size of line to prevent mem corrupt */
unsigned chromStart, chromEnd;
char name[WEBB_MAX_HEADER];
char attribs[WEBB_MAX_HEADER];
char* humanSeq = NULL;
char* alignSeq = NULL;
struct refAlign* refAlign;
int numFields;

/* Skip blank lines */
while (fgets(buf, WEBB_MAX_HEADER, fh) != NULL)
    {
    (*lineNum)++;
    eraseTrailingSpaces(buf);
    if (buf[0] != '\0')
        break; /* non-empty line */
    }
if (feof(fh))
    return NULL;

/* Parse the header line */
attribs[0] = '\0'; /* optional */
numFields = sscanf(buf, "%s %u-%u %s %s", chrom, &chromStart, &chromEnd, name,
                   attribs);
if (!((4 <= numFields) && (numFields <= 5)))
    {
    errAbort("invalid record header in %s at %d", fname, *lineNum);
    }

/* expect one-based coordinates */
if ((chromStart <= 0) || (chromEnd <= 0) || (chromStart > chromEnd))
    {
    errAbort("invalid chrom cooordinates in %s at %d", fname, *lineNum);
    }
tolowers(chrom);

humanSeq = readLine(fh);
(*lineNum)++;
if (humanSeq != NULL)
    cleanValidateSeq(humanSeq, fname, *lineNum);

alignSeq = readLine(fh);
(*lineNum)++;
if (alignSeq == NULL)
    errAbort("premature EOF in %s", fname);
cleanValidateSeq(alignSeq, fname, *lineNum);

if (strlen(humanSeq) != strlen(alignSeq))
    {
    errAbort("human and aligned sequences are not the same length for %s in %s at %d",
             name, fname, *lineNum);
    }
#if 0
/*FIXME: disable check, we are making tmp tracks with no data */
if (strlen(humanSeq) == 0)
    {
    errAbort("zero -length alignment for %s in %s at %d",
             name, fname, *lineNum);
    }
#endif

/* Got it all now fill in the record, converting to [0, end) coordinators */
AllocVar(refAlign);
refAlign->next = NULL;
refAlign->chrom = cloneString(chrom);
refAlign->chromStart = chromStart-1;
refAlign->chromEnd = chromEnd;
refAlign->name = cloneString(name);
refAlign->humanSeq = humanSeq;
refAlign->alignSeq = alignSeq;
refAlign->attribs = cloneString(attribs);
return refAlign;
}

struct refAlign* parseWebbFile(char* fname)
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

while ((refAlign = parseWebbRec(fname, &lineNum, fh)) != NULL)
    {
    slAddHead(&refAlignList, refAlign);
    }
if (strcmp(fname, "-") != 0)
    fclose(fh);
if (refAlignList == NULL)
    errAbort("no rows read from %s", fname);
return refAlignList;
}

struct refAlign* parseWebbFiles(int nfiles, char** fnames)
/* Read and parse multiple files, return head of list */
{
int fIdx;
struct refAlign* refAlignList = NULL;
for (fIdx = 0; fIdx < nfiles; fIdx++)
    {
    struct refAlign* newRefAlignList = parseWebbFile(fnames[fIdx]);
    refAlignList = slCat(refAlignList, newRefAlignList);
    }
return refAlignList;
}
