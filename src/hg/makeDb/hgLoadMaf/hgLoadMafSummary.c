/* hgLoadMafSummary - Load a summary table of pairs in a maf into a database. */
#include "common.h"
#include "cheapcgi.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "hgRelate.h"
#include "portable.h"
#include "maf.h"
#include "dystring.h"
#include "mafSummary.h"

static char const rcsid[] = "$Id: hgLoadMafSummary.c,v 1.18 2008/09/03 19:19:43 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"mergeGap", OPTION_INT},
    {"minSize", OPTION_INT},
    {"maxSize", OPTION_INT},
    {"minSeqSize", OPTION_INT},
    {"test", OPTION_BOOLEAN},
    {NULL, 0}
};
boolean test = FALSE;
int mergeGap = 500;
int minSize = 10000;
int maxSize = 50000;
int minSeqSize = 1000000;
char *database = NULL;
long summaryCount = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
"hgLoadMafSummary - Load a summary table of pairs in a maf into a database\n"
  "usage:\n"
  "   hgLoadMafSummary database table file.maf\n"
  "options:\n"
    "   -mergeGap=N   max size of gap to merge regions (default %d)\n"
    "   -minSize=N         merge blocks smaller than N (default %d)\n"
    "   -maxSize=N         break up blocks larger than N (default %d)\n"
    "   -minSeqSize=N skip alignments when reference sequence is less than N\n"
    "                 (default %d -- match with hgTracks min window size for\n"
    "                 using summary table)\n"
    "   -test         suppress loading the database. Just create .tab file(s)\n"
    "                     in current dir.\n",
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
/* Take the score over the relevant range of text symbols in the maf,
 * and divide it by the bases we cover in the master genome to 
 * get a normalized by base score. */
score = mafScoreRangeMultiz(maf, 0, endT)/endB;

/* Scale the score so that it is between 0 and 1 */
score = (score - minScore) * scoreScale;
if (score < 0.0) score = 0.0;
if (score > 1.0) score = 1.0;
return score;
}

void outputSummary(FILE *f, struct mafSummary *ms)
/* output to .tab file */
{
fprintf(f, "%u\t", hFindBin(ms->chromStart, ms->chromEnd));
mafSummaryTabOut(ms, f);
summaryCount++;
}

double mergeScores(struct mafSummary *ms1, struct mafSummary *ms2)
/* determine score for merged maf summary blocks.
 * Compute weighted average of block scores vs. bases
 * ms1 is first summary block and ms2 is second positionally */
{
    double total = ms1->score * (ms1->chromEnd - ms1->chromStart) +
                        ms2->score * (ms2->chromEnd - ms2->chromStart);
    return total / (ms2->chromEnd - ms1->chromStart);
}

struct mafComp *mafMaster(struct mafAli *maf, struct mafFile *mf, 
                                        char *fileName)
/* Get master component from maf. Error abort if no master component */
{
char msg[256];
struct mafComp *mcMaster = mafMayFindCompPrefix(maf, database, ".");
if (mcMaster == NULL) 
    {
    safef(msg, sizeof(msg), "Couldn't find %s. sequence line %d of %s\n", 
                    database, mf->lf->lineIx, fileName);
    errAbort(msg);
    }
return mcMaster;
}

long processMaf(struct mafAli *maf, struct hash *componentHash, 
                FILE *f, struct mafFile *mf, char *fileName)
/* Compute scores for each pairwise component in the maf and output to .tab file */
{
struct mafComp *mc = NULL, *nextMc = NULL;
struct mafSummary *ms, *msPending;
struct mafAli pairMaf;
long componentCount = 0;
struct mafComp *mcMaster = mafMaster(maf, mf, fileName);
struct mafComp *oldMasterNext = mcMaster->next; 
char *e, *chrom;
char src[256];

strcpy(src, mcMaster->src);
chrom = chopPrefix(src);
for (mc = maf->components; mc != NULL; mc = nextMc)
    {
    nextMc = mc->next;
    if (sameString(mcMaster->src, mc->src) || mc->size == 0)
        continue;

    /* create maf summary for this alignment component */
    AllocVar(ms);
    ms->chrom = cloneString(chrom);
    /* both MAF and BED format define chromStart as 0-based */
    ms->chromStart = mcMaster->start;
    /* BED chromEnd is start+size */
    ms->chromEnd = mcMaster->start + mcMaster->size;
    ms->src = cloneString(mc->src);
    /* remove trailing components (following initial .) to src name */
    if ((e = strchr(ms->src, '.')) != NULL)
        *e = 0;

    /* construct pairwise maf for scoring */
    ZeroVar(&pairMaf);
    pairMaf.textSize = maf->textSize;
    pairMaf.components = mcMaster;
    mcMaster->next = mc;
    mc->next = NULL;
    ms->score = scorePairwise(&pairMaf);
    ms->leftStatus[0] = mc->leftStatus;
    ms->rightStatus[0] = mc->rightStatus;

    /* restore component links to allow memory recovery */
    mcMaster->next = oldMasterNext; 
    mc->next = nextMc;

    /* output to .tab file, or save for merging with another block 
     * if this one is too small */

    /* handle pending alignment block for this species, if any  */
    if ((msPending = (struct mafSummary *) hashFindVal(componentHash, ms->src)) != NULL)
        {
        /* there is a pending alignment block */
        /* either merge it with the current block, or output it */
        if (sameString(ms->chrom, msPending->chrom) &&
                    (ms->chromStart+1 - msPending->chromEnd < mergeGap))
            {
            /* merge pending block with current */
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
    /* handle current alignment block (possibly merged) */
    if (ms->chromEnd - ms->chromStart > minSize)
        {
        /* current block is big enough to output */
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
    {
    outputSummary(f, ms);
    }
}

void hgLoadMafSummary(char *db, char *table, char *fileName)
/* hgLoadMafSummary - Load a summary table of pairs in a maf into a database. */
{
long mafCount = 0;
struct mafComp *mcMaster = NULL;
struct mafAli *maf;
struct mafFile *mf = mafOpen(fileName);
struct sqlConnection *conn;
FILE *f = hgCreateTabFile(".", table);
long componentCount = 0;
struct hash *componentHash = newHash(0);

if (!test)
    {
    conn = sqlConnect(database);
    mafSummaryTableCreate(conn, table, hGetMinIndexLength(db));
    }
verbose(1, "Indexing and tabulating %s\n", fileName);

/* process mafs */
while ((maf = mafNext(mf)) != NULL)
    {
    mcMaster = mafMaster(maf, mf, fileName);
    if (mcMaster->srcSize < minSeqSize)
	continue;
    while (mcMaster->size > maxSize)
        {
        /* break maf into maxSize pieces */
        int end = mcMaster->start + maxSize;
        struct mafAli *subMaf = 
                mafSubset(maf, mcMaster->src, mcMaster->start, end);
        verbose(3, "Splitting maf %s:%d len %d\n", mcMaster->src,
                                        mcMaster->start, mcMaster->size);
        componentCount += 
            processMaf(subMaf, componentHash, f, mf, fileName);
        mafAliFree(&subMaf);
        subMaf = mafSubset(maf, mcMaster->src, 
                                end, end + (mcMaster->size - maxSize));
        mafAliFree(&maf);
        maf = subMaf;
        mcMaster = mafMaster(maf, mf, fileName);
        }
    if (mcMaster->size != 0)
        {
        /* remainder of maf after splitting off maxSize submafs */
        componentCount += 
            processMaf(maf, componentHash, f, mf, fileName);
        }
    mafAliFree(&maf);
    mafCount++;
    }
mafFileFree(&mf);
flushSummaryBlocks(componentHash, f);
verbose(1, 
    "Created %ld summary blocks from %ld components and %ld mafs from %s\n",
        summaryCount, componentCount, mafCount, fileName);
if (test)
    return;
verbose(1, "Loading into %s table %s...\n", database, table);
hgLoadTabFile(conn, ".", table, &f);
verbose(1, "Loading complete");
hgEndUpdate(&conn, "Add %ld maf summary blocks from %s\n", 
                        summaryCount, fileName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
test = optionExists("test");
mergeGap = optionInt("mergeGap", mergeGap);
minSize = optionInt("minSize", minSize);
maxSize = optionInt("maxSize", maxSize);
minSeqSize = optionInt("minSeqSize", minSeqSize);
if (argc != 4)
    usage();
database = argv[1];
hgLoadMafSummary(database, argv[2], argv[3]);
return 0;
}
