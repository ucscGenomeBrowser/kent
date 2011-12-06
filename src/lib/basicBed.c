/* basicBed contains the basic code for Browser Extensible Data (bed) files and tables.
 * The idea behind bed is that the first three fields are defined and required.
 * A total of 15 fields are defined, and the file can contain any number of these.
 * In addition after any number of defined fields there can be custom fields that
 * are not defined in the bed spec.
 *
 * There's additional bed-related code in src/hg/inc/bed.h.  This module contains the
 * stuff that's independent of the database and other genomic structures. */


#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "sqlNum.h"
#include "sqlList.h"
#include "rangeTree.h"
#include "binRange.h"
#include "basicBed.h"


void bedStaticLoad(char **row, struct bed *ret)
/* Load a row from bed table into ret.  The contents of ret will
 * be replaced at the next call to this function. */
{
ret->chrom = row[0];
ret->chromStart = sqlUnsigned(row[1]);
ret->chromEnd = sqlUnsigned(row[2]);
ret->name = row[3];
}

struct bed *bedLoad(char **row)
/* Load a bed from row fetched with select * from bed
 * from database.  Dispose of this with bedFree(). */
{
struct bed *ret;
AllocVar(ret);
ret->chrom = cloneString(row[0]);
ret->chromStart = sqlUnsigned(row[1]);
ret->chromEnd = sqlUnsigned(row[2]);
ret->name = cloneString(row[3]);
return ret;
}

struct bed *bedCommaIn(char **pS, struct bed *ret)
/* Create a bed out of a comma separated string. 
 * This will fill in ret if non-null, otherwise will
 * return a new bed */
{
char *s = *pS;

if (ret == NULL)
    AllocVar(ret);
ret->chrom = sqlStringComma(&s);
ret->chromStart = sqlUnsignedComma(&s);
ret->chromEnd = sqlUnsignedComma(&s);
ret->name = sqlStringComma(&s);
*pS = s;
return ret;
}

void bedFree(struct bed **pEl)
/* Free a single dynamically allocated bed such as created
 * with bedLoad(). */
{
struct bed *el;

if ((el = *pEl) == NULL) return;
freeMem(el->chrom);
freeMem(el->name);
freeMem(el->blockSizes);
freeMem(el->chromStarts);
freeMem(el->expIds);
freeMem(el->expScores);
freez(pEl);
}

void bedFreeList(struct bed **pList)
/* Free a list of dynamically allocated bed's */
{
struct bed *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    bedFree(&el);
    }
*pList = NULL;
}

void bedOutput(struct bed *el, FILE *f, char sep, char lastSep) 
/* Print out bed.  Separate fields with sep. Follow last field with lastSep. */
{
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->chrom);
if (sep == ',') fputc('"',f);
fputc(sep,f);
fprintf(f, "%u", el->chromStart);
fputc(sep,f);
fprintf(f, "%u", el->chromEnd);
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->name);
if (sep == ',') fputc('"',f);
fputc(lastSep,f);
}

/* --------------- End of AutoSQL generated code. --------------- */

int bedCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart. */
{
const struct bed *a = *((struct bed **)va);
const struct bed *b = *((struct bed **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
return dif;
}

int bedCmpEnd(const void *va, const void *vb)
/* Compare to sort based on chrom,chromEnd. */
{
const struct bed *a = *((struct bed **)va);
const struct bed *b = *((struct bed **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromEnd - b->chromEnd;
return dif;
}

int bedCmpScore(const void *va, const void *vb)
/* Compare to sort based on score - lowest first. */
{
const struct bed *a = *((struct bed **)va);
const struct bed *b = *((struct bed **)vb);
return a->score - b->score;
}

int bedCmpPlusScore(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart. */
{
const struct bed *a = *((struct bed **)va);
const struct bed *b = *((struct bed **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    {
    dif = (a->chromStart - b->chromStart) * 1000 +(a->score - b->score);
    }
return dif;
}

int bedCmpSize(const void *va, const void *vb)
/* Compare to sort based on size of element (end-start == size) */
{
const struct bed *a = *((struct bed **)va);
const struct bed *b = *((struct bed **)vb);
int a_size = a->chromEnd - a->chromStart;
int b_size = b->chromEnd - b->chromStart;
return (a_size - b_size);
}

int bedCmpChromStrandStart(const void *va, const void *vb)
/* Compare to sort based on chrom,strand,chromStart. */
{
const struct bed *a = *((struct bed **)va);
const struct bed *b = *((struct bed **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = strcmp(a->strand, b->strand);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
return dif;
}

struct bedLine *bedLineNew(char *line)
/* Create a new bedLine based on tab-separated string s. */
{
struct bedLine *bl;
char *s, c;

AllocVar(bl);
bl->chrom = cloneString(line);
s = strchr(bl->chrom, '\t');
if (s == NULL)
    errAbort("Expecting tab in bed line %s", line);
*s++ = 0;
c = *s;
if (isdigit(c) || (c == '-' && isdigit(s[1])))
    {
    bl->chromStart = atoi(s);
    bl->line = s;
    return bl;
    }
else
    {
    errAbort("Expecting start position in second field of %s", line);
    return NULL;
    }
}

void bedLineFree(struct bedLine **pBl)
/* Free up memory associated with bedLine. */
{
struct bedLine *bl;

if ((bl = *pBl) != NULL)
    {
    freeMem(bl->chrom);
    freez(pBl);
    }
}

void bedLineFreeList(struct bedLine **pList)
/* Free a list of dynamically allocated bedLine's */
{
struct bedLine *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    bedLineFree(&el);
    }
*pList = NULL;
}


int bedLineCmp(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct bedLine *a = *((struct bedLine **)va);
const struct bedLine *b = *((struct bedLine **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
return dif;
}


void bedSortFile(char *inFile, char *outFile)
/* Sort a bed file (in place, overwrites old file. */
{
struct lineFile *lf = NULL;
FILE *f = NULL;
struct bedLine *blList = NULL, *bl;
char *line;
int lineSize;

verbose(2, "Reading %s\n", inFile);
lf = lineFileOpen(inFile, TRUE);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
        continue;
    bl = bedLineNew(line);
    slAddHead(&blList, bl);
    }
lineFileClose(&lf);

verbose(2, "Sorting\n");
slSort(&blList, bedLineCmp);

verbose(2, "Writing %s\n", outFile);
f = mustOpen(outFile, "w");
for (bl = blList; bl != NULL; bl = bl->next)
    {
    fprintf(f, "%s\t%s\n", bl->chrom, bl->line);
    if (ferror(f))
        {
	perror("Writing error\n");
	errAbort("%s is truncated, sorry.", outFile);
	}
    }
fclose(f);
}

struct bed *bedLoad3(char **row)
/* Load first three fields of bed. */
{
struct bed *ret;
AllocVar(ret);
ret->chrom = cloneString(row[0]);
ret->chromStart = sqlUnsigned(row[1]);
ret->chromEnd = sqlUnsigned(row[2]);
return ret;
}

struct bed *bedLoad5(char **row)
/* Load first five fields of bed. */
{
struct bed *ret;
AllocVar(ret);
ret->chrom = cloneString(row[0]);
ret->chromStart = sqlUnsigned(row[1]);
ret->chromEnd = sqlUnsigned(row[2]);
ret->name = cloneString(row[3]);
ret->score = sqlSigned(row[4]);
return ret;
}

struct bed *bedLoad6(char **row)
/* Load first six fields of bed. */
{
struct bed *ret = bedLoad5(row);
safef(ret->strand, sizeof(ret->strand), "%s", row[5]);
return ret;
}

/* it turns out that it isn't just hgLoadBed and custom tracks
 *	that may encounter the r,g,b specification.  Any program that
 *	reads bed files may enconter them, so take care of them
 *	at any time.  The strchr() function is very fast which will
 *	be a failure in the vast majority of cases parsing integers,
 *	therefore, this shouldn't be too severe a performance hit.
 */
static int itemRgbColumn(char *column9)
{
int itemRgb = 0;
/*  Allow comma separated list of rgb values here   */
char *comma = strchr(column9, ',');
if (comma)
    {
    if (-1 == (itemRgb = bedParseRgb(column9)))
	errAbort("ERROR: expecting r,g,b specification, "
		    "found: '%s'", column9);
    }
else
    itemRgb = sqlUnsigned(column9);
return itemRgb;
}

struct bed *bedLoad12(char **row)
/* Load a bed from row fetched with select * from bed
 * from database.  Dispose of this with bedFree(). */
{
struct bed *ret;
int sizeOne;

AllocVar(ret);
ret->blockCount = sqlSigned(row[9]);
ret->chrom = cloneString(row[0]);
ret->chromStart = sqlUnsigned(row[1]);
ret->chromEnd = sqlUnsigned(row[2]);
ret->name = cloneString(row[3]);
ret->score = sqlSigned(row[4]);
strcpy(ret->strand, row[5]);
ret->thickStart = sqlUnsigned(row[6]);
ret->thickEnd = sqlUnsigned(row[7]);
ret->itemRgb = itemRgbColumn(row[8]);
sqlSignedDynamicArray(row[10], &ret->blockSizes, &sizeOne);
assert(sizeOne == ret->blockCount);
sqlSignedDynamicArray(row[11], &ret->chromStarts, &sizeOne);
assert(sizeOne == ret->blockCount);
return ret;
}

struct bed *bedLoadN(char *row[], int wordCount)
/* Convert a row of strings to a bed. */
{
struct bed * bed;
int count;

AllocVar(bed);
bed->chrom = cloneString(row[0]);
bed->chromStart = sqlUnsigned(row[1]);
bed->chromEnd = sqlUnsigned(row[2]);
if (wordCount > 3)
     bed->name = cloneString(row[3]);
if (wordCount > 4)
     bed->score = sqlSigned(row[4]);
if (wordCount > 5)
     bed->strand[0] = row[5][0];
if (wordCount > 6)
     bed->thickStart = sqlUnsigned(row[6]);
else
     bed->thickStart = bed->chromStart;
if (wordCount > 7)
     bed->thickEnd = sqlUnsigned(row[7]);
else
     bed->thickEnd = bed->chromEnd;
if (wordCount > 8)
    bed->itemRgb = itemRgbColumn(row[8]);
if (wordCount > 9)
    bed->blockCount = sqlUnsigned(row[9]);
if (wordCount > 10)
    sqlSignedDynamicArray(row[10], &bed->blockSizes, &count);
if (wordCount > 11)
    sqlSignedDynamicArray(row[11], &bed->chromStarts, &count);
if (wordCount > 12)
    bed->expCount = sqlUnsigned(row[12]);
if (wordCount > 13)
    sqlSignedDynamicArray(row[13], &bed->expIds, &count);
if (wordCount > 14)
    sqlFloatDynamicArray(row[14], &bed->expScores, &count);
return bed;
}

struct bed *bedLoadNAllChrom(char *fileName, int numFields, char* chrom) 
/* Load bed entries from a tab-separated file that have the given chrom.
 * Dispose of this with bedFreeList(). */
{
struct bed *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[numFields];

while (lineFileRow(lf, row))
    {
    el = bedLoadN(row, numFields);
    if(chrom == NULL || sameString(el->chrom, chrom))
        slAddHead(&list, el);
    else
        bedFree(&el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct bed *bedLoadNAll(char *fileName, int numFields) 
/* Load all bed from a tab-separated file.
 * Dispose of this with bedFreeList(). */
{
return bedLoadNAllChrom(fileName, numFields, NULL);
}

struct bed *bedLoadAll(char *fileName)
/* Determines how many fields are in a bedFile and load all beds from
 * a tab-separated file.  Dispose of this with bedFreeList(). */
{
struct bed *list = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *row[bedKnownFields];

while (lineFileNextReal(lf, &line))
    {
    int numFields = chopByWhite(line, row, ArraySize(row));
    if (numFields < 4)
	errAbort("file %s doesn't appear to be in bed format. At least 4 fields required, got %d", fileName, numFields);
    slAddHead(&list, bedLoadN(row, numFields));
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

void bedLoadAllReturnFieldCount(char *fileName, struct bed **retList, int *retFieldCount)
/* Load bed of unknown size and return number of fields as well as list of bed items.
 * Ensures that all lines in bed file have same field count. */
{
struct bed *list = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *row[bedKnownFields];
int fieldCount = 0;

while (lineFileNextReal(lf, &line))
    {
    int numFields = chopByWhite(line, row, ArraySize(row));
    if (numFields < 4)
	errAbort("file %s doesn't appear to be in bed format. At least 4 fields required, got %d", 
		fileName, numFields);
    if (fieldCount == 0)
        fieldCount = numFields;
    else
        if (fieldCount != numFields)
	    errAbort("Inconsistent number of fields in file. %d on line %d of %s, %d previously.",
	        numFields, lf->lineIx, lf->fileName, fieldCount);
    slAddHead(&list, bedLoadN(row, fieldCount));
    }
lineFileClose(&lf);
slReverse(&list);
*retList = list;
*retFieldCount = fieldCount;
}

static void bedOutputN_Opt(struct bed *el, int wordCount, FILE *f,
	char sep, char lastSep, boolean useItemRgb)
/* Write a bed of wordCount fields, optionally interpreting field nine
	as R,G,B values. */
{
int i;
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->chrom);
if (sep == ',') fputc('"',f);
fputc(sep,f);
fprintf(f, "%u", el->chromStart);
fputc(sep,f);
fprintf(f, "%u", el->chromEnd);
if (wordCount <= 3)
    {
    fputc(lastSep, f);
    return;
    }
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->name);
if (sep == ',') fputc('"',f);
if (wordCount <= 4)
    {
    fputc(lastSep, f);
    return;
    }
fputc(sep,f);
fprintf(f, "%d", el->score);
if (wordCount <= 5)
    {
    fputc(lastSep, f);
    return;
    }
fputc(sep,f);
if (sep == ',') fputc('"',f);
fprintf(f, "%s", el->strand);
if (sep == ',') fputc('"',f);
if (wordCount <= 6)
    {
    fputc(lastSep, f);
    return;
    }
fputc(sep,f);
fprintf(f, "%u", el->thickStart);
if (wordCount <= 7)
    {
    fputc(lastSep, f);
    return;
    }
fputc(sep,f);
fprintf(f, "%u", el->thickEnd);
if (wordCount <= 8)
    {
    fputc(lastSep, f);
    return;
    }
fputc(sep,f);
if (useItemRgb)
    fprintf(f, "%d,%d,%d", (el->itemRgb & 0xff0000) >> 16,
        (el->itemRgb & 0xff00) >> 8, (el->itemRgb & 0xff));
else
    fprintf(f, "%u", el->itemRgb);
if (wordCount <= 9)
    {
    fputc(lastSep, f);
    return;
    }
fputc(sep,f);
fprintf(f, "%d", el->blockCount);
if (wordCount <= 10)
    {
    fputc(lastSep, f);
    return;
    }
fputc(sep,f);
if (sep == ',') fputc('{',f);
for (i=0; i<el->blockCount; ++i)
    {
    fprintf(f, "%d", el->blockSizes[i]);
    fputc(',', f);
    }
if (sep == ',') fputc('}',f);
if (wordCount <= 11)
    {
    fputc(lastSep, f);
    return;
    }
fputc(sep,f);
if (sep == ',') fputc('{',f);
for (i=0; i<el->blockCount; ++i)
    {
    fprintf(f, "%d", el->chromStarts[i]);
    fputc(',', f);
    }
if (sep == ',') fputc('}',f);

if (wordCount <= 12)
    {
    fputc(lastSep, f);
    return;
    }
fputc(sep,f);
fprintf(f, "%d", el->expCount);

if (wordCount <= 13)
    {
    fputc(lastSep, f);
    return;
    }
fputc(sep,f);
if (sep == ',') fputc('{',f);
for (i=0; i<el->expCount; ++i)
    {
    fprintf(f, "%d", el->expIds[i]);
    fputc(',', f);
    }
if (sep == ',') fputc('}',f);


if (wordCount <= 14)
    {
    fputc(lastSep, f);
    return;
    }
fputc(sep,f);
if (sep == ',') fputc('{',f);
for (i=0; i<el->expCount; ++i)
    {
    fprintf(f, "%g", el->expScores[i]);
    fputc(',', f);
    }
if (sep == ',') fputc('}',f);


fputc(lastSep,f);
}

void bedOutputN(struct bed *el, int wordCount, FILE *f, char sep, char lastSep)
/* Write a bed of wordCount fields. */
{
bedOutputN_Opt(el, wordCount, f, sep, lastSep, FALSE);
}

void bedOutputNitemRgb(struct bed *el, int wordCount, FILE *f,
	char sep, char lastSep)
/* Write a bed of wordCount fields, interpret column 9 as RGB. */
{
bedOutputN_Opt(el, wordCount, f, sep, lastSep, TRUE);
}


int bedTotalBlockSize(struct bed *bed)
/* Return total size of all blocks. */
{
int total = 0;
int i;
if (bed->blockCount == 0)
    return bed->chromEnd - bed->chromStart;
for (i=0; i<bed->blockCount; ++i)
    total += bed->blockSizes[i];
return total;
}

int bedBlockSizeInRange(struct bed *bed, int rangeStart, int rangeEnd)
/* Get size of all parts of all exons between rangeStart and rangeEnd. */
{
int total = 0;
int i;
for (i=0; i<bed->blockCount; ++i)
    {
    int start = bed->chromStart + bed->chromStarts[i];
    int end = start + bed->blockSizes[i];
    total += positiveRangeIntersection(start, end, rangeStart, rangeEnd);
    }
return total;
}

int bedTotalThickBlockSize(struct bed *bed)
/* Return total size of all thick blocks. */
{
return bedBlockSizeInRange(bed, bed->thickStart, bed->thickEnd);
}

int bedStartThinSize(struct bed *bed)
/* Return total size of all blocks before thick part. */
{
return bedBlockSizeInRange(bed, bed->chromStart, bed->thickStart);
}

int bedEndThinSize(struct bed *bed)
/* Return total size of all blocks after thick part. */
{
return bedBlockSizeInRange(bed, bed->thickEnd, bed->chromEnd);
}

struct bed *bedFromPsl(struct psl *psl)
/* Convert a single psl to a bed structure */
{
struct bed *bed;
int i, blockCount, *chromStarts, chromStart;

/* A tiny bit of error checking on the psl. */
if (psl->qStart >= psl->qEnd || psl->qEnd > psl->qSize 
    || psl->tStart >= psl->tEnd || psl->tEnd > psl->tSize)
    {
    errAbort("mangled psl format for %s", psl->qName);
    }

/* Allocate bed and fill in from psl. */
AllocVar(bed);
bed->chrom = cloneString(psl->tName);
bed->chromStart = bed->thickStart = chromStart = psl->tStart;
bed->chromEnd = bed->thickEnd = psl->tEnd;
bed->score = 1000 - 2*pslCalcMilliBad(psl, TRUE);
if (bed->score < 0) bed->score = 0;
bed->strand[0] = psl->strand[0];
bed->blockCount = blockCount = psl->blockCount;
bed->blockSizes = (int *)cloneMem(psl->blockSizes,(sizeof(int)*psl->blockCount));
if (pslIsProtein(psl))
    {
    /* Convert blockSizes from protein to dna. */
    for (i=0; i<blockCount; ++i)
	bed->blockSizes[i] *= 3;
    }
bed->chromStarts = chromStarts = (int *)cloneMem(psl->tStarts, (sizeof(int)*psl->blockCount));
bed->name = cloneString(psl->qName);

/* Switch minus target strand to plus strand. */
if (psl->strand[1] == '-')
    {
    int chromSize = psl->tSize;
    reverseInts(bed->blockSizes, blockCount);
    reverseInts(chromStarts, blockCount);
    for (i=0; i<blockCount; ++i)
	chromStarts[i] = chromSize - chromStarts[i] - bed->blockSizes[i];
    if (bed->strand[0] == '-')
	bed->strand[0] = '+';
    else
	bed->strand[0] = '-';
    }

/* Convert coordinates to relative. */
for (i=0; i<blockCount; ++i)
    chromStarts[i] -= chromStart;
return bed;
}

void makeItBed12(struct bed *bedList, int numFields)
/* If it's less than bed 12, make it bed 12. The numFields */
/* param is for how many fields the bed *currently* has. */
{
int i = 1;
struct bed *cur;
for (cur = bedList; cur != NULL; cur = cur->next)
    {
    /* it better be bigger than bed 3. */
    if (numFields < 4)
	{
	char name[50];
	safef(name, ArraySize(name), "item.%d", i+1);
	cur->name = cloneString(name);
	}
    if (numFields < 5)
	cur->score = 1000;
    if (numFields < 6)
	{
	cur->strand[0] = '?';
	cur->strand[1] = '\0';
	}
    if (numFields < 8)
	{
	cur->thickStart = cur->chromStart;
	cur->thickEnd = cur->chromEnd;
	}
    if (numFields < 9)
	cur->itemRgb = 0;
    if (numFields < 12)
	{
	cur->blockSizes = needMem(sizeof(int));
	cur->chromStarts = needMem(sizeof(int));
	cur->blockCount = 1;
	cur->chromStarts[0] = 0;
	cur->blockSizes[0] = cur->chromEnd - cur->chromStart;
	}
    i++;
    }
}

struct bed *lmCloneBed(struct bed *bed, struct lm *lm)
/* Make a copy of bed in local memory. */
{
struct bed *newBed;
if (bed == NULL)
    return NULL;
lmAllocVar(lm, newBed);
newBed->chrom = lmCloneString(lm, bed->chrom);
newBed->chromStart = bed->chromStart;
newBed->chromEnd = bed->chromEnd;
newBed->name = lmCloneString(lm, bed->name);
newBed->score = bed->score;
strncpy(newBed->strand, bed->strand, sizeof(bed->strand));
newBed->thickStart = bed->thickStart;
newBed->thickEnd = bed->thickEnd;
newBed->itemRgb = bed->itemRgb;
newBed->blockCount = bed->blockCount;
if (bed->blockCount > 0)
    {
    newBed->blockSizes = lmCloneMem(lm, bed->blockSizes, 
    	sizeof(bed->blockSizes[0]) * bed->blockCount);
    newBed->chromStarts = lmCloneMem(lm, bed->chromStarts, 
    	sizeof(bed->chromStarts[0]) * bed->blockCount);
    }
newBed->expCount = bed->expCount;
if (bed->expCount > 0)
    {
    newBed->expIds = lmCloneMem(lm, bed->expIds, 
    	sizeof(bed->expIds[0]) * bed->expCount);
    newBed->expScores = lmCloneMem(lm, bed->expScores, 
    	sizeof(bed->expScores[0]) * bed->expCount);
    }
return(newBed);
}


struct bed *cloneBed(struct bed *bed)
/* Make an all-newly-allocated copy of a single bed record. */
{
struct bed *newBed;
if (bed == NULL)
    return NULL;
AllocVar(newBed);
newBed->chrom = cloneString(bed->chrom);
newBed->chromStart = bed->chromStart;
newBed->chromEnd = bed->chromEnd;
newBed->name = cloneString(bed->name);
newBed->score = bed->score;
strncpy(newBed->strand, bed->strand, sizeof(bed->strand));
newBed->thickStart = bed->thickStart;
newBed->thickEnd = bed->thickEnd;
newBed->itemRgb = bed->itemRgb;
newBed->blockCount = bed->blockCount;
if (bed->blockCount > 0)
    {
    newBed->blockSizes = needMem(sizeof(int) * bed->blockCount);
    memcpy(newBed->blockSizes, bed->blockSizes,
	   sizeof(int) * bed->blockCount);
    newBed->chromStarts = needMem(sizeof(int) * bed->blockCount);
    memcpy(newBed->chromStarts, bed->chromStarts,
	   sizeof(int) * bed->blockCount);
    }
newBed->expCount = bed->expCount;
if (bed->expCount > 0)
    {
    newBed->expIds = needMem(sizeof(int) * bed->expCount);
    memcpy(newBed->expIds, bed->expIds,
	   sizeof(int) * bed->expCount);
    newBed->expScores = needMem(sizeof(float) * bed->expCount);
    memcpy(newBed->expScores, bed->expScores,
	   sizeof(float) * bed->expCount);
    }
return(newBed);
}


struct bed *cloneBedList(struct bed *bedList)
/* Make an all-newly-allocated list copied from bed. */
{
struct bed *bedListOut = NULL, *bed=NULL;

for (bed=bedList;  bed != NULL;  bed=bed->next)
    {
    struct bed *newBed = cloneBed(bed);
    slAddHead(&bedListOut, newBed);
    }

slReverse(&bedListOut);
return bedListOut;
}

struct bed *bedListNextDifferentChrom(struct bed *bedList)
/* Return next bed in list that is from a different chrom than the start of the list. */
{
char *firstChrom = bedList->chrom;
struct bed *bed;
for (bed = bedList->next; bed != NULL; bed = bed->next)
    if (!sameString(firstChrom, bed->chrom))
        break;
return bed;
}

struct bed *bedCommaInN(char **pS, struct bed *ret, int fieldCount)
/* Create a bed out of a comma separated string looking for fieldCount
 * fields. This will fill in ret if non-null, otherwise will return a
 * new bed */
{
char *s = *pS;
int i;

if (ret == NULL)
    AllocVar(ret);
ret->chrom = sqlStringComma(&s);
ret->chromStart = sqlUnsignedComma(&s);
ret->chromEnd = sqlUnsignedComma(&s);
if (fieldCount > 3)
    ret->name = sqlStringComma(&s);
if (fieldCount > 4)
    ret->score = sqlUnsignedComma(&s);
if (fieldCount > 5)
    sqlFixedStringComma(&s, ret->strand, sizeof(ret->strand));
if (fieldCount > 6)
    ret->thickStart = sqlUnsignedComma(&s);
else
    ret->thickStart = ret->chromStart;
if (fieldCount > 7)
    ret->thickEnd = sqlUnsignedComma(&s);
else
     ret->thickEnd = ret->chromEnd;
if (fieldCount > 8)
    ret->itemRgb = sqlUnsignedComma(&s);
if (fieldCount > 9)
    ret->blockCount = sqlUnsignedComma(&s);
if (fieldCount > 10)
    {
    s = sqlEatChar(s, '{');
    AllocArray(ret->blockSizes, ret->blockCount);
    for (i=0; i<ret->blockCount; ++i)
	{
	ret->blockSizes[i] = sqlSignedComma(&s);
	}
    s = sqlEatChar(s, '}');
    s = sqlEatChar(s, ',');
    }
if(fieldCount > 11)
    {
    s = sqlEatChar(s, '{');
    AllocArray(ret->chromStarts, ret->blockCount);
    for (i=0; i<ret->blockCount; ++i)
	{
	ret->chromStarts[i] = sqlSignedComma(&s);
	}
    s = sqlEatChar(s, '}');
    s = sqlEatChar(s, ',');
    }
if (fieldCount > 12)
    ret->expCount = sqlSignedComma(&s);
if (fieldCount > 13)
    {
    s = sqlEatChar(s, '{');
    AllocArray(ret->expIds, ret->expCount);
    for (i=0; i<ret->expCount; ++i)
	{
	ret->expIds[i] = sqlSignedComma(&s);
	}
    s = sqlEatChar(s, '}');
    s = sqlEatChar(s, ',');
    }
if (fieldCount > 14)
    {
    s = sqlEatChar(s, '{');
    AllocArray(ret->expScores, ret->expCount);
    for (i=0; i<ret->expCount; ++i)
	{
	ret->expScores[i] = sqlFloatComma(&s);
	}
    s = sqlEatChar(s, '}');
    s = sqlEatChar(s, ',');
    }
*pS = s;
return ret;
}

struct hash *readBedToBinKeeper(char *sizeFileName, char *bedFileName, int wordCount)
/* Read a list of beds and return results in hash of binKeeper structure for fast query
 * See also bedsIntoKeeperHash, which takes the beds read into a list already, but
 * dispenses with the need for the sizeFile. */
{
struct binKeeper *bk; 
struct bed *bed;
struct lineFile *lf = lineFileOpen(sizeFileName, TRUE);
struct lineFile *bf = lineFileOpen(bedFileName , TRUE);
struct hash *hash = newHash(0);
char *chromRow[2];
char *row[3] ;

assert (wordCount == 3);
while (lineFileRow(lf, chromRow))
    {
    char *name = chromRow[0];
    int size = lineFileNeedNum(lf, chromRow, 1);

    if (hashLookup(hash, name) != NULL)
        warn("Duplicate %s, ignoring all but first\n", name);
    else
        {
        bk = binKeeperNew(0, size);
        assert(size > 1);
	hashAdd(hash, name, bk);
        }
    }
while (lineFileNextRow(bf, row, ArraySize(row)))
    {
    bed = bedLoadN(row, wordCount);
    bk = hashMustFindVal(hash, bed->chrom);
    binKeeperAdd(bk, bed->chromStart, bed->chromEnd, bed);
    }
lineFileClose(&bf);
lineFileClose(&lf);
return hash;
}

int bedParseRgb(char *itemRgb)
/*	parse a string: "r,g,b" into three unsigned char values
	returned as 24 bit number, or -1 for failure */
{
char dupe[64];
int wordCount;
char *row[4];

strncpy(dupe, itemRgb, sizeof(dupe));
wordCount = chopString(dupe, ",", row, ArraySize(row));

if ((wordCount != 3) || (!isdigit(row[0][0]) ||
    !isdigit(row[1][0]) || !isdigit(row[2][0])))
	return (-1);

return ( ((atoi(row[0]) & 0xff) << 16) |
	((atoi(row[1]) & 0xff) << 8) |
	(atoi(row[2]) & 0xff) );
}

long long bedTotalSize(struct bed *bedList)
/* Add together sizes of all beds in list. */
{
long long total=0;
struct bed *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
    total += (bed->chromEnd - bed->chromStart);
return total;
}

void bedIntoRangeTree(struct bed *bed, struct rbTree *rangeTree)
/* Add all blocks in bed to range tree.  For beds without blocks,
 * add entire bed. */
{
int i;
if (bed->blockCount == 0)
    rangeTreeAdd(rangeTree, bed->chromStart, bed->chromEnd);
else
    {
    for (i=0; i < bed->blockCount; ++i)
	{
	int start = bed->chromStart + bed->chromStarts[i];
	int end = start + bed->blockSizes[i];
	rangeTreeAdd(rangeTree, start, end);
	}
    }
}

struct rbTree *bedToRangeTree(struct bed *bed)
/* Convert bed into a range tree. */
{
struct rbTree *rangeTree = rangeTreeNew();
bedIntoRangeTree(bed, rangeTree);
return rangeTree;
}

int bedRangeTreeOverlap(struct bed *bed, struct rbTree *rangeTree)
/* Return number of bases bed overlaps with rangeTree. */
{
int totalOverlap = 0;
if (bed->blockCount == 0)
    totalOverlap = rangeTreeOverlapSize(rangeTree, bed->chromStart, bed->chromEnd);
else
    {
    int i;
    for (i=0; i < bed->blockCount; ++i)
	{
	int start = bed->chromStart + bed->chromStarts[i];
	int end = start + bed->blockSizes[i];
	totalOverlap += rangeTreeOverlapSize(rangeTree, start, end);
	}
    }
return totalOverlap;
}

int bedSameStrandOverlap(struct bed *a, struct bed *b)
/* Return amount of block-level overlap on same strand between a and b */
{
/* Make sure on same strand, chromosome, and that overlap
 * at the non-block level. */
if (a->strand[0] != b->strand[0])
    return 0;
if (!sameString(a->chrom, b->chrom))
    return 0;
int outerOverlap = rangeIntersection(a->chromStart, a->chromEnd, 
	b->chromStart, b->chromEnd);
if (outerOverlap <= 0)
    return 0;

/* If both beds are non-blocked then we're pretty much done. */
if (a->blockCount == 0 && b->blockCount == 0)
    return outerOverlap;

/* Otherwise make up a range tree containing regions covered by a,
 * and figure out how much b overlaps it.. */
struct rbTree *rangeTree = bedToRangeTree(a);
int overlap = bedRangeTreeOverlap(b, rangeTree);

/* Clean up and return result. */
rangeTreeFree(&rangeTree);
return overlap;
}

boolean bedExactMatch(struct bed *oldBed, struct bed *newBed)
/* Return TRUE if it's an exact match. */
{
if (oldBed->blockCount != newBed->blockCount)
    return FALSE;
int oldSize = bedTotalBlockSize(oldBed);
int newSize = bedTotalBlockSize(newBed);
int overlap = bedSameStrandOverlap(oldBed, newBed);
return  (oldSize == newSize && oldSize == overlap);
}

boolean bedCompatibleExtension(struct bed *oldBed, struct bed *newBed)
/* Return TRUE if newBed is a compatible extension of oldBed, meaning
 * all internal exons and all introns of old bed are contained, in the 
 * same order in the new bed. */
{
/* New bed must have at least as many exons as old bed... */
if (oldBed->blockCount > newBed->blockCount)
    return FALSE;

/* New bed must also must also encompass old bed. */
if (newBed->chromStart > oldBed->chromStart)
    return FALSE;
if (newBed->chromEnd < oldBed->chromEnd)
    return FALSE;

/* Look for an exact match */
int oldSize = bedTotalBlockSize(oldBed);
int newSize = bedTotalBlockSize(newBed);
int overlap = bedSameStrandOverlap(oldBed, newBed);
if (oldSize == newSize && oldSize == overlap)
    return TRUE;

/* If overlap is smaller than old size then we can't be a superset. */
if (overlap < oldSize)
    return FALSE;

/* If we're a single exon bed then we're done. */
if (oldBed->blockCount <= 1)
    return TRUE;

/* Otherwise we look for first intron start in old bed, and then
 * flip through new bed until we find an intron that starts at the
 * same place. */
int oldFirstIntronStart = oldBed->chromStart + oldBed->chromStarts[0] + oldBed->blockSizes[0];
int newLastBlock = newBed->blockCount-1, oldLastBlock = oldBed->blockCount-1;
int newIx, oldIx;
for (newIx=0; newIx < newLastBlock; ++newIx)
    {
    int iStartNew = newBed->chromStart + newBed->chromStarts[newIx] + newBed->blockSizes[newIx];
    if (iStartNew == oldFirstIntronStart)
        break;
    }
if (newIx == newLastBlock)
    return FALSE;

/* Now we go through all introns in old bed, and make sure they match. */
for (oldIx=0; oldIx < oldLastBlock; ++oldIx, ++newIx)
    {
    int iStartOld = oldBed->chromStart + oldBed->chromStarts[oldIx] + oldBed->blockSizes[oldIx];
    int iEndOld = oldBed->chromStart + oldBed->chromStarts[oldIx+1];
    int iStartNew = newBed->chromStart + newBed->chromStarts[newIx] + newBed->blockSizes[newIx];
    int iEndNew = newBed->chromStart + newBed->chromStarts[newIx+1];
    if (iStartOld != iStartNew || iEndOld != iEndNew)
        return FALSE;
    }

/* Finally, make sure that the new bed doesn't contain any introns that overlap with the
 * last exon of the old bed */
for(; newIx < newLastBlock; ++newIx)
    {
    int iStartNew = newBed->chromStart + newBed->chromStarts[newIx] + newBed->blockSizes[newIx];
    if (iStartNew < oldBed->chromEnd)
        return FALSE;
    else if (iStartNew >= oldBed->chromEnd)
        break;
    }

return TRUE;
}

struct bed3 *bed3New(char *chrom, int start, int end)
/* Make new bed3. */
{
struct bed3 *bed;
AllocVar(bed);
bed->chrom = cloneString(chrom);
bed->chromStart = start;
bed->chromEnd = end;
return bed;
}

struct bed *bedThickOnly(struct bed *in)
/* Return a bed that only has the thick part. (Which is usually the CDS). */
{
if (in->thickStart >= in->thickEnd)
     return NULL;
if (in->expCount != 0 || in->expIds != NULL || in->expScores != NULL)
   errAbort("Sorry, bedThickOnly only works on beds with up to 12 fields.");

/* Allocate output, and fill in simple fields. */
struct bed *out;
AllocVar(out);
out->chrom = cloneString(in->chrom);
out->chromStart = out->thickStart = in->thickStart;
out->chromEnd = out->thickEnd = in->thickEnd;
out->name = cloneString(in->name);
out->strand[0] = in->strand[0];
out->score = in->score;
out->itemRgb = in->itemRgb;

/* If need be fill in blocks. */
if (in->blockCount > 0)
   {
   /* Count up blocks inside CDS. */
   int i;
   int outBlockCount = 0;
   for (i=0; i<in->blockCount; ++i)
       {
       int start = in->chromStart + in->chromStarts[i];
       int end = start + in->blockSizes[i];
       if (start < in->thickStart) start = in->thickStart;
       if (end > in->thickEnd) end = in->thickEnd;
       if (start < end)
           outBlockCount += 1;
	}

    /* This trivieal case shouldn't happen, but just in case, we deal with it. */
    if (outBlockCount == 0)
        {
	freeMem(out);
	return NULL;
	}

    /* Allocate block arrays for output. */
    out->blockCount = outBlockCount;
    AllocArray(out->chromStarts, outBlockCount);
    AllocArray(out->blockSizes, outBlockCount);

    /* Scan through input one more time, copying to out. */
    int outBlockIx = 0;
    for (i=0; i<in->blockCount; ++i)
	{
	int start = in->chromStart + in->chromStarts[i];
	int end = start + in->blockSizes[i];
	if (start < in->thickStart) start = in->thickStart;
	if (end > in->thickEnd) end = in->thickEnd;
	if (start < end)
	    {
	    out->chromStarts[outBlockIx] = start - out->chromStart;
	    out->blockSizes[outBlockIx] = end - start;
	    outBlockIx += 1;
	    }
	}
    }
return out;
}

struct bed *bedThickOnlyList(struct bed *inList)
/* Return a list of beds that only are the thick part of input. */
{
struct bed *outList = NULL, *out, *in;
for (in = inList; in != NULL; in = in->next)
    {
    if ((out = bedThickOnly(in)) != NULL)
        slAddHead(&outList, out);
    }
slReverse(&outList);
return outList;
}

char *bedAsDef(int bedFieldCount, int totalFieldCount)
/* Return an autoSql definition for a bed of given number of fields. 
 * Normally totalFieldCount is equal to bedFieldCount.  If there are extra
 * fields they are just given the names field16, field17, etc and type string. */
{
if (bedFieldCount < 3 || bedFieldCount > 15)
    errAbort("bedFieldCount is %d, but must be between %d and %d in bedAsDef", bedFieldCount, 3, 15);
struct dyString *dy = dyStringNew(0);
dyStringAppend(dy, 
    "table bed\n"
    "\"Browser Extensible Data\"\n"
    "   (\n"
    "   string chrom;       \"Reference sequence chromosome or scaffold\"\n"
    "   uint   chromStart;  \"Start position in chromosome\"\n"
    "   uint   chromEnd;    \"End position in chromosome\"\n"
    );
if (bedFieldCount >= 4)
    dyStringAppend(dy, "   string name;        \"Name of item.\"\n");
if (bedFieldCount >= 5)
    dyStringAppend(dy, "   int score;          \"Score (0-1000)\"\n");
if (bedFieldCount >= 6)
    dyStringAppend(dy, "   char[1] strand;     \"+ or - for strand\"\n");
if (bedFieldCount >= 7)
    dyStringAppend(dy, "   uint thickStart;   \"Start of where display should be thick (start codon)\"\n");
if (bedFieldCount >= 8)
    dyStringAppend(dy, "   uint thickEnd;     \"End of where display should be thick (stop codon)\"\n");
if (bedFieldCount >= 9)
    dyStringAppend(dy, "   string reserved;     \"Used as itemRgb as of 2004-11-22\"\n");
if (bedFieldCount >= 10)
    dyStringAppend(dy, "   int blockCount;    \"Number of blocks\"\n");
if (bedFieldCount >= 11)
    dyStringAppend(dy, "   int[blockCount] blockSizes; \"Comma separated list of block sizes\"\n");
if (bedFieldCount >= 12)
    dyStringAppend(dy, "   int[blockCount] chromStarts; \"Start positions relative to chromStart\"\n");
if (bedFieldCount >= 13)
    dyStringAppend(dy, "   int expCount;	\"Experiment count\"\n");
if (bedFieldCount >= 14)
    dyStringAppend(dy, "   int[expCount] expIds;	\"Comma separated list of experiment ids. Always 0,1,2,3....\"\n");
if (bedFieldCount >= 15)
    dyStringAppend(dy, "   float[expCount] expScores; \"Comma separated list of experiment scores.\"\n");
int i;
for (i=bedFieldCount+1; i<=totalFieldCount; ++i)
    dyStringPrintf(dy, "string field%d;	\"Undocumented field\"\n", i+1);
dyStringAppend(dy, "   )\n");
return dyStringCannibalize(&dy);
}

