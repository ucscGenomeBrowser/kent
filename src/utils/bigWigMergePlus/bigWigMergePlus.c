/*
 * bigWigMergePlus - Merge together multiple bigWigs into a single bigWig
 *
 * Copyright (C) 2018 Romain Grégoire <romain.gregoire@mcgill.ca>
 */

#include <stdlib.h>
#include <stdint.h>

#include "common.h"
#include "bbiFile.h"
#include "bigWig.h"
#include "bPlusTree.h"
#include "bwgInternal.h"
#include "cirTree.h"
#include "dystring.h"
#include "hash.h"
#include "linefile.h"
#include "localmem.h"
#include "memalloc.h"
#include "obscure.h"
#include "options.h"
#include "sig.h"
#include "sqlNum.h"
#include "zlibFace.h"

/* version history:
 *  1.0.0 - copied from bigWigMerge
 *  1.0.1 - fix help text
 */

/* A range of bigWig file */
struct Range {
  char *chrom;
  boolean isFullChrom;
  uint32_t start;
  uint32_t end;
};
typedef struct Range Range;

/* An item in a section of a bedGraph. */
struct SectionItem
{
  uint32_t start;
  uint32_t end;           /* Position in chromosome, half open. */
  float val;              /* Single precision value. */
};
typedef struct SectionItem SectionItem;

/* for writing */
static uint8_t reserved8 = 0;
static uint16_t reserved16 = 0;
static uint32_t reserved32 = 0;
static uint64_t reserved64 = 0;


/* for itemsToBigWig */
static int blockSize = 256;
static int itemsPerSlot = 1024;
static boolean doCompress = FALSE;


void usage()
/* Explain usage and exit. */
{
  printf(
      "bigWigMergePlus 1.0.1 - Merge together multiple bigWigs into a single bigWig.\n"
      "\n"
      "Usage:\n"
      "   bigWigMergePlus in1.bw in2.bw .. inN.bw out.bw\n"
      "\n"
      "Options:\n"
      "   -range=chr1:0-100   - Range to merge (default: none)\n"
      "   -threshold=0.N      - Don't output values at or below this threshold. (default: 0.0)\n"
      "   -normalize          - Use values weighted according to each file maximum (default: false)\n"
      );
  exit(0);
}

double clThreshold = 0.0;
boolean clNormalize = FALSE;
Range *clRange = NULL;


static struct optionSpec options[] = {
  {"range", OPTION_STRING},
  {"threshold", OPTION_DOUBLE},
  {"normalize", OPTION_BOOLEAN},
  {NULL, 0},
};


/**
 * Parses a chrom position range
 * (chr6)
 * (chr1:0-10000)
 */
Range *parseRange(char *input)
{
  char *s;
  char *e;
  boolean isFullChrom = TRUE;
  uint32_t start = 0;
  uint32_t end = 0;

  s = strchr(input, ':');

  if (s != NULL) {
    *s++ = 0;

    e = strchr(s, '-');
    if (e == NULL)
        return NULL;

    *e++ = 0;
    if (!isdigit(s[0]) || !isdigit(e[0]))
        return NULL;

    isFullChrom = FALSE;
    start = atoi(s);
    end = atoi(e);
  } else {
    s = input;
  }

  Range *range = malloc(sizeof(struct Range));
  range->chrom = input;
  range->isFullChrom = isFullChrom;
  range->start = start;
  range->end = end;
  return range;
}

/**
 * Compare SectionItems
 */
static int compareSectionItems(const void *va, const void *vb)
{
  const SectionItem *a = (SectionItem *)va;
  const SectionItem *b = (SectionItem *)vb;
  return a->start - b->start;
}

/** Compare strings such as gene names that may have embedded numbers,
 * so that bmp4a comes before bmp14a */
static int bbiChromInfoCmpStringsWithEmbeddedNumbers(const void *va, const void *vb)
{
  const struct bbiChromInfo *a = *((struct bbiChromInfo **)va);
  const struct bbiChromInfo *b = *((struct bbiChromInfo **)vb);
  return cmpStringsWithEmbeddedNumbers(a->name, b->name);
}

/**
 * Read chromosomes from all files and make sure they agree, and return merged list.
 */
struct bbiChromInfo *getAllChroms(struct bbiFile *fileList)
{
  struct bbiFile *file;
  struct hash *hash = hashNew(0);
  struct bbiChromInfo *nameList = NULL;

  for (file = fileList; file != NULL; file = file->next)
  {
    struct bbiChromInfo *info, *next, *infoList = bbiChromList(file);
    for (info = infoList; info != NULL; info = next)
    {
      next = info->next;

      struct bbiChromInfo *oldInfo = hashFindVal(hash, info->name);
      if (oldInfo != NULL)
      {
        if (info->size != oldInfo->size)
          errAbort("Error: Merging from different assemblies? "
              "Chromosome %s is %d in %s but %d in another file",
              info->name, (int)(info->size), file->fileName, (int)oldInfo->size);
      }
      else
      {
        hashAdd(hash, info->name, info);
        slAddHead(&nameList, info);
      }
    }
  }
  slSort(&nameList, bbiChromInfoCmpStringsWithEmbeddedNumbers);
  freeMem(hash);
  return nameList;
}

/**
 * Finds chromosome by name
 */
struct bbiChromInfo *findChrom(struct bbiChromInfo *chromList, char *name)
{
  struct bbiChromInfo *chrom;
  for (chrom = chromList; chrom != NULL; chrom = chrom->next)
  {
    if (sameString(chrom->name, name))
      return chrom;
  }
  return NULL;
}

/** Gets the maximum size from a chrom list */
int getMaxChromSize(struct bbiChromInfo *chromList)
{
  struct bbiChromInfo *chrom;
  int maxSize = 0;

  for (chrom = chromList; chrom != NULL; chrom = chrom->next)
  {
    if (chrom->size > maxSize)
      maxSize = chrom->size;
  }

  return maxSize;
}

/** Return count of numbers at start that are the same as first number.  */
int getDoubleSequenceCount(double *pt, int size)
{
  int sameCount = 1;
  int i;
  double x = pt[0];
  for (i = 1; i < size; i++)
  {
    if (pt[i] != x)
      break;
    sameCount++;
  }
  return sameCount;
}

/**
 * Writes a single section to disk
 */
void writeSection(
    struct bbiChromUsage* usage,
    struct bbiBoundsArray *bounds,
    SectionItem *items,
    int *sectionIx,
    int *itemIx,
    int sectionCount,
    int maxSectionSize,
    struct dyString *stream,
    FILE *f) {

  /* Figure out section position. */
  bits32 chromId = usage->id;
  bits32 sectionStart = items[0].start;
  bits32 sectionEnd = items[*(itemIx) - 1].end;

  /* Save section info for indexing. */
  assert(*sectionIx < sectionCount);
  struct bbiBoundsArray *section = &bounds[*sectionIx];
  *sectionIx += 1;
  section->offset = ftell(f);
  section->range.chromIx = chromId;
  section->range.start = sectionStart;
  section->range.end = sectionEnd;

  /* Output section header to stream. */
  dyStringClear(stream);
  UBYTE type = bwgTypeBedGraph;
  bits16 itemCount = *itemIx;
  dyStringWriteOne(stream, chromId);      // chromId
  dyStringWriteOne(stream, sectionStart); // start
  dyStringWriteOne(stream, sectionEnd);   // end
  dyStringWriteOne(stream, reserved32);   // itemStep
  dyStringWriteOne(stream, reserved32);   // itemSpan
  dyStringWriteOne(stream, type);         // type
  dyStringWriteOne(stream, reserved8);    // reserved
  dyStringWriteOne(stream, itemCount);    // itemCount

  /* Output each item in section to stream. */
  for (int i = 0; i < *itemIx; i++)
  {
    SectionItem *item = &items[i];
    dyStringWriteOne(stream, item->start);
    dyStringWriteOne(stream, item->end);
    dyStringWriteOne(stream, item->val);
  }

  /* Save stream to file, compressing if need be. */
  if (stream->stringSize > maxSectionSize)
    maxSectionSize = stream->stringSize;

  if (doCompress)
  {
    size_t maxCompSize = zCompBufSize(stream->stringSize);
    char compBuf[maxCompSize];
    int compSize = zCompress(stream->string, stream->stringSize, compBuf, maxCompSize);
    mustWrite(f, compBuf, compSize);
  }
  else {
    mustWrite(f, stream->string, stream->stringSize);
  }

  /* Set up for next section. */
  *itemIx = 0;
}

/**
 * Go through chromItems, chunking it into sections that get written to f.
 * Save info about sections in bounds.
 */
void writeSections(
    struct bbiChromUsage *usageList,
    SectionItem **chromItems,
    int itemsPerSlot,
    struct bbiBoundsArray *bounds,
    int sectionCount,
    FILE *f,
    int resTryCount,
    int resScales[],
    int resSizes[],
    bits32 *retMaxSectionSize)
{
  int maxSectionSize = 0;
  struct bbiChromUsage *usage = usageList;

  int itemIx = 0;
  int sectionIx = 0;

  SectionItem items[itemsPerSlot];
  SectionItem *lastB = NULL;
  bits32 resEnds[resTryCount];

  int resTry;
  for (resTry = 0; resTry < resTryCount; resTry++)
    resEnds[resTry] = 0;

  struct dyString *stream = dyStringNew(0);

  int c = 0;
  for (usage = usageList; usage != NULL; usage = usage->next, c++)
  {
    for (int i = 0; i < usage->itemCount; i++)
    {
      SectionItem item = chromItems[c][i];

      /* Figure out whether we need to output section. */
      if (itemIx >= itemsPerSlot)
      {
        writeSection(
          usage,
          bounds,
          items,
          &sectionIx,
          &itemIx,
          sectionCount,
          maxSectionSize,
          stream,
          f);
      }

      /* Parse out input. */
      bits32 start = item.start;
      bits32 end = item.end;
      float val = item.val;

      /* Verify that inputs meets our assumption - that it is a sorted bedGraph file. */
      if (start > end)
        errAbort("Error: start (%u) after end (%u) item %i of %i", start, end, i, c);

      if (lastB != NULL)
      {
        if (lastB->start > start) {
          errAbort("Error: bedGraph not sorted on start item %i of %i (%s [%i])\n%i-%i : %f\n",
              i, usage->itemCount, usage->name, c, start, end, val);
        }
        if (lastB->end > start) {
          errAbort("Error: overlapping regions in bedGraph item %i of %i (%s [%i])",
              i, usage->itemCount, usage->name, c);
        }
      }

      /* Do zoom counting */
      for (resTry = 0; resTry < resTryCount; resTry++)
      {
        bits32 resEnd = resEnds[resTry];
        if (start >= resEnd)
        {
          resSizes[resTry] += 1;
          resEnds[resTry] = resEnd = start + resScales[resTry];
        }
        while (end > resEnd)
        {
          resSizes[resTry] += 1;
          resEnds[resTry] = resEnd = resEnd + resScales[resTry];
        }
      }

      /* Save values in output array. */
      SectionItem *b = &items[itemIx];
      b->start = start;
      b->end = end;
      b->val = val;
      lastB = b;
      itemIx += 1;
    }

    writeSection(
      usage,
      bounds,
      items,
      &sectionIx,
      &itemIx,
      sectionCount,
      maxSectionSize,
      stream,
      f);

    lastB = NULL;
    for (resTry = 0; resTry < resTryCount; ++resTry)
      resEnds[resTry] = 0;
  }

  assert(sectionIx == sectionCount);

  *retMaxSectionSize = maxSectionSize;
}

/**
 * Write out data reduced by factor of initialReduction. Also calculate and keep
 * in memory next reduction level. This is more work than some ways, but it
 * keeps us from having to keep the first reduction entirely in memory.
 */
static struct bbiSummary *itemsWriteReducedOnceReturnReducedTwice(
    struct bbiChromUsage *usageList,
    SectionItem **chromItems,
    int fieldCount,
    bits32 initialReduction,
    bits32 initialReductionCount,
    int zoomIncrement,
    int blockSize,
    int itemsPerSlot,
    struct lm *lm,
    FILE *f,
    bits64 *retDataStart,
    bits64 *retIndexStart,
    struct bbiSummaryElement *totalSum)
{
  struct bbiSummary *twiceReducedList = NULL;
  bits32 doubleReductionSize = initialReduction * zoomIncrement;
  struct bbiChromUsage *usage = usageList;
  struct bbiSummary oneSummary;
  struct bbiSummary *sum = NULL;
  struct bbiBoundsArray *boundsArray,
                        *boundsPt,
                        *boundsEnd;
  boundsPt = AllocArray(boundsArray, initialReductionCount);
  boundsEnd = boundsPt + initialReductionCount;

  *retDataStart = ftell(f);
  writeOne(f, initialReductionCount);
  boolean firstRow = TRUE;

  struct bbiSumOutStream *stream = bbiSumOutStreamOpen(itemsPerSlot, f, doCompress);

  int c = 0;
  for (usage = usageList; usage != NULL; usage = usage->next, c++)
  {
    SectionItem *items = chromItems[c];

    for (int i = 0; i < usage->itemCount; i++)
    {
      SectionItem item = items[i];

      bits32 start = item.start;
      bits32 end = item.end;
      float val = item.val;

      /* Update total summary stuff. */
      bits32 size = end - start;
      if (firstRow)
      {
        totalSum->validCount = size;
        totalSum->minVal = totalSum->maxVal = val;
        totalSum->sumData = val*size;
        totalSum->sumSquares = val*val*size;
        firstRow = FALSE;
      }
      else
      {
        totalSum->validCount += size;
        if (val < totalSum->minVal) totalSum->minVal = val;
        if (val > totalSum->maxVal) totalSum->maxVal = val;
        totalSum->sumData += val*size;
        totalSum->sumSquares += val*val*size;
      }

      /* If start past existing block then output it. */
      if (sum != NULL && sum->end <= start)
      {
        bbiOutputOneSummaryFurtherReduce(sum, &twiceReducedList, doubleReductionSize,
            &boundsPt, boundsEnd, lm, stream);
        sum = NULL;
      }

      /* If don't have a summary we're working on now, make one. */
      if (sum == NULL)
      {
        oneSummary.chromId = usage->id;
        oneSummary.start = start;
        oneSummary.end = start + initialReduction;
        if (oneSummary.end > usage->size) oneSummary.end = usage->size;
        oneSummary.minVal = oneSummary.maxVal = val;
        oneSummary.sumData = oneSummary.sumSquares = 0.0;
        oneSummary.validCount = 0;
        sum = &oneSummary;
      }

      /*
       * Deal with case where might have to split an item between multiple summaries.
       * This loop handles all but the final affected summary in that case.
       */
      while (end > sum->end)
      {
        verbose(3, "Splitting start %d, end %d, sum->start %d, sum->end %d\n", start, end, sum->start, sum->end);

        /* Fold in bits that overlap with existing summary and output. */
        bits32 overlap = rangeIntersection(start, end, sum->start, sum->end);
        sum->validCount += overlap;
        if (sum->minVal > val) sum->minVal = val;
        if (sum->maxVal < val) sum->maxVal = val;
        sum->sumData += val * overlap;
        sum->sumSquares += val*val * overlap;
        bbiOutputOneSummaryFurtherReduce(sum, &twiceReducedList, doubleReductionSize,
            &boundsPt, boundsEnd, lm, stream);
        size -= overlap;

        /* Move summary to next part. */
        sum->start = start = sum->end;
        sum->end = start + initialReduction;
        if (sum->end > usage->size) sum->end = usage->size;
        sum->minVal = sum->maxVal = val;
        sum->sumData = sum->sumSquares = 0.0;
        sum->validCount = 0;
      }

      /* Add to summary. */
      sum->validCount += size;
      if (sum->minVal > val) sum->minVal = val;
      if (sum->maxVal < val) sum->maxVal = val;
      sum->sumData += val * size;
      sum->sumSquares += val*val * size;
    }

    /* Output section before chrom switch */
    if (sum != NULL)
    {
      bbiOutputOneSummaryFurtherReduce(sum, &twiceReducedList, doubleReductionSize,
          &boundsPt, boundsEnd, lm, stream);
      sum = NULL;
    }
  }

  bbiSumOutStreamClose(&stream);

  /* Write out 1st zoom index. */
  int indexOffset = *retIndexStart = ftell(f);
  assert(boundsPt == boundsEnd);
  cirTreeFileBulkIndexToOpenFile(boundsArray, sizeof(boundsArray[0]), initialReductionCount,
      blockSize, itemsPerSlot, NULL, bbiBoundsArrayFetchKey, bbiBoundsArrayFetchOffset,
      indexOffset, f);

  freez(&boundsArray);
  slReverse(&twiceReducedList);
  return twiceReducedList;
}

/**
 * Go through items and collect chromosomes and statistics.
 */
void itemsStats(
    struct bbiChromUsage *usageList,
    SectionItem **chromItems,
    double *retAveSize,
    bits64 *retBedCount)
{
  struct bbiChromUsage *usage = NULL;
  bits64 totalBases = 0;
  bits64 bedCount = 0;

  int c = 0;
  for (usage = usageList; usage != NULL; usage = usage->next, c++)
  {
    SectionItem *items = chromItems[c];

    for (int i = 0; i < usage->itemCount; i++)
    {
      SectionItem item = items[i];

      bedCount++;
      totalBases += (item.start - item.start);
    }
  }

  double aveSize = 0;

  if (bedCount > 0)
    aveSize = (double)totalBases/bedCount;

  *retAveSize = aveSize;
  *retBedCount = bedCount;
}

/**
 * Write out all the zoom levels and return the number of levels written.
 * Writes actual zoom amount and the offsets of the zoomed data and index in the last three
 * parameters.
 * Sorry for all the parameters - it was this or duplicate a big chunk of
 * code between bedToBigBed and bedGraphToBigWig.
 */
int writeZoomLevels(
    struct bbiChromUsage *usageList,
    SectionItem **chromItems,
    FILE *f,            /* Output. */
    int blockSize,      /* Size of index block */
    int itemsPerSlot,   /* Number of data points bundled at lowest level. */
    int fieldCount,     /* Number of fields in bed (4 for bedGraph) */
    boolean doCompress,     /* Do we compress. Answer really should be yes! */
    bits64 dataSize,        /* Size of data on disk (after compression if any). */
    int resTryCount, int resScales[], int resSizes[],   /* How much to zoom at each level */
    bits32 zoomAmounts[bbiMaxZoomLevels],      /* Fills in amount zoomed at each level. */
    bits64 zoomDataOffsets[bbiMaxZoomLevels],  /* Fills in where data starts for each zoom level. */
    bits64 zoomIndexOffsets[bbiMaxZoomLevels], /* Fills in where index starts for each level. */
    struct bbiSummaryElement *totalSum)
{
  /* Write out first zoomed section while storing in memory next zoom level. */
  assert(resTryCount > 0);
  int maxReducedSize = dataSize / 2;
  int initialReduction = 0;
  int initialReducedCount = 0;

  /* Figure out initialReduction for zoom - one that is maxReducedSize or less. */
  int resTry;
  for (resTry = 0; resTry < resTryCount; ++resTry)
  {
    bits64 reducedSize = resSizes[resTry] * sizeof(struct bbiSummaryOnDisk);

    if (doCompress)
      reducedSize /= 2;   // Estimate!

    if (reducedSize <= maxReducedSize)
    {
      initialReduction = resScales[resTry];
      initialReducedCount = resSizes[resTry];
      break;
    }
  }
  verbose(2, "initialReduction %d, initialReducedCount = %d\n",
      initialReduction, initialReducedCount);

  /* Force there to always be at least one zoom.  It may waste a little space on small
   * files, but it makes files more uniform, and avoids special case code for calculating
   * overall file summary. */
  if (initialReduction == 0)
  {
    initialReduction = resScales[0];
    initialReducedCount = resSizes[0];
  }

  /* Call routine to make the initial zoom level and also a bit of work towards further levels. */
  struct lm *lm = lmInit(0);
  int zoomIncrement = bbiResIncrement;

  struct bbiSummary *rezoomedList = itemsWriteReducedOnceReturnReducedTwice(
      usageList,
      chromItems,
      fieldCount,
      initialReduction,
      initialReducedCount,
      zoomIncrement,
      blockSize,
      itemsPerSlot,
      lm,
      f,
      &zoomDataOffsets[0],
      &zoomIndexOffsets[0],
      totalSum);

  verboseTime(2, "itemsWriteReducedOnceReturnReducedTwice");

  zoomAmounts[0] = initialReduction;

  int zoomLevels = 1;

  /* Loop around to do any additional levels of zoom. */
  int zoomCount = initialReducedCount;
  int reduction = initialReduction * zoomIncrement;
  while (zoomLevels < bbiMaxZoomLevels)
  {
    int rezoomCount = slCount(rezoomedList);
    if (rezoomCount >= zoomCount)
      break;
    zoomCount = rezoomCount;
    zoomDataOffsets[zoomLevels] = ftell(f);
    zoomIndexOffsets[zoomLevels] = bbiWriteSummaryAndIndex(rezoomedList,
        blockSize, itemsPerSlot, doCompress, f);
    zoomAmounts[zoomLevels] = reduction;
    zoomLevels++;
    reduction *= zoomIncrement;
    rezoomedList = bbiSummarySimpleReduce(rezoomedList, reduction, lm);
  }
  lmCleanup(&lm);
  verboseTime(2, "further reductions");
  return zoomLevels;
}

/**
 * Output chroms & items to bigWig
 */
void itemsToBigWig(
    struct bbiChromUsage *usageList,
    SectionItem **chromItems,
    char *outName)
{
  verboseTimeInit();

  int i;
  double aveSize = 0;
  bits64 bedCount = 0;
  bits32 uncompressBufSize = 0;

  itemsStats(usageList, chromItems, &aveSize, &bedCount);

  verboseTime(2, "pass1");
  verbose(2, "%d chroms, aveSize=%g, bedCount=%lld\n", slCount(usageList), aveSize, bedCount);

  /* Write out dummy header, zoom offsets. */
  FILE *f = mustOpen(outName, "wb");
  bbiWriteDummyHeader(f);
  bbiWriteDummyZooms(f);

  /* Write out dummy total summary. */
  struct bbiSummaryElement totalSum;
  ZeroVar(&totalSum);
  bits64 totalSummaryOffset = ftell(f);
  bbiSummaryElementWrite(f, &totalSum);

  /* Write out chromosome/size database. */
  bits64 chromTreeOffset = ftell(f);
  bbiWriteChromInfo(usageList, blockSize, f);

  /* Set up to keep track of possible initial reduction levels. */
  int resScales[bbiMaxZoomLevels];
  int resSizes[bbiMaxZoomLevels];
  int resTryCount = bbiCalcResScalesAndSizes(aveSize, resScales, resSizes);

  /* Write out primary full resolution data in sections, collect stats to use for reductions. */
  bits64 dataOffset = ftell(f);
  bits64 sectionCount = bbiCountSectionsNeeded(usageList, itemsPerSlot);
  writeOne(f, sectionCount);
  struct bbiBoundsArray *boundsArray;
  AllocArray(boundsArray, sectionCount);

  bits32 maxSectionSize = 0;

  writeSections(usageList, chromItems, itemsPerSlot, boundsArray, sectionCount, f,
      resTryCount, resScales, resSizes, &maxSectionSize);

  verboseTime(2, "pass2");

  /* Write out primary data index. */
  bits64 indexOffset = ftell(f);
  cirTreeFileBulkIndexToOpenFile(boundsArray, sizeof(boundsArray[0]), sectionCount,
      blockSize, 1, NULL, bbiBoundsArrayFetchKey, bbiBoundsArrayFetchOffset,
      indexOffset, f);

  verboseTime(2, "index write");

  /* Declare arrays and vars that track the zoom levels we actually output. */
  bits32 zoomAmounts[bbiMaxZoomLevels];
  bits64 zoomDataOffsets[bbiMaxZoomLevels];
  bits64 zoomIndexOffsets[bbiMaxZoomLevels];

  /* Call monster zoom maker library function that bedToBigBed also uses. */
  int zoomLevels = writeZoomLevels(usageList, chromItems, f, blockSize, itemsPerSlot,
      4,
      doCompress, indexOffset - dataOffset,
      resTryCount, resScales, resSizes,
      zoomAmounts, zoomDataOffsets, zoomIndexOffsets, &totalSum);


  /* Figure out buffer size needed for uncompression if need be. */
  if (doCompress)
  {
    int maxZoomUncompSize = itemsPerSlot * sizeof(struct bbiSummaryOnDisk);
    uncompressBufSize = max(maxSectionSize, maxZoomUncompSize);
  }

  /* Go back and rewrite header. */
  rewind(f);
  bits32 sig = bigWigSig;
  bits16 version = bbiCurrentVersion;
  bits16 summaryCount = zoomLevels;

  /* Write fixed header */
  writeOne(f, sig);
  writeOne(f, version);
  writeOne(f, summaryCount);
  writeOne(f, chromTreeOffset);
  writeOne(f, dataOffset);
  writeOne(f, indexOffset);
  writeOne(f, reserved16);        // fieldCount
  writeOne(f, reserved16);        // definedFieldCount
  writeOne(f, reserved64);        // autoSqlOffset
  writeOne(f, totalSummaryOffset);
  writeOne(f, uncompressBufSize);
  writeOne(f, reserved64);        // nameIndexOffset
  assert(ftell(f) == 64);

  /* Write summary headers with data. */
  verbose(2, "Writing %d levels of zoom\n", zoomLevels);
  for (i=0; i<zoomLevels; ++i)
  {
    verbose(3, "zoomAmounts[%d] = %d\n", i, (int)zoomAmounts[i]);
    writeOne(f, zoomAmounts[i]);
    writeOne(f, reserved32);
    writeOne(f, zoomDataOffsets[i]);
    writeOne(f, zoomIndexOffsets[i]);
  }
  /* Write rest of summary headers with no data. */
  for (i=zoomLevels; i<bbiMaxZoomLevels; ++i)
  {
    writeOne(f, reserved32);
    writeOne(f, reserved32);
    writeOne(f, reserved64);
    writeOne(f, reserved64);
  }

  /* Write total summary. */
  fseek(f, totalSummaryOffset, SEEK_SET);
  bbiSummaryElementWrite(f, &totalSum);

  /* Write end signature. */
  fseek(f, 0L, SEEK_END);
  writeOne(f, sig);

  carefulClose(&f);
}

/**
 * Merge together multiple bigWigs into a single one
 */
void bigWigMergePlus(int inCount, char *inFilenames[], char *outFile)
{
  /* Factors for normalization */
  double factors[inCount];
  double totalMaximum = 0.0;

  /* Make a list of open bigWig files. */
  struct bbiFile *inFile;
  struct bbiFile *inFiles = NULL;

  for (int i = 0; i < inCount; i++)
  {
    inFile = bigWigFileOpen(inFilenames[i]);
    slAddTail(&inFiles, inFile);

    if (clNormalize) {
      struct bbiSummaryElement sum = bbiTotalSummary(inFile);
      factors[i] = sum.maxVal;
      totalMaximum += sum.maxVal;
    }
  }

  if (clNormalize) {
    for (int i = 0; i < inCount; i++)
    {
      factors[i] = factors[i] / totalMaximum;
    }
  }

  struct bbiChromInfo *chrom;
  struct bbiChromInfo *chromList = getAllChroms(inFiles);
  int maxChromSize = getMaxChromSize(chromList);
  int chromCount = slCount(chromList);


  verbose(1, "Got %d chromosomes from %d bigWigs (maxSize: %i)\n",
      slCount(chromList), slCount(inFiles), maxChromSize);

  /* Assert provided chrom exists if applicable */
  if (clRange != NULL) {
    if (findChrom(chromList, clRange->chrom) == NULL)
      errAbort("Error: Chromosome parsed from -position param didn't match "
          "any chromosome in files: %s",
          clRange->chrom);
  }

  /* Informations to write will be stored here */
  struct bbiChromUsage *usageList = NULL;
  SectionItem **chromItems = needMem(chromCount * sizeof(SectionItem *));

  /* Buffer to merge values */
  double *mergeBuf = needHugeMem(maxChromSize * sizeof(double));

  int c = 0;
  for (chrom = chromList; chrom != NULL; chrom = chrom->next, c++)
  {
    struct bbiChromUsage *usage = needMem(sizeof(struct bbiChromUsage));
    usage->id = chrom->id;
    usage->name = chrom->name;
    usage->size = chrom->size;
    slAddHead(&usageList, usage);

    /* If there is a range and it's not this chrom, just fill a single zero item */
    if (clRange != NULL && differentString(clRange->chrom, chrom->name)) {
      usage->itemCount = 1;
      chromItems[c] = needMem(sizeof(SectionItem));
      chromItems[c][0] = (SectionItem) { 0, chrom->size, 0.0 };
      continue;
    }

    struct lm *lm = lmInit(0);
    chromItems[c] = needHugeMem(chrom->size * sizeof(SectionItem));

    /* Reset mergeBuf memory to 0 */
    memset(mergeBuf, 0, chrom->size * sizeof(double));

    verbose(1, "Processing \x1b[1;93m%s\x1b[0m (%d bases)\n", chrom->name, chrom->size);

    /* Loop through each input file grabbing data and merging it in. */
    int f = 0;
    for (inFile = inFiles; inFile != NULL; inFile = inFile->next, f++)
    {
      uint32_t start = clRange == NULL ? 0 : clRange->start;
      uint32_t end   = clRange == NULL || clRange->isFullChrom ? chrom->size : clRange->end;

      struct bbiInterval *ivList = bigWigIntervalQuery(inFile, chrom->name, start, end, lm);
      struct bbiInterval *iv;

      verbose(2, "Got %d intervals in %s\n", slCount(ivList), inFile->fileName);

      for (iv = ivList; iv != NULL; iv = iv->next)
      {
        for (int i = iv->start; i < iv->end; i++)
        {
          if (clNormalize)
            mergeBuf[i] += iv->val * factors[f];
          else
            mergeBuf[i] += iv->val;
        }
      }
    }

    /* Store each range of contiguous values as a section item */
    int index = 0;
    int sameCount;
    for (int i = 0; i < chrom->size; i += sameCount)
    {
      sameCount = getDoubleSequenceCount(mergeBuf + i, chrom->size - i);
      double val = mergeBuf[i];
      if (val > clThreshold) {
        SectionItem item = { i, i + sameCount, val };
        chromItems[c][index++] = item;
      }
    }
    usage->itemCount = index;

    /* Sort items by start position */
    qsort(chromItems[c], usage->itemCount, sizeof(SectionItem), compareSectionItems);

    lmCleanup(&lm);
  }
  slReverse(&usageList);

  verbose(1, "\n");

  itemsToBigWig(usageList, chromItems, outFile);
}



int main(int argc, char *argv[])
  /* Process command line. */
{
  optionInit(&argc, argv, options);
  clThreshold = optionDouble("threshold", clThreshold);
  clNormalize = optionExists("normalize");
  char *range = optionVal("range", NULL);

  if (range != NULL) {
    clRange = parseRange(range);

    if (clRange == NULL)
      errAbort("Error: Invalid value for argument -range (format: chrom:start-end)");
  }

  int minArgs = 4;

  if (argc < minArgs)
    usage();

  bigWigMergePlus(argc-2, argv+1, argv[argc-1]);

  return 0;
}
