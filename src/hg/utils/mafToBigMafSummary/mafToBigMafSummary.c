/* mafToBigMafSummary - Convert a maf into bed3+4 input ready for bedToBigBed
 * to make a bigMaf summary (.bb) file.  Companion to mafToBigMaf. */

/* The summary scoring/merging logic in this file is deliberately duplicated
 * from hgLoadMafSummary.c rather than refactored into a shared library.
 * That code is stable, hgLoadMafSummary is widely referenced from existing
 * makedocs, and a refactor would force retesting all of those pipelines for
 * minimal payoff.  If you change one, change the other. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "maf.h"
#include "mafSummary.h"

static struct optionSpec optionSpecs[] = {
    {"mergeGap", OPTION_INT},
    {"minSize", OPTION_INT},
    {"maxSize", OPTION_INT},
    {"minSeqSize", OPTION_INT},
    {NULL, 0}
};

int mergeGap = 500;
int minSize = 10000;
int maxSize = 50000;
int minSeqSize = 1;
char *referenceDb = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
"mafToBigMafSummary - Convert a maf into the bed3+4 input for a bigMaf summary file\n"
  "usage:\n"
  "   mafToBigMafSummary referenceDb input.maf out.bed\n"
  "Pipe the output through 'sort -k1,1 -k2,2n', then run bedToBigBed:\n"
  "   bedToBigBed -type=bed3+4 -as=mafSummary.as -tab out.sorted.bed \\\n"
  "       referenceDb.chrom.sizes bigMafSummary.bb\n"
  "options:\n"
  "   -mergeGap=N   max size of gap to merge regions (default %d)\n"
  "   -minSize=N    merge blocks smaller than N (default %d)\n"
  "   -maxSize=N    break up blocks larger than N (default %d)\n"
  "   -minSeqSize=N skip alignments when reference sequence is less than N\n"
  "                 (default %d)\n",
mergeGap, minSize, maxSize, minSeqSize
  );
}

double scorePairwise(struct mafAli *maf)
/* generate score from 0.0 to 1.0 for an alignment pair */
/* Adapted from multiz scoring in hgTracks/mafTrack.c */
{
int endB;       /* end in the reference (master) genome */
int deltaB;
int endT;       /* end in the master genome maf text (includes gaps) */
double score;
double minScore = -100.0, maxScore = 100.0;
double scoreScale = 1.0 / (maxScore - minScore);
struct mafComp *mcMaster = maf->components;

endB = mcMaster->size;
deltaB = endB;
for (endT = 0; endT < maf->textSize; endT++)
    {
    if (deltaB <= 0)
        break;
    if (mcMaster->text[endT] != '-')
        deltaB -= 1;
    }
score = mafScoreRangeMultiz(maf, 0, endT)/endB;

score = (score - minScore) * scoreScale;
if (score < 0.0) score = 0.0;
if (score > 1.0) score = 1.0;
return score;
}

void outputSummary(FILE *f, struct mafSummary *ms)
/* Output a single summary block as bed3+4 (no SQL bin column). */
{
mafSummaryTabOut(ms, f);
}

double mergeScores(struct mafSummary *ms1, struct mafSummary *ms2)
/* Weighted-average score for two adjacent summary blocks (ms1 first). */
{
double total = ms1->score * (ms1->chromEnd - ms1->chromStart) +
               ms2->score * (ms2->chromEnd - ms2->chromStart);
return total / (ms2->chromEnd - ms1->chromStart);
}

struct mafComp *mafMaster(struct mafAli *maf, struct mafFile *mf, char *fileName)
/* Get master component from maf. errAbort if not found. */
{
struct mafComp *mcMaster = mafMayFindCompPrefix(maf, referenceDb, ".");
if (mcMaster == NULL)
    errAbort("Couldn't find %s. sequence line %d of %s\n",
             referenceDb, mf->lf->lineIx, fileName);
return mcMaster;
}

char *mafSplitSrcGetChrom(char *src, char *database)
/* src can be in format chrom, db|chrom or db.chrom: split string on separator and return pointer to chrom.
 * If database is non-NULL and src starts with "database.", strip exactly that prefix
 * (so a database name like GCF_1234.3 with its own dot is handled correctly).
 * Side effect: src is truncated at the separator (so it becomes just the db). */
{
char *pipe = strchr(src, '|');
if (pipe)
    {
    *pipe = '\0';
    return pipe + 1;
    }

if (database != NULL)
    {
    int dbLen = strlen(database);
    if (strncmp(src, database, dbLen) == 0 && src[dbLen] == '.')
        {
        src[dbLen] = '\0';
        return src + dbLen + 1;
        }
    }

char *dot = strchr(src, '.');
if (!dot)
    return src;
*dot = '\0';
return dot + 1;
}

long processMaf(struct mafAli *maf, struct hash *componentHash,
                FILE *f, struct mafFile *mf, char *fileName)
/* Compute scores for each pairwise component in the maf and emit summary blocks. */
{
struct mafComp *mc = NULL, *nextMc = NULL;
struct mafSummary *ms, *msPending;
struct mafAli pairMaf;
long componentCount = 0;
struct mafComp *mcMaster = mafMaster(maf, mf, fileName);
struct mafComp *oldMasterNext = mcMaster->next;
char *chrom;
char src[256];

strcpy(src, mcMaster->src);
chrom = mafSplitSrcGetChrom(src, referenceDb);

for (mc = maf->components; mc != NULL; mc = nextMc)
    {
    nextMc = mc->next;
    if (sameString(mcMaster->src, mc->src) || mc->size == 0)
        continue;

    AllocVar(ms);
    ms->chrom = cloneString(chrom);
    ms->chromStart = mcMaster->start;
    ms->chromEnd = mcMaster->start + mcMaster->size;
    ms->src = cloneString(mc->src);
    mafSplitSrcGetChrom(ms->src, referenceDb);

    ZeroVar(&pairMaf);
    pairMaf.textSize = maf->textSize;
    pairMaf.components = mcMaster;
    mcMaster->next = mc;
    mc->next = NULL;
    ms->score = scorePairwise(&pairMaf);
    ms->leftStatus[0] = mc->leftStatus;
    ms->rightStatus[0] = mc->rightStatus;

    mcMaster->next = oldMasterNext;
    mc->next = nextMc;

    if ((msPending = (struct mafSummary *) hashFindVal(componentHash, ms->src)) != NULL)
        {
        if (sameString(ms->chrom, msPending->chrom) &&
            (ms->chromStart+1 - msPending->chromEnd < mergeGap))
            {
            ms->score = mergeScores(msPending, ms);
            ms->chromStart = msPending->chromStart;
            ms->leftStatus[0] = msPending->leftStatus[0];
            ms->rightStatus[0] = ms->rightStatus[0];
            }
        else
            outputSummary(f, msPending);
        hashRemove(componentHash, msPending->src);
        mafSummaryFree(&msPending);
        }
    if (ms->chromEnd - ms->chromStart > minSize)
        {
        outputSummary(f, ms);
        mafSummaryFree(&ms);
        }
    else
        hashAdd(componentHash, ms->src, ms);
    componentCount++;
    }
return componentCount;
}

void flushSummaryBlocks(struct hash *componentHash, FILE *f)
/* flush any pending summary blocks */
{
struct mafSummary *ms;
struct hashCookie hc = hashFirst(componentHash);

while ((ms = (struct mafSummary *)hashNextVal(&hc)) != NULL)
    outputSummary(f, ms);
}

void mafToBigMafSummary(char *db, char *inMaf, char *outBed)
/* mafToBigMafSummary - emit summary blocks for the maf as bed3+4. */
{
long mafCount = 0, allMafCount = 0;
struct mafComp *mcMaster = NULL;
struct mafAli *maf;
struct mafFile *mf = mafOpen(inMaf);
FILE *f = mustOpen(outBed, "w");
long componentCount = 0;
struct hash *componentHash = newHash(0);

verbose(1, "Indexing and tabulating %s\n", inMaf);

while ((maf = mafNext(mf)) != NULL)
    {
    mcMaster = mafMaster(maf, mf, inMaf);
    allMafCount++;
    if (mcMaster->srcSize < minSeqSize)
        continue;
    while (mcMaster->size > maxSize)
        {
        int end = mcMaster->start + maxSize;
        struct mafAli *subMaf =
                mafSubset(maf, mcMaster->src, mcMaster->start, end);
        verbose(3, "Splitting maf %s:%d len %d\n", mcMaster->src,
                mcMaster->start, mcMaster->size);
        componentCount +=
            processMaf(subMaf, componentHash, f, mf, inMaf);
        mafAliFree(&subMaf);
        subMaf = mafSubset(maf, mcMaster->src,
                           end, end + (mcMaster->size - maxSize));
        mafAliFree(&maf);
        maf = subMaf;
        mcMaster = mafMaster(maf, mf, inMaf);
        }
    if (mcMaster->size != 0)
        componentCount +=
            processMaf(maf, componentHash, f, mf, inMaf);
    mafAliFree(&maf);
    mafCount++;
    }
mafFileFree(&mf);
flushSummaryBlocks(componentHash, f);
carefulClose(&f);
verbose(1, "%ld components, %ld mafs from %s\n",
        componentCount, allMafCount, inMaf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
mergeGap = optionInt("mergeGap", mergeGap);
minSize = optionInt("minSize", minSize);
maxSize = optionInt("maxSize", maxSize);
minSeqSize = optionInt("minSeqSize", minSeqSize);
if (argc != 4)
    usage();
referenceDb = argv[1];
mafToBigMafSummary(referenceDb, argv[2], argv[3]);
return 0;
}
