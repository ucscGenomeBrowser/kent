/* checkBigDbSnp - Flag overlapping items and look for clustering errors. */

/* Copyright (C) 2019 The Regents of the University of California */

#include "common.h"
#include "bigDbSnp.h"
#include "dnautil.h"
#include "indelShift.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "twoBit.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkBigDbSnp - Add ucscNotes for overlapping items, clustering errors, mapping errors\n"
  "usage:\n"
  "   checkBigDbSnp hgNN.dbSnpMMM.bigDbSnp hgNN.2bit hgNN.dbSnpMMM.checked.bigDbSnp\n"
  "\n"
  "options:\n"
  "   -mapErrIds=file.txt    file.txt contains one rs# ID per line that had a mapping error.\n"
  "                          Add note otherSeqMapErr if current rs is found in file.\n"
  "\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {"mapErrIds", OPTION_STRING},
    {NULL, 0},
};

static boolean bigDbSnpOverlap(struct bigDbSnp *a, struct bigDbSnp *b)
/* Return TRUE if a and b overlap; special treatment for 0-length insertions. */
{
if (differentString(a->chrom, b->chrom))
    return FALSE;
boolean aIsIns = (a->chromStart == a->chromEnd);
boolean bIsIns = (b->chromStart == b->chromEnd);
if (aIsIns && bIsIns)
    return (a->chromStart == b->chromStart);
int aStart = a->chromStart, aEnd = a->chromEnd;
int bStart = b->chromStart, bEnd = b->chromEnd;
if (aIsIns)
    {
    aStart--;
    aEnd++;
    }
if (bIsIns)
    {
    bStart--;
    bEnd++;
    }
return (aStart < bEnd && bStart < aEnd);
}

static void checkMapErrIds(struct bigDbSnp *bds, struct hash *mapErrIdHash)
/* If rs# ID is found in mapErrIdHash, add ucscNote otherMapErr. */
{
if (hashIntValDefault(mapErrIdHash, bds->name, 0))
    {
    struct dyString *dy = dyStringCreate("%s%s,", bds->ucscNotes, bdsOtherMapErr);
    freeMem(bds->ucscNotes);
    bds->ucscNotes = dyStringCannibalize(&dy);
    }
}

static void queueAdd(struct bigDbSnp **pQ, struct bigDbSnp *item)
/* Add item to FIFO queue of overlapping items. */
{
// slAddTail is not the fastest, but I expect the Q to be empty most of the time, and to very
// rarely have more than 3 items.
slAddTail(pQ, item);
}

static void queueFlush(struct bigDbSnp **pQ, struct bigDbSnp *newItem, FILE *outF)
/* If any items at the head of Q do not overlap newItem then pop them from the queue,
 * print them out and free them.  If newItem is NULL then flush everything. */
{
struct bigDbSnp *bds, *nextBds;
for (bds = *pQ;  bds != NULL;  bds = nextBds)
    {
    nextBds = bds->next;
    if (newItem && bigDbSnpOverlap(bds, newItem))
        break;
    else
        {
        struct bigDbSnp *popped = slPopHead(pQ);
        if (popped != bds)
            {
            if (popped == NULL)
                errAbort("queueFlush: Expected to pop %s %s:%d:%d but popped NULL",
                         bds->name, bds->chrom, bds->chromStart, bds->chromEnd);
            else
                errAbort("queueFlush: Expected to pop %s %s:%d:%d but popped %s %s:%d:%d",
                         bds->name, bds->chrom, bds->chromStart, bds->chromEnd,
                         popped->name, popped->chrom, popped->chromStart, popped->chromEnd);
            }
        bigDbSnpTabOut(bds, outF);
        bigDbSnpFree(&bds);
        }
    }
}

static void appendNoteIfNecessary(struct bigDbSnp *bds, char *note, struct dyString *dy)
/* If bds->ucscNotes does not already include note, then add it using dy as scratch space. */
{
if (!stringIn(note, bds->ucscNotes))
    {
    dyStringClear(dy);
    dyStringAppend(dy, bds->ucscNotes);
    dyStringAppend(dy, note);
    dyStringAppendC(dy, ',');
    freeMem(bds->ucscNotes);
    bds->ucscNotes = cloneString(dy->string);
    }
}

static void checkOverlap(struct bigDbSnp *bdsList, struct dyString *dy)
/* Add ucscNotes flags to overlapping items in bdsList. */
{
// Compare previously queued items vs. only the new item, because they have already had a turn
// as the new item and have been compared to other queued overlapping items.
struct bigDbSnp *newItem = slLastEl(bdsList);
struct bigDbSnp *bds;
for (bds = bdsList;  bds != newItem;  bds = bds->next)
    {
    if (!bigDbSnpOverlap(bds, newItem))
        // Sometimes a large item holds up the queue a bit; skip over intervening non-overlapping
        // items.
        continue;
    if (bds->class != newItem->class)
        {
        appendNoteIfNecessary(bds, bdsOverlapDiffClass, dy);
        appendNoteIfNecessary(newItem, bdsOverlapDiffClass, dy);
        }
    else if (bds->chromStart == newItem->chromStart && bds->chromEnd == newItem->chromEnd)
        {
        appendNoteIfNecessary(bds, bdsClusterError, dy);
        appendNoteIfNecessary(newItem, bdsClusterError, dy);
        }
    else
        {
        appendNoteIfNecessary(bds, bdsOverlapSameClass, dy);
        appendNoteIfNecessary(newItem, bdsOverlapSameClass, dy);
        }
    }
}

struct hash *maybeReadMapErrIds()
/* If -mapErrIds was given, open the file and read rs# IDs into a hash. */
{
struct hash *mapErrIdHash = NULL;
char *mapErrIds = optionVal("mapErrIds", NULL);
if (mapErrIds)
    {
    struct lineFile *lf = lineFileOpen(mapErrIds, TRUE);
    char *line = NULL;
    mapErrIdHash = hashNew(18);
    while (lineFileNextReal(lf, &line))
        {
        if (!startsWith("rs", line))
            lineFileAbort(lf, "-mapErrIds: expected one rs# ID per line, got '%s'", line);
        char *p;
        for (p = line+2;  *p != 0;  p++)
            {
            if (! isdigit(*p))
                lineFileAbort(lf, "-mapErrIds: expected one rs# ID per line, got '%s'", line);
            }
        hashAddInt(mapErrIdHash, line, 1);
        }
    lineFileClose(&lf);
    }
return mapErrIdHash;
}

void checkBigDbSnp(char *bigDbSnpFile, char *twoBitFile, char *outFile)
/* checkBigDbSnp - Flag overlapping items and look for clustering errors. */
{
struct lineFile *lf = lineFileOpen(bigDbSnpFile, TRUE);
FILE *outF = mustOpen(outFile, "w");
struct hash *mapErrIdHash = maybeReadMapErrIds();
struct bigDbSnp *overlapQueue = NULL;
struct dyString *dy = dyStringNew(0);
char *line;
while (lineFileNextReal(lf, &line))
    {
    char *row[BIGDBSNP_NUM_COLS];
    int wordCount = chopTabs(line, row);
    lineFileExpectWords(lf, BIGDBSNP_NUM_COLS, wordCount);
    struct bigDbSnp *bds = bigDbSnpLoad(row);

    if (mapErrIdHash)
        checkMapErrIds(bds, mapErrIdHash);

    // check for overlapping items
    queueFlush(&overlapQueue, bds, outF);
    queueAdd(&overlapQueue, bds);
    checkOverlap(overlapQueue, dy);
    }
queueFlush(&overlapQueue, NULL, outF);
lineFileClose(&lf);
carefulClose(&outF);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
checkBigDbSnp(argv[1], argv[2], argv[3]);
return 0;
}
