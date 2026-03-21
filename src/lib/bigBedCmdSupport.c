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
    hashAdd(chromHash, cloneString(chrom->name), chrom);
    }
return chromHash;
}

void parsePositionIntoRows(char *position, char **row, struct lineFile *lf)
/* parse position into row array. Free row[0] to release memory. */
{
char *chrom = cloneString(position);
char *startStr = strchr(chrom, ':');
if (startStr == NULL)
    {
    errAbort("required character colon (:) not found in input string position %s fileName=%s row number=%d",
	position, lf->fileName, lf->lineIx);
    }
*startStr++ = '\0';
if (isEmpty(chrom))
    {
    errAbort("chrom is not allowed to be empty in input string position %s fileName=%s row number=%d",
	position, lf->fileName, lf->lineIx);
    }
char *endStr = strchr(startStr, '-');
if (endStr == NULL)
    {
    errAbort("required character dash (-) not found in input string position %s fileName=%s row number=%d",
	position, lf->fileName, lf->lineIx);
    }
*endStr++ = '\0';
if (isEmpty(startStr))
    {
    errAbort("start %s is not allowed to be empty in input string position %s fileName=%s row number=%d",
	startStr, position, lf->fileName, lf->lineIx);
    }
if (isEmpty(endStr))
    {
    errAbort("end %s is not allowed to be empty in input string position %s fileName=%s row number=%d",
	endStr, position, lf->fileName, lf->lineIx);
    }
row[0] = chrom;
row[1] = startStr;
row[2] = endStr;
}

void lineFileAllIntsFieldNameAndLine(struct lineFile *lf, char *words[], int wordIx, char *fieldName, void *val,
  boolean isSigned,  int byteCount, char *typeString, boolean noNeg, char *line)
/* Returns long long integer from converting the input string. Aborts on error. */
{
char *s = words[wordIx];
char errMsg[256];
int res = lineFileCheckAllIntsNoAbort(s, val, isSigned, byteCount, typeString, noNeg, errMsg, sizeof errMsg);
if (res > 0)
    {
    errAbort("%s in %s field %d line %d of %s, got %s in line %s",
	errMsg, fieldName, wordIx+1, lf->lineIx, lf->fileName, s, line);
    }
}


static struct bed *bed3FromPositionString(char *line, struct lineFile *lf)
/* Read positions line and return bed 3. */
{
struct bed *bed = NULL;
char *row[3];

/* Convert input to bed record */

parsePositionIntoRows(line, row, lf);  

AllocVar(bed);
bed->chrom=cloneString(row[0]);
lineFileAllIntsFieldNameAndLine(lf, row, 1, "chromStart", &bed->chromStart, FALSE, 4, "integer", FALSE, line);
lineFileAllIntsFieldNameAndLine(lf, row, 2, "chromEnd"  , &bed->chromEnd  , FALSE, 4, "integer", FALSE, line);

if (bed->chromStart == 0)
    errAbort("invalid position start = 0 since position starts at 1, fileName=%s row number=%d %s", lf->fileName, lf->lineIx, line);

--bed->chromStart;

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
    bed = bed3FromPositionString(line, lf);
    slAddHead(&bedList, bed);
    }
lineFileClose(&lf);
return bedList;
}

static struct bed *bedLoad3Plus(char *fileName)
/* Determines how many fields are in a bedFile and load all beds from
 * a tab-separated file.  Dispose of this with bedFreeList(). 
 * Small change by Michael to require only 3 or more fields. Meaning we will accept bed3 
 */
{
struct bed *list = NULL, *bed = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *row[bedKnownFields];

while (lineFileNextReal(lf, &line))
    {
    char *val=cloneString(line);
    int numFields = chopByWhite(val, row, ArraySize(row));
    if (numFields < 3)
	errAbort("file %s doesn't appear to be in bed format. At least 3 fields required, got %d", fileName, numFields);

    AllocVar(bed);
    bed->chrom = cloneString(row[0]);
    lineFileAllIntsFieldNameAndLine(lf, row, 1, "chromStart", &bed->chromStart, FALSE, 4, "integer", FALSE, line);
    lineFileAllIntsFieldNameAndLine(lf, row, 2, "chromEnd"  , &bed->chromEnd  , FALSE, 4, "integer", FALSE, line);
    if (numFields >= 4)
	bed->name = cloneString(row[3]);

    slAddHead(&list, bed);

    freeMem(val);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct bed *bedLoad3FromRangeOption(struct slName *ranges)
/* Determines how many fields are in a bedFile and load all beds from
 * possibly multiple -range parameters.  Dispose of this with bedFreeList(). 
 */
{
struct slName *range = NULL;
struct bed *bed, *list = NULL;
char *row[3];

// make fake lf to pass fileName and line number in error messages
struct lineFile *lf=NULL;
AllocVar(lf);
lf->fileName=cloneString("range");
lf->lineIx = 0;

for(range=ranges; range; range=range->next)
    {
    ++lf->lineIx;
    if (strchr(range->name, ':') || strchr(range->name, '-'))
        {
        bed = bed3FromPositionString(range->name, lf);
        }
    else
        {
	char *val=cloneString(range->name);
	int numFields = chopByWhite(val, row, ArraySize(row));
	if (numFields != 3)
	    errAbort("range %s doesn't appear to be in bed format. 3 fields required, got %d", 
		range->name, numFields);
	// copy column 1 as chromName clonestring
	AllocVar(bed);
	bed->chrom = cloneString(row[0]);
	lineFileAllIntsFieldNameAndLine(lf, row, 1, "chromStart", &bed->chromStart, FALSE, 4, "integer", FALSE, range->name);
	lineFileAllIntsFieldNameAndLine(lf, row, 2, "chromEnd"  , &bed->chromEnd  , FALSE, 4, "integer", FALSE, range->name);
	freez(&val);
        }
    slAddHead(&list, bed);
    }
slReverse(&list);
freez(&lf->fileName);
freez(&lf);
return list;
}

static void checkBed(struct bed *bed, struct hash *chromHash)
/* check a bed record */
{
if (chromHash && !hashLookup(chromHash, bed->chrom))
    errAbort("invalid chrom = %s not found", bed->chrom);
struct bbiChromInfo *chrom = NULL;
if (chromHash && hashLookup(chromHash, bed->chrom))
    chrom = (struct bbiChromInfo *) hashFindVal(chromHash, bed->chrom);

// complain and abort over coords and chromSize
if (chrom && (bed->chromStart > chrom->size))
    errAbort("invalid start=%u > chromSize=%u", bed->chromStart, chrom->size);
if (chrom && (bed->chromEnd > chrom->size))
    errAbort("invalid end=%u > chromSize=%u", bed->chromEnd, chrom->size);

if (bed->chromStart > bed->chromEnd)
    errAbort("invalid range, start=%u > end=%u", bed->chromStart, bed->chromEnd);
}

void genericBigToNonBigFromBed(struct bbiFile *bbi, struct hash *chromHash, char *bedFileName, FILE *outFile, 
 void (*processChromChunk)(struct bbiFile *bbi, char *chrom, uint start, uint end, char *bedName, FILE *f)
)
/* Read list of ranges from bed file chrom start end.
 * Automatically sort them by chrom, start. If bed4 pass bed->name too. */
{
struct bed *bed, *bedList = bedLoad3Plus(bedFileName);
slSort(&bedList, bedCmp);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    checkBed(bed, chromHash);
    processChromChunk(bbi, bed->chrom, bed->chromStart, bed->chromEnd, bed->name, outFile);
    }
bedFreeList(&bedList);
}

void genericBigToNonBigFromRange(struct bbiFile *bbi, struct hash *chromHash, FILE *outFile, struct slName *ranges,
 void (*processChromChunk)(struct bbiFile *bbi, char *chrom, uint start, uint end, char *bedName, FILE *f)
)
/* Read list of ranges as chrom start end or chrom:start-end.
 * Automatically sort them by chrom, start. */
{
struct bed *bed, *bedList = bedLoad3FromRangeOption(ranges);
slSort(&bedList, bedCmp);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    checkBed(bed, chromHash);
    processChromChunk(bbi, bed->chrom, bed->chromStart, bed->chromEnd, bed->name, outFile);
    }
bedFreeList(&bedList);
}


void genericBigToNonBigFromPos(struct bbiFile *bbi, struct hash *chromHash, char *posFileName, FILE *outFile, 
 void (*processChromChunk)(struct bbiFile *bbi, char *chrom, uint start, uint end, char *bedName, FILE *f)
)
{
/* Read  positions from file (chrom:start-end). starts are 1-based,
 * but after conversion to bed3 list, they are 0-based. 
 * Automatically sort them by chrom, start */
struct bed *bed, *bedList = bed3FromPositions(posFileName);

slSort(&bedList, bedCmp);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    checkBed(bed, chromHash);
    processChromChunk(bbi, bed->chrom, bed->chromStart, bed->chromEnd, NULL, outFile);
    }
bedFreeList(&bedList);
}

