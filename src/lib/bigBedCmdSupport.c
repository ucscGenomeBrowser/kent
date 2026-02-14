/* bigBedCmdSupport - functions to support writing bigBed related commands. */

/* Copyright (C) 2022 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "bigBedCmdSupport.h"
#include "basicBed.h"
#include "options.h"
#include "hash.h"

static void outputHeader(struct bbiFile *bbi, FILE *f, char startChar)
/* output a header from the autoSql in the file, startChar is '#' or 0 to omit  */
{
char *asText = bigBedAutoSqlText(bbi);
if (asText == NULL)
    errAbort("bigBed files does not contain an autoSql schema");
struct asObject *asObj = asParseText(asText);
char sep = startChar;
for (struct asColumn *asCol = asObj->columnList; asCol != NULL; asCol = asCol->next)
    {
    if (sep != '\0')
        fputc(sep, f);
    fputs(asCol->name, f);
    sep = '\t';
    }
fputc('\n', f);
}

void bigBedCmdOutputHeader(struct bbiFile *bbi, FILE *f)
/* output a autoSql-style header from the autoSql in the file */
{
outputHeader(bbi, f, '#');
}

void bigBedCmdOutputTsvHeader(struct bbiFile *bbi, FILE *f)
/* output a TSV-style header from the autoSql in the file */
{
outputHeader(bbi, f, '\0');
}

struct hash *makeChromHash(struct bbiChromInfo *chromList)
// make a fast searchable hash from chromList 
{

struct hash *chromHash = hashNew(10);
struct bbiChromInfo *chrom;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    hashAdd(chromHash, cloneString(chrom->name), NULL);
    }
return chromHash;
}


static struct bed *bed3FromPositionString(char *line)
/* Read positions line and return bed 3. */
{
char *chrom;
int start, end;
struct bed *bed = NULL;

/* Convert input to bed record */
char *val = cloneString(line);
mustParseRange(val, &chrom, &start, &end);  // does not subtract 1
 verbose(1,"bed3FromPositionString after mustParseRange chrom=%s\n", chrom);

// used to use parseRegion which checks for overflow,
// but only catches start < 1 because of integer overflow.
if (start < 1)
    errAbort("invalid range, start=%d < 1, but first base is 1 or more", start);
--start;
if (start > end)
    errAbort("invalid range, (start - 1)=%d > end=%d", start, end);
AllocVar(bed);
bed->chrom=cloneString(chrom);
bed->chromStart=start;
bed->chromEnd=end;
freez(&val);
return bed;
}


struct bed *bed3FromPositions(char *fileName)
/* Read positions file and retrun bed 3 list. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct bed *bed = NULL, *bedList = NULL;
char *line;

/* Convert input to bed file */
while (lineFileNextReal(lf, &line))
    {
    bed = bed3FromPositionString(line);
    }
slAddHead(&bedList, bed);
lineFileClose(&lf);
return bedList;
}

struct bed *bedLoad3Plus(char *fileName)
/* Determines how many fields are in a bedFile and load all beds from
 * a tab-separated file.  Dispose of this with bedFreeList(). 
 * Small change by Michael to require only 3 or more fields. Meaning we will accept bed3 
 */
{
struct bed *list = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *row[bedKnownFields];

while (lineFileNextReal(lf, &line))
    {
    int numFields = chopByWhite(line, row, ArraySize(row));
    if (numFields < 3)
	errAbort("file %s doesn't appear to be in bed format. At least 3 fields required, got %d", fileName, numFields);
    slAddHead(&list, bedLoadN(row, numFields));
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct bed *bedLoad3FromRangeOption(struct slName *ranges)
/* Determines how many fields are in a bedFile and load all beds from
 * a tab-separated file.  Dispose of this with bedFreeList(). 
 * Small change by Michael to require only 3 or more fields. Meaning we will accept bed3 
 */
{
struct slName *range = NULL;
struct bed *bed, *list = NULL;
char *row[bedKnownFields];

for(range=ranges; range; range=range->next)
    {
    if (strchr(range->name, ':'))
        {
        bed = bed3FromPositionString(range->name);
        }
    else
        {
	char *val=cloneString(range->name);
	int numFields = chopByWhite(val, row, ArraySize(row));
	if (numFields != 3)
	    errAbort("range %s doesn't appear to be in bed format. 3 fields required, got %d", 
		range->name, numFields);
	bed = bedLoadN(row, numFields);
	freez(&val);
        }
    slAddHead(&list, bed);
    }
slReverse(&list);
return list;
}


void genericBigToNonBigFromBed(struct bbiFile *bbi, struct hash *chromHash, char *bedFileName, FILE *outFile, 
 void (*processChromChunk)(struct bbiFile *bbi, char *chrom, int start, int end, char *bedName, FILE *f)
)
/* Read list of ranges from bed file chrom start end.
 * Automatically sort them by chrom, start. If bed4 pass bed->name too. */
{
struct bed *bed, *bedList = bedLoad3Plus(bedFileName);
slSort(&bedList, bedCmp);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (chromHash && !hashLookup(chromHash, bed->chrom))
	errAbort("invalid chrom = %s not found", bed->chrom);
    if (bed->chromStart > bed->chromEnd)
	errAbort("invalid range, start=%u > end=%u", bed->chromStart, bed->chromEnd);
    processChromChunk(bbi, bed->chrom, bed->chromStart, bed->chromEnd, bed->name, outFile);
    }
bedFreeList(&bedList);
}

void genericBigToNonBigFromRange(struct bbiFile *bbi, struct hash *chromHash, FILE *outFile, struct slName *ranges,
 void (*processChromChunk)(struct bbiFile *bbi, char *chrom, int start, int end, char *bedName, FILE *f)
)
/* Read list of ranges as chrom start end or chrom:start-end.
 * Automatically sort them by chrom, start. */
{
struct bed *bed, *bedList = bedLoad3FromRangeOption(ranges);
slSort(&bedList, bedCmp);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (chromHash && !hashLookup(chromHash, bed->chrom))
	errAbort("invalid chrom = %s not found", bed->chrom);
    if (bed->chromStart > bed->chromEnd)
	errAbort("invalid range, start=%u > end=%u", bed->chromStart, bed->chromEnd);
    processChromChunk(bbi, bed->chrom, bed->chromStart, bed->chromEnd, bed->name, outFile);
    }
bedFreeList(&bedList);
}


void genericBigToNonBigFromPos(struct bbiFile *bbi, struct hash *chromHash, char *posFileName, FILE *outFile, 
 void (*processChromChunk)(struct bbiFile *bbi, char *chrom, int start, int end, char *bedName, FILE *f)
)
{
/* Read  positions from file (chrom:start-end). starts are 1-based,
 * but after conversion to bed3 list, they are 0-based. 
 * Automatically sort them by chrom, start */
struct bed *bed, *bedList = bed3FromPositions(posFileName);

slSort(&bedList, bedCmp);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (chromHash && !hashLookup(chromHash, bed->chrom))
	errAbort("invalid chrom = %s not found", bed->chrom);
    if (bed->chromStart > bed->chromEnd)
	errAbort("invalid range, start=%u > end=%u", bed->chromStart, bed->chromEnd);
    processChromChunk(bbi, bed->chrom, bed->chromStart, bed->chromEnd, NULL, outFile);
    }
bedFreeList(&bedList);
}

