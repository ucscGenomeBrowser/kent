/* bigBed - interface to binary file with bed-style values (that is a bunch of
 * possibly overlapping regions. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "obscure.h"
#include "dystring.h"
#include "rangeTree.h"
#include "cirTree.h"
#include "bPlusTree.h"
#include "basicBed.h"
#include "asParse.h"
#include "zlibFace.h"
#include "sig.h"
#include "udc.h"
#include "bbiFile.h"
#include "bigBed.h"

struct bbiFile *bigBedFileOpen(char *fileName)
/* Open up big bed file. */
{
return bbiFileOpen(fileName, bigBedSig, "big bed");
}

boolean bigBedFileCheckSigs(char *fileName)
/* check file signatures at beginning and end of file */
{
return bbiFileCheckSigs(fileName, bigBedSig, "big bed");
}

struct bigBedInterval *bigBedIntervalQuery(struct bbiFile *bbi, char *chrom,
	bits32 start, bits32 end, int maxItems, struct lm *lm)
/* Get data for interval.  Return list allocated out of lm.  Set maxItems to maximum
 * number of items to return, or to 0 for all items. */
{
struct bigBedInterval *el, *list = NULL;
int itemCount = 0;
bbiAttachUnzoomedCir(bbi);
bits32 chromId;
struct fileOffsetSize *blockList = bbiOverlappingBlocks(bbi, bbi->unzoomedCir,
	chrom, start, end, &chromId);
struct fileOffsetSize *block, *beforeGap, *afterGap;
struct udcFile *udc = bbi->udc;
boolean isSwapped = bbi->isSwapped;

/* Set up for uncompression optionally. */
char *uncompressBuf = NULL;
if (bbi->uncompressBufSize > 0)
    uncompressBuf = needLargeMem(bbi->uncompressBufSize);

char *mergedBuf = NULL;
for (block = blockList; block != NULL; )
    {
    /* Find contigious blocks and read them into mergedBuf. */
    fileOffsetSizeFindGap(block, &beforeGap, &afterGap);
    bits64 mergedOffset = block->offset;
    bits64 mergedSize = beforeGap->offset + beforeGap->size - mergedOffset;
    udcSeek(udc, mergedOffset);
    mergedBuf = needLargeMem(mergedSize);
    udcMustRead(udc, mergedBuf, mergedSize);
    char *blockBuf = mergedBuf;

    /* Loop through individual blocks within merged section. */
    for (;block != afterGap; block = block->next)
        {
	/* Uncompress if necessary. */
	char *blockPt, *blockEnd;
	if (uncompressBuf)
	    {
	    blockPt = uncompressBuf;
	    int uncSize = zUncompress(blockBuf, block->size, uncompressBuf, bbi->uncompressBufSize);
	    blockEnd = blockPt + uncSize;
	    }
	else
	    {
	    blockPt = blockBuf;
	    blockEnd = blockPt + block->size;
	    }

	while (blockPt < blockEnd)
	    {
	    /* Read next record into local variables. */
	    bits32 chr = memReadBits32(&blockPt, isSwapped);	// Read and discard chromId
	    bits32 s = memReadBits32(&blockPt, isSwapped);
	    bits32 e = memReadBits32(&blockPt, isSwapped);

	    /* calculate length of rest of bed fields */
	    int restLen = strlen(blockPt);

	    /* If we're actually in range then copy it into a new  element and add to list. */
	    if (chr == chromId && s < end && e > start)
		{
		++itemCount;
		if (maxItems > 0 && itemCount > maxItems)
		    break;

		lmAllocVar(lm, el);
		el->start = s;
		el->end = e;
		if (restLen > 0)
		    el->rest = lmCloneStringZ(lm, blockPt, restLen);
		el->chromId = chromId;
		slAddHead(&list, el);
		}

	    // move blockPt pointer to end of previous bed
	    blockPt += restLen + 1;
	    }
	if (maxItems > 0 && itemCount > maxItems)
	    break;
	blockBuf += block->size;
        }
    if (maxItems > 0 && itemCount > maxItems)
        break;
    freez(&mergedBuf);
    }
freez(&mergedBuf);
freeMem(uncompressBuf);
slFreeList(&blockList);
slReverse(&list);
return list;
}

int bigBedIntervalToRow(struct bigBedInterval *interval, char *chrom, char *startBuf, char *endBuf,
	char **row, int rowSize)
/* Convert bigBedInterval into an array of chars equivalent to what you'd get by
 * parsing the bed file. The startBuf and endBuf are used to hold the ascii representation of
 * start and end.  Note that the interval->rest string will have zeroes inserted as a side effect.
 */
{
int fieldCount = 3;
sprintf(startBuf, "%u", interval->start);
sprintf(endBuf, "%u", interval->end);
row[0] = chrom;
row[1] = startBuf;
row[2] = endBuf;
if (!isEmpty(interval->rest))
    {
    int wordCount = chopByChar(interval->rest, '\t', row+3, rowSize-3);
    fieldCount += wordCount;
    }
return fieldCount;
}

static struct bbiInterval *bigBedCoverageIntervals(struct bbiFile *bbi,
	char *chrom, bits32 start, bits32 end, struct lm *lm)
/* Return intervals where the val is the depth of coverage. */
{
/* Get list of overlapping intervals */
struct bigBedInterval *bi, *biList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
if (biList == NULL)
    return NULL;

/* Make a range tree that collects coverage. */
struct rbTree *rangeTree = rangeTreeNew();
for (bi = biList; bi != NULL; bi = bi->next)
    rangeTreeAddToCoverageDepth(rangeTree, bi->start, bi->end);
struct range *range, *rangeList = rangeTreeList(rangeTree);

/* Convert rangeList to bbiInterval list. */
struct bbiInterval *bwi, *bwiList = NULL;
for (range = rangeList; range != NULL; range = range->next)
    {
    lmAllocVar(lm, bwi);
    bwi->start = range->start;
    if (bwi->start < start)
       bwi->start = start;
    bwi->end = range->end;
    if (bwi->end > end)
       bwi->end = end;
    bwi->val = ptToInt(range->val);
    slAddHead(&bwiList, bwi);
    }
slReverse(&bwiList);

/* Clean up and go home. */
rangeTreeFree(&rangeTree);
return bwiList;
}

boolean bigBedSummaryArrayExtended(struct bbiFile *bbi, char *chrom, bits32 start, bits32 end,
	int summarySize, struct bbiSummaryElement *summary)
/* Get extended summary information for summarySize evenly spaced elements into
 * the summary array. */
{
return bbiSummaryArrayExtended(bbi, chrom, start, end, bigBedCoverageIntervals,
	summarySize, summary);
}

boolean bigBedSummaryArray(struct bbiFile *bbi, char *chrom, bits32 start, bits32 end,
	enum bbiSummaryType summaryType, int summarySize, double *summaryValues)
/* Fill in summaryValues with  data from indicated chromosome range in bigBed file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan("") depending on the application.)  Returns FALSE if no data
 * at that position. */
{
return bbiSummaryArray(bbi, chrom, start, end, bigBedCoverageIntervals,
	summaryType, summarySize, summaryValues);
}

struct offsetSize 
/* Simple file offset and file size. */
    {
    bits64 offset; 
    bits64 size;
    };

static int cmpOffsetSizeRef(const void *va, const void *vb)
/* Compare to sort slRef pointing to offsetSize.  Sort is kind of hokey,
 * but guarantees all items that are the same will be next to each other
 * at least, which is all we care about. */
{
const struct slRef *a = *((struct slRef **)va);
const struct slRef *b = *((struct slRef **)vb);
return memcmp(a->val, b->val, sizeof(struct offsetSize));
}

static struct fileOffsetSize *fosFromRedundantBlockList(struct slRef **pBlockList, 
    boolean isSwapped)
/* Convert from list of references to offsetSize format to list of fileOffsetSize
 * format, while removing redundancy.   Sorts *pBlockList as a side effect. */
{
/* Sort input so it it easy to uniquify. */
slSort(pBlockList, cmpOffsetSizeRef);
struct slRef *blockList = *pBlockList;

/* Make new fileOffsetSize for each unique offsetSize. */
struct fileOffsetSize *fosList = NULL, *fos;
struct offsetSize lastOffsetSize = {0,0};
struct slRef *blockRef;
for (blockRef = blockList; blockRef != NULL; blockRef = blockRef->next)
    {
    if (memcmp(&lastOffsetSize, blockRef->val, sizeof(lastOffsetSize)) != 0)
        {
	memcpy(&lastOffsetSize, blockRef->val, sizeof(lastOffsetSize));
	AllocVar(fos);
	if (isSwapped)
	    {
	    fos->offset = byteSwap64(lastOffsetSize.offset);
	    fos->size = byteSwap64(lastOffsetSize.size);
	    }
	else
	    {
	    fos->offset = lastOffsetSize.offset;
	    fos->size = lastOffsetSize.size;
	    }
	slAddHead(&fosList, fos);
	}
    }
slReverse(&fosList);
return fosList;
}


static struct fileOffsetSize *bigBedChunksMatchingName(struct bbiFile *bbi, 
    struct bptFile *index, char *name)
/* Get list of file chunks that match name.  Can slFreeList this when done. */
{
struct slRef *blockList = bptFileFindMultiple(index, 
	name, strlen(name), sizeof(struct offsetSize));
struct fileOffsetSize *fosList = fosFromRedundantBlockList(&blockList, bbi->isSwapped);
slRefFreeListAndVals(&blockList);
return fosList;
}

static struct fileOffsetSize *bigBedChunksMatchingNames(struct bbiFile *bbi, 
	struct bptFile *index, char **names, int nameCount)
/* Get list of file chunks that match any of the names.  Can slFreeList this when done. */
{
/* Go through all names and make a blockList that includes all blocks with any hit to any name.  
 * Many of these blocks will occur multiple times. */
struct slRef *blockList = NULL;
int nameIx;
for (nameIx = 0; nameIx < nameCount; ++nameIx)
    {
    char *name = names[nameIx];
    struct slRef *oneList = bptFileFindMultiple(index, 
	    name, strlen(name), sizeof(struct offsetSize));
    blockList = slCat(oneList, blockList);
    }

/* Create nonredundant list of blocks. */
struct fileOffsetSize *fosList = fosFromRedundantBlockList(&blockList, bbi->isSwapped);

/* Clean up and resturn result. */
slRefFreeListAndVals(&blockList);
return fosList;
}

typedef boolean (*BbFirstWordMatch)(char *line, int fieldIx, void *target);
/* A function that returns TRUE if first word in tab-separated line matches target. */

static void extractField(char *line, int fieldIx, char **retField, int *retFieldSize)
/* Go through tab separated line and figure out start and size of given field. */
{
int i;
fieldIx -= 3;	/* Skip over chrom/start/end, which are not in line. */
for (i=0; i<fieldIx; ++i)
    {
    line = strchr(line, '\t');
    if (line == NULL)
        {
	warn("Not enough fields in extractField of %s", line);
	internalErr();
	}
    line += 1;
    }
char *end = strchr(line, '\t');
if (end == NULL)
    end = line + strlen(line);
*retField = line;
*retFieldSize = end - line;
}

static boolean bbWordMatchesName(char *line, int fieldIx, void *target)
/* Return true if first word of line is same as target, which is just a string. */
{
char *name = target;
int fieldSize;
char *field;
extractField(line, fieldIx, &field, &fieldSize);
return strlen(name) == fieldSize && memcmp(name, field, fieldSize) == 0;
}

static boolean bbWordIsInHash(char *line, int fieldIx, void *target)
/* Return true if first word of line is same as target, which is just a string. */
{
int fieldSize;
char *field;
extractField(line, fieldIx, &field, &fieldSize);
char fieldString[fieldSize+1];
memcpy(fieldString, field, fieldSize);
fieldString[fieldSize] = 0;

/* Return boolean value that reflects whether we found it in hash */
struct hash *hash = target;
return hashLookup(hash, fieldString) != NULL;
}

static struct bigBedInterval *bigBedIntervalsMatchingName(struct bbiFile *bbi, 
    struct fileOffsetSize *fosList, BbFirstWordMatch matcher, int fieldIx, 
    void *target, struct lm *lm)
/* Return list of intervals inside of sectors of bbiFile defined by fosList where the name 
 * matches target somehow. */
{
struct bigBedInterval *interval, *intervalList = NULL;
struct fileOffsetSize *fos;
boolean isSwapped = bbi->isSwapped;
for (fos = fosList; fos != NULL; fos = fos->next)
    {
    /* Read in raw data */
    udcSeek(bbi->udc, fos->offset);
    char *rawData = needLargeMem(fos->size);
    udcRead(bbi->udc, rawData, fos->size);

    /* Optionally uncompress data, and set data pointer to uncompressed version. */
    char *uncompressedData = NULL;
    char *data = NULL;
    int dataSize = 0;
    if (bbi->uncompressBufSize > 0)
	{
	data = uncompressedData = needLargeMem(bbi->uncompressBufSize);
	dataSize = zUncompress(rawData, fos->size, uncompressedData, bbi->uncompressBufSize);
	}
    else
	{
        data = rawData;
	dataSize = fos->size;
	}

    /* Set up for "memRead" routines to more or less treat memory block like file */
    char *blockPt = data, *blockEnd = data + dataSize;
    struct dyString *dy = dyStringNew(32); // Keep bits outside of chrom/start/end here


    /* Read next record into local variables. */
    while (blockPt < blockEnd)
	{
	bits32 chromIx = memReadBits32(&blockPt, isSwapped);
	bits32 s = memReadBits32(&blockPt, isSwapped);
	bits32 e = memReadBits32(&blockPt, isSwapped);
	int c;
	dyStringClear(dy);
	// TODO - can simplify this probably just to for (;;) {if ((c = *blockPt++) == 0) ...
	while ((c = *blockPt++) >= 0)
	    {
	    if (c == 0)
		break;
	    dyStringAppendC(dy, c);
	    }
	if ((*matcher)(dy->string, fieldIx, target))
	    {
	    lmAllocVar(lm, interval);
	    interval->start = s;
	    interval->end = e;
	    interval->rest = cloneString(dy->string);
	    interval->chromId = chromIx;
	    slAddHead(&intervalList, interval);
	    }
	}

    /* Clean up temporary buffers. */
    dyStringFree(&dy);
    freez(&uncompressedData);
    freez(&rawData);
    }
slReverse(&intervalList);
return intervalList;
}



struct bigBedInterval *bigBedNameQuery(struct bbiFile *bbi, struct bptFile *index,
    int fieldIx, char *name, struct lm *lm)
/* Return list of intervals matching file. These intervals will be allocated out of lm. */
{
struct fileOffsetSize *fosList = bigBedChunksMatchingName(bbi, index, name);
struct bigBedInterval *intervalList = bigBedIntervalsMatchingName(bbi, fosList, 
    bbWordMatchesName, fieldIx, name, lm);
slFreeList(&fosList);
return intervalList;
}

struct bigBedInterval *bigBedMultiNameQuery(struct bbiFile *bbi, struct bptFile *index,
    int fieldIx, char **names, int nameCount, struct lm *lm)
/* Fetch all records matching any of the names. Using given index on given field.
 * Return list is allocated out of lm. */
{
/* Set up name index and get list of chunks that match any of our names. */
struct fileOffsetSize *fosList = bigBedChunksMatchingNames(bbi, index, names, nameCount);

/* Create hash of all names. */
struct hash *hash = newHash(0);
int nameIx;
for (nameIx=0; nameIx < nameCount; ++nameIx)
    hashAdd(hash, names[nameIx], NULL);


/* Get intervals where name matches hash target. */
struct bigBedInterval *intervalList = bigBedIntervalsMatchingName(bbi, fosList, 
    bbWordIsInHash, fieldIx, hash, lm);

/* Clean up and return results. */
slFreeList(&fosList);
hashFree(&hash);
return intervalList;
}

void bigBedIntervalListToBedFile(struct bbiFile *bbi, struct bigBedInterval *intervalList, FILE *f)
/* Write out big bed interval list to bed file, looking up chromosome. */
{
char chromName[bbi->chromBpt->keySize+1];
int lastChromId = -1;
struct bigBedInterval *interval;
for (interval = intervalList; interval != NULL; interval = interval->next)
    {
    bbiCachedChromLookup(bbi, interval->chromId, lastChromId, chromName, sizeof(chromName));
    lastChromId = interval->chromId;
    fprintf(f, "%s\t%u\t%u\t%s\n", chromName, interval->start, interval->end, interval->rest);
    }
}

int bigBedIntervalToRowLookupChrom(struct bigBedInterval *interval, 
    struct bigBedInterval *prevInterval, struct bbiFile *bbi,
    char *chromBuf, int chromBufSize, char *startBuf, char *endBuf, char **row, int rowSize)
/* Convert bigBedInterval to array of chars equivalend to what you'd get by parsing the
 * bed file.  If you already know what chromosome the interval is on use the simpler
 * bigBedIntervalToRow.  This one will look up the chromosome based on the chromId field
 * of the interval,  which is relatively time consuming.  To avoid doing this unnecessarily
 * pass in a non-NULL prevInterval,  and if the chromId is the same on prevInterval as this,
 * it will avoid the lookup.  The chromBufSize should be at greater or equal to 
 * bbi->chromBpt->keySize+1. The startBuf and endBuf are used to hold the ascii representation of
 * start and end, and should be 16 bytes.  Note that the interval->rest string will have zeroes 
 * inserted as a side effect.  Returns number of fields in row.  */
{
int lastChromId = (prevInterval == NULL ? -1 : prevInterval->chromId);
bbiCachedChromLookup(bbi, interval->chromId, lastChromId, chromBuf, chromBufSize);
return bigBedIntervalToRow(interval, chromBuf, startBuf, endBuf, row, rowSize);
}

char *bigBedAutoSqlText(struct bbiFile *bbi)
/* Get autoSql text if any associated with file.  Do a freeMem of this when done. */
{
if (bbi->asOffset == 0)
    return NULL;
struct udcFile *f = bbi->udc;
udcSeek(f, bbi->asOffset);
return udcReadStringAndZero(f);
}

struct asObject *bigBedAs(struct bbiFile *bbi)
/* Get autoSql object definition if any associated with file. */
{
if (bbi->asOffset == 0)
    return NULL;
char *asText = bigBedAutoSqlText(bbi);
struct asObject *as = asParseText(asText);
freeMem(asText);
return as;
}

struct asObject *bigBedAsOrDefault(struct bbiFile *bbi)
// Get asObject associated with bigBed - if none exists in file make it up from field counts.
{
struct asObject *as = bigBedAs(bbi);
if (as == NULL)
    as = asParseText(bedAsDef(bbi->definedFieldCount, bbi->fieldCount));
return as;
}

struct asObject *bigBedFileAsObjOrDefault(char *fileName)
// Get asObject associated with bigBed file, or the default.
{
struct bbiFile *bbi = bigBedFileOpen(fileName);
if (bbi)
    {
    struct asObject *as = bigBedAsOrDefault(bbi);
    bbiFileClose(&bbi);
    return as;
    }
return NULL;
}

bits64 bigBedItemCount(struct bbiFile *bbi)
/* Return total items in file. */
{
udcSeek(bbi->udc, bbi->unzoomedDataOffset);
return udcReadBits64(bbi->udc, bbi->isSwapped);
}

struct slName *bigBedListExtraIndexes(struct bbiFile *bbi)
/* Return list of names of extra indexes beyond primary chrom:start-end one" */
{
struct udcFile *udc = bbi->udc;
boolean isSwapped = bbi->isSwapped;

/* See if we have any extra indexes, and if so seek to there. */
bits64 offset = bbi->extraIndexListOffset;
if (offset == 0)
   return NULL;
udcSeek(udc, offset);

/* Construct list of field that are being indexed.  List is list of 
 * field numbers within asObj. */
int i;
struct slInt *intList = NULL, *intEl;
for (i=0; i<bbi->extraIndexCount; ++i)
    {
    bits16 type,fieldCount;
    type = udcReadBits16(udc, isSwapped);
    fieldCount = udcReadBits16(udc, isSwapped);
    udcSeekCur(udc, sizeof(bits64));  // skip over fileOffset
    udcSeekCur(udc, 4);    // skip over reserved bits
    if (fieldCount == 1)
        {
	bits16 fieldId = udcReadBits16(udc, isSwapped);
	udcSeekCur(udc, 2);    // skip over reserved bits
	intEl = slIntNew(fieldId);
	slAddHead(&intList, intEl);
	}
    else
        {
	warn("Not yet understanding indexes on multiple fields at once.");
	internalErr();
	}
    }

/* Now have to make an asObject to find out name that corresponds to this field. */
struct asObject *as = bigBedAsOrDefault(bbi);

/* Make list of field names out of list of field numbers */
struct slName *nameList = NULL;
for (intEl = intList; intEl != NULL; intEl = intEl->next)
    {
    struct asColumn *col = slElementFromIx(as->columnList, intEl->val);
    if (col == NULL)
	{
        warn("Inconsistent bigBed file %s", bbi->fileName);
	internalErr();
	}
    slNameAddHead(&nameList, col->name);
    }

asObjectFree(&as);
return nameList;
}

struct bptFile *bigBedOpenExtraIndex(struct bbiFile *bbi, char *fieldName, int *retFieldIx)
/* Return index associated with fieldName.  Aborts if no such index.  Optionally return
 * index in a row of this field. */
{
struct udcFile *udc = bbi->udc;
boolean isSwapped = bbi->isSwapped;
struct asObject *as = bigBedAsOrDefault(bbi);
struct asColumn *col = asColumnFind(as, fieldName);
if (col == NULL)
    errAbort("No field %s in %s", fieldName, bbi->fileName);
int colIx = slIxFromElement(as->columnList, col);
if (retFieldIx != NULL)
   *retFieldIx = colIx;
asObjectFree(&as);

/* See if we have any extra indexes, and if so seek to there. */
bits64 offset = bbi->extraIndexListOffset;
if (offset == 0)
   errAbort("%s has no indexes", bbi->fileName);
udcSeek(udc, offset);

/* Go through each extra index and see if it's a match */
int i;
for (i=0; i<bbi->extraIndexCount; ++i)
    {
    bits16 type = udcReadBits16(udc, isSwapped);
    bits16 fieldCount = udcReadBits16(udc, isSwapped);
    bits64 fileOffset = udcReadBits64(udc, isSwapped);
    udcSeekCur(udc, 4);    // skip over reserved bits

    if (type != 0)
        {
	warn("Don't understand type %d", type);
	internalErr();
	}
    if (fieldCount == 1)
        {
	bits16 fieldId = udcReadBits16(udc, isSwapped);
	udcSeekCur(udc, 2);    // skip over reserved bits
	if (fieldId == colIx)
	    {
	    udcSeek(udc, fileOffset);
	    struct bptFile *bpt = bptFileAttach(bbi->fileName, udc);
	    return bpt;
	    }
	}
    else
        {
	warn("Not yet understanding indexes on multiple fields at once.");
	internalErr();
	}
    }

errAbort("%s is not indexed in %s", fieldName, bbi->fileName);
return NULL;
}



