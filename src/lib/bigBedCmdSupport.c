/* bigBedCmdSupport - functions to support writing bigBed related commands. */

/* Copyright (C) 2022 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "bigBedCmdSupport.h"
#include "basicBed.h"

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

struct bed *bed3FromPositions(char *fileName)
/* Read positions file and retrun bed 3 list. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
char *chrom;
uint start, end;
struct bed *bed, *bedList = NULL;

/* Convert input to bed file */
while (lineFileNextReal(lf, &line))
    {
    if (parsePosition(line, &chrom, &start, &end))
	{
	if (start > end)
	    errAbort("invalid range, (start - 1)=%u > end=%u", start, end);
	AllocVar(bed);
	bed->chrom=chrom;
        bed->chromStart=start;
        bed->chromEnd=end;
	slAddHead(&bedList, bed);
        }
    else
	{
	errAbort("line %s has invalid position", line);
	}
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


void genericBigToNonBigFromBed(struct bbiFile *bbi, char *bedFileName, FILE *outFile, 
 void (*processChromChunk)(struct bbiFile *bbi, char *chrom, int start, int end, char *bedName, FILE *f)
)
/* Read list of ranges from bed file chrom start end.
 * Automatically sort them by chrom, start. If bed4 pass bed->name too. */
{
struct bed *bed, *bedList = bedLoad3Plus(bedFileName);
slSort(&bedList, bedCmp);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (bed->chromStart > bed->chromEnd)
	errAbort("invalid range, start=%u > end=%u", bed->chromStart, bed->chromEnd);
    processChromChunk(bbi, bed->chrom, bed->chromStart, bed->chromEnd, bed->name, outFile);
    }
bedFreeList(&bedList);
}

void genericBigToNonBigFromPos(struct bbiFile *bbi, char *posFileName, FILE *outFile, 
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
    if (bed->chromStart > bed->chromEnd)
	errAbort("invalid range, start=%u > end=%u", bed->chromStart, bed->chromEnd);
    processChromChunk(bbi, bed->chrom, bed->chromStart, bed->chromEnd, NULL, outFile);
    }
bedFreeList(&bedList);
}

