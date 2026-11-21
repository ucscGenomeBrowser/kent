/* chainValidate - report obvious problems in a chain file. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "localmem.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
"chainValidate - report obvious problems in a chain file\n"
"usage:\n"
"   chainValidate in.chain\n"
"The file may be compressed.  Each problem is printed on one line to stdout.\n"
"The exit status is 1 if any problem was found, 0 otherwise.\n"
"Problems reported:\n"
"   - a chain header with the wrong number of fields or a field that is not a number\n"
"   - a target strand other than + or a query strand other than + or -\n"
"   - a start that is not before the end, or an end past the sequence size\n"
"   - a sequence size that differs between chains, or from a chrom.sizes file\n"
"   - block lines that do not add up to the start and end in the header\n"
"   - a block size that is not positive, or a negative gap\n"
"   - a duplicate chain ID\n"
"   - a chain that starts exactly where another chain ends on both assemblies,\n"
"     on the same strand.  Such chains should be one chain.\n"
"options:\n"
"   -tSizes=target.chrom.sizes - check target sequence sizes against this file\n"
"   -qSizes=query.chrom.sizes - check query sequence sizes against this file\n"
);
}

static struct optionSpec options[] = {
    {"tSizes", OPTION_STRING},
    {"qSizes", OPTION_STRING},
    {NULL, 0},
};

struct chainInfo
/* What we keep about one chain after reading it. */
    {
    struct chainInfo *next;
    char *id;                   /* Chain ID as written, or "?" if missing. */
    int line;                   /* Line number of the chain header. */
    boolean headerOk;           /* TRUE if the header parsed and passed its own checks. */
    char *tName, *qName;        /* Sequence names. */
    char qStrand;               /* + or - */
    long long tStart, tEnd, tSize;
    long long qStart, qEnd, qSize;
    long long tSum, qSum;       /* Where the blocks so far end, from tStart and qStart. */
    int blockCount;             /* Number of block lines read. */
    boolean blocksOk;           /* FALSE if any block line was bad. */
    boolean ended;              /* TRUE after the one-number last block line. */
    };

static int problemCount = 0;

static void problem(struct lineFile *lf, int line, struct chainInfo *ci, char *format, ...)
/* Print one problem line and count it. */
#if defined(__GNUC__)
__attribute__((format(printf, 4, 5)))
#endif
;

static void problem(struct lineFile *lf, int line, struct chainInfo *ci, char *format, ...)
/* Print one problem line and count it. */
{
va_list args;
va_start(args, format);
printf("%s line %d", lf->fileName, line);
if (ci != NULL)
    printf(" chain %s", ci->id);
printf(": ");
vprintf(format, args);
printf("\n");
va_end(args);
problemCount++;
}

static boolean parseCount(char *s, long long *ret)
/* Parse a number that may not be negative.  Return FALSE if s is not one. */
{
if (!isAllDigits(s) || strlen(s) > 15)
    return FALSE;
*ret = atoll(s);
return TRUE;
}

static boolean parseSigned(char *s, long long *ret)
/* Parse a number that may be negative.  Return FALSE if s is not one. */
{
char *digits = (*s == '-') ? s + 1 : s;
if (!isAllDigits(digits) || strlen(digits) > 15)
    return FALSE;
*ret = atoll(s);
return TRUE;
}

static void checkSize(struct lineFile *lf, struct chainInfo *ci, char *side, char *name,
    long long size, struct hash *sizesFromFile, struct hash *sizesSeen, struct lm *lm)
/* Check one sequence size against the chrom.sizes file, if given, and against the
 * size the first chain on this sequence gave. */
{
if (sizesFromFile != NULL)
    {
    struct hashEl *hel = hashLookup(sizesFromFile, name);
    if (hel == NULL)
        problem(lf, ci->line, ci, "%s sequence %s is not in the %s sizes file",
            side, name, side);
    else if (ptToInt(hel->val) != size)
        problem(lf, ci->line, ci, "%s size of %s is %lld, the sizes file says %d",
            side, name, size, ptToInt(hel->val));
    }
long long *seen = hashFindVal(sizesSeen, name);
if (seen == NULL)
    {
    lmAllocVar(lm, seen);
    *seen = size;
    hashAdd(sizesSeen, name, seen);
    }
else if (*seen != size)
    problem(lf, ci->line, ci, "%s size of %s is %lld, an earlier chain said %lld",
        side, name, size, *seen);
}

static struct chainInfo *parseHeader(struct lineFile *lf, char **row, int wordCount,
    struct hash *tSizes, struct hash *qSizes, struct hash *tSeen, struct hash *qSeen,
    struct hash *idHash, struct lm *lm)
/* Parse and check a chain header line.  Always returns a chainInfo, so the block
 * lines after a bad header are still read and skipped. */
{
struct chainInfo *ci;
lmAllocVar(lm, ci);
ci->line = lf->lineIx;
ci->id = "?";
ci->blocksOk = TRUE;
if (wordCount == 13)
    ci->id = lmCloneString(lm, row[12]);
if (wordCount != 12 && wordCount != 13)
    {
    problem(lf, ci->line, NULL, "chain header has %d fields, expecting 12 or 13", wordCount);
    return ci;
    }
boolean ok = TRUE;
if (wordCount == 13)
    {
    long long id;
    if (!parseCount(row[12], &id))
        {
        problem(lf, ci->line, ci, "chain ID is not a number");
        ok = FALSE;
        }
    int firstLine = hashIntValDefault(idHash, ci->id, 0);
    if (firstLine != 0)
        problem(lf, ci->line, ci, "duplicate chain ID, first used on line %d", firstLine);
    else
        hashAddInt(idHash, ci->id, ci->line);
    }
if (!parseCount(row[3], &ci->tSize) || !parseCount(row[5], &ci->tStart)
    || !parseCount(row[6], &ci->tEnd))
    {
    problem(lf, ci->line, ci, "target size, start or end is not a number");
    ok = FALSE;
    }
if (!parseCount(row[8], &ci->qSize) || !parseCount(row[10], &ci->qStart)
    || !parseCount(row[11], &ci->qEnd))
    {
    problem(lf, ci->line, ci, "query size, start or end is not a number");
    ok = FALSE;
    }
if (!sameString(row[4], "+"))
    {
    problem(lf, ci->line, ci, "target strand is '%s', expecting +", row[4]);
    ok = FALSE;
    }
if (!sameString(row[9], "+") && !sameString(row[9], "-"))
    {
    problem(lf, ci->line, ci, "query strand is '%s', expecting + or -", row[9]);
    ok = FALSE;
    }
if (!ok)
    return ci;
ci->tName = lmCloneString(lm, row[2]);
ci->qName = lmCloneString(lm, row[7]);
ci->qStrand = row[9][0];
if (ci->tStart >= ci->tEnd)
    {
    problem(lf, ci->line, ci, "target start %lld is not before target end %lld",
        ci->tStart, ci->tEnd);
    ok = FALSE;
    }
if (ci->qStart >= ci->qEnd)
    {
    problem(lf, ci->line, ci, "query start %lld is not before query end %lld",
        ci->qStart, ci->qEnd);
    ok = FALSE;
    }
if (ci->tEnd > ci->tSize)
    {
    problem(lf, ci->line, ci, "target end %lld is past the end of %s (size %lld)",
        ci->tEnd, ci->tName, ci->tSize);
    ok = FALSE;
    }
if (ci->qEnd > ci->qSize)
    {
    problem(lf, ci->line, ci, "query end %lld is past the end of %s (size %lld)",
        ci->qEnd, ci->qName, ci->qSize);
    ok = FALSE;
    }
checkSize(lf, ci, "target", ci->tName, ci->tSize, tSizes, tSeen, lm);
checkSize(lf, ci, "query", ci->qName, ci->qSize, qSizes, qSeen, lm);
ci->tSum = ci->tStart;
ci->qSum = ci->qStart;
ci->headerOk = ok;
return ci;
}

static void parseBlock(struct lineFile *lf, char **row, int wordCount, struct chainInfo *ci)
/* Parse and check one block line of the current chain. */
{
if (ci->ended)
    {
    problem(lf, lf->lineIx, ci, "block line after the last block (the one with one number)");
    ci->blocksOk = FALSE;
    return;
    }
ci->blockCount++;
if (wordCount != 1 && wordCount != 3)
    {
    problem(lf, lf->lineIx, ci, "block line has %d fields, expecting 1 or 3", wordCount);
    ci->blocksOk = FALSE;
    return;
    }
long long size, dt = 0, dq = 0;
if (!parseSigned(row[0], &size) || (wordCount == 3 &&
    (!parseSigned(row[1], &dt) || !parseSigned(row[2], &dq))))
    {
    problem(lf, lf->lineIx, ci, "block line has a field that is not a number");
    ci->blocksOk = FALSE;
    return;
    }
if (size <= 0)
    {
    problem(lf, lf->lineIx, ci, "block size %lld is not positive", size);
    ci->blocksOk = FALSE;
    }
if (dt < 0 || dq < 0)
    {
    problem(lf, lf->lineIx, ci, "negative gap (target %lld, query %lld)", dt, dq);
    ci->blocksOk = FALSE;
    }
ci->tSum += size + dt;
ci->qSum += size + dq;
if (wordCount == 1)
    ci->ended = TRUE;
}

static void finishChain(struct lineFile *lf, struct chainInfo *ci, struct chainInfo **pGoodList)
/* Check that the blocks of a chain agree with its header.  Add the chain to the
 * good list if they do. */
{
if (ci->blockCount == 0)
    {
    problem(lf, ci->line, ci, "chain has no block lines");
    return;
    }
if (!ci->ended)
    {
    problem(lf, ci->line, ci, "last block line has more than one number");
    ci->blocksOk = FALSE;
    }
if (!ci->headerOk || !ci->blocksOk)
    return;
boolean ok = TRUE;
if (ci->tSum != ci->tEnd)
    {
    problem(lf, ci->line, ci, "blocks end at target %lld, the header says %lld",
        ci->tSum, ci->tEnd);
    ok = FALSE;
    }
if (ci->qSum != ci->qEnd)
    {
    problem(lf, ci->line, ci, "blocks end at query %lld, the header says %lld",
        ci->qSum, ci->qEnd);
    ok = FALSE;
    }
if (ok)
    slAddHead(pGoodList, ci);
}

static void joinKey(char *buf, int bufSize, struct chainInfo *ci, long long tPos, long long qPos)
/* Make a hash key from the two sequences, the query strand and a position on each. */
{
safef(buf, bufSize, "%s\t%lld\t%s\t%c\t%lld", ci->tName, tPos, ci->qName, ci->qStrand, qPos);
}

static void checkAbutting(struct lineFile *lf, struct chainInfo *list)
/* Report each chain that starts exactly where another chain ends on both assemblies. */
{
struct hash *endHash = hashNew(20);
struct chainInfo *ci;
char key[1024];
for (ci = list; ci != NULL; ci = ci->next)
    {
    joinKey(key, sizeof key, ci, ci->tEnd, ci->qEnd);
    hashAdd(endHash, key, ci);
    }
for (ci = list; ci != NULL; ci = ci->next)
    {
    joinKey(key, sizeof key, ci, ci->tStart, ci->qStart);
    struct hashEl *hel;
    for (hel = hashLookup(endHash, key); hel != NULL; hel = hashLookupNext(hel))
        {
        struct chainInfo *prev = hel->val;
        problem(lf, ci->line, ci, "starts where chain %s (line %d) ends, at target %s:%lld "
            "and query %s:%lld (%c strand); they should be one chain",
            prev->id, prev->line, ci->tName, ci->tStart, ci->qName, ci->qStart, ci->qStrand);
        }
    }
hashFree(&endHash);
}

void chainValidate(char *inFile, char *tSizesFile, char *qSizesFile)
/* chainValidate - report obvious problems in a chain file. */
{
struct hash *tSizes = (tSizesFile != NULL) ? hashNameIntFile(tSizesFile) : NULL;
struct hash *qSizes = (qSizesFile != NULL) ? hashNameIntFile(qSizesFile) : NULL;
struct hash *tSeen = hashNew(0);
struct hash *qSeen = hashNew(0);
struct hash *idHash = hashNew(20);
struct lm *lm = lmInit(0);
struct lineFile *lf = lineFileOpen(inFile, TRUE);
struct chainInfo *ci = NULL, *goodList = NULL;
int chainCount = 0;
char *line;
char *row[14];
while (lineFileNextReal(lf, &line))
    {
    int wordCount = chopByWhite(line, row, ArraySize(row));
    if (sameString(row[0], "chain"))
        {
        if (ci != NULL)
            finishChain(lf, ci, &goodList);
        ci = parseHeader(lf, row, wordCount, tSizes, qSizes, tSeen, qSeen, idHash, lm);
        chainCount++;
        }
    else if (ci == NULL)
        problem(lf, lf->lineIx, NULL, "line before the first chain header");
    else
        parseBlock(lf, row, wordCount, ci);
    }
if (ci != NULL)
    finishChain(lf, ci, &goodList);
slReverse(&goodList);
checkAbutting(lf, goodList);
verbose(1, "%d chains, %d problems\n", chainCount, problemCount);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
chainValidate(argv[1], optionVal("tSizes", NULL), optionVal("qSizes", NULL));
return (problemCount > 0) ? 1 : 0;
}
