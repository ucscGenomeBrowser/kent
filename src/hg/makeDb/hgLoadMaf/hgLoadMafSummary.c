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

static char const rcsid[] = "$Id: hgLoadMafSummary.c,v 1.2 2005/03/08 17:11:09 kate Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"mergeGap", OPTION_INT},
    {"minSize", OPTION_INT},
    {"maxSize", OPTION_INT},
    {"test", OPTION_BOOLEAN},
    {NULL, 0}
};
boolean test = FALSE;
int mergeGap = 10;
int minSize = 1000;
int maxSize = 1000;
char *database = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
"hgLoadMafSummary - Load a summary table of pairs in a maf into a database\n"
  "usage:\n"
  "   hgLoadMafSummary database table file.maf\n"
  "options:\n"
    "   -mergeGap=N   max size of gap to merge regions (default 10)\n"
    "   -minSize=N         merge blocks smaller than N (default 1000)\n"
    "   -maxSize=N         break up blocks larger than N (default 1500)\n"
    "   -test         suppress loading the database. Just create .tab file(s)\n"
    "                     in current dir.\n" 
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

int processMaf(struct mafAli *maf, struct hash *componentHash, 
            struct mafSummary **summaryList, struct mafFile *mf, char *fileName)
/* Compute scores for each cmoponent in the maf and output to .tab file */
{
struct mafComp *mc = NULL, *nextMc = NULL;
struct mafSummary *ms, *msPending;
struct mafAli pairMaf;
long componentCount = 0;
long mafCount = 0;
struct mafComp *mcMaster = mafMaster(maf, mf, fileName);

for (mc = maf->components; mc != NULL; mc = nextMc)
    {
    nextMc = mc->next;
    if (sameString(mcMaster->src, mc->src))
        continue;

    /* create maf summary for this alignment component */
    AllocVar(ms);
    ms->chrom = chopPrefix(cloneString(mcMaster->src));
    /* both MAF and BED format define chromStart as 0-based */
    ms->chromStart = mcMaster->start;
    /* BED chromEnd is start+size */
    ms->chromEnd = mcMaster->start + mcMaster->size;
    ms->src = cloneString(mc->src);
    chopSuffix(ms->src);

    /* construct pairwise maf for scoring */
    ZeroVar(&pairMaf);
    pairMaf.textSize = maf->textSize;
    pairMaf.components = mcMaster;
    mcMaster->next = mc;
    mc->next = NULL;
    ms->score = scorePairwise(&pairMaf);

    /* output to .tab file, or save for merging with another block 
     * if this one is too small */

    /* handle pending alignment block for this species, if any  */
    if (((struct ms *)msPending = 
                hashFindVal(componentHash, ms->src)) != NULL)
        {
        /* there is a pending alignment block */
        /* either merge it with the current block, or output it */
        if (sameString(ms->chrom, msPending->chrom) &&
                    (ms->chromStart+1 - msPending->chromEnd < mergeGap))
            {
            /* merge pending block with current */
            ms->score = mergeScores(msPending, ms);
            ms->chromStart = msPending->chromStart;
            }
        else
            slAddHead(summaryList, msPending);
        hashRemove(componentHash, msPending->src);
        }
    /* handle current alignment block (possibly merged) */
    if (ms->chromEnd - ms->chromStart > minSize)
        /* current block is big enough to output */
        slAddHead(summaryList, ms);
    else
        hashAdd(componentHash, ms->src, ms);
    componentCount++;
    }
return componentCount;
}

void flushSummaryBlocks(struct hash *componentHash, 
                                struct mafSummary **summaryList)
/* flush any pending summary blocks */
{
struct mafSummary *ms;
struct hashCookie hc = hashFirst(componentHash);

while (((struct mafSummary *)ms = hashNextVal(&hc)) != NULL)
    slAddHead(summaryList, ms);
}

void hgLoadMafSummary(char *table, char *fileName)
/* hgLoadMafSummary - Load a summary table of pairs in a maf into a database. */
{
int i;
long mafCount = 0;
struct mafComp *mcMaster = NULL;
struct mafAli *maf;
int dbNameLen = strlen(database);
struct mafFile *mf = mafOpen(fileName);
struct sqlConnection *conn = sqlConnect(database);
FILE *f = hgCreateTabFile(".", table);
int maxComponents = 0;
int componentCount = 0;
struct hash *componentHash = newHash(0);
struct mafSummary *summaryList = NULL, *ms;

hSetDb(database);
if (!test)
    mafSummaryTableCreate(conn, table, hGetMinIndexLength());
verbose(1, "Indexing and tabulating %s\n", fileName);
mf = mafOpen(fileName);

/* process mafs */
while ((maf = mafNext(mf)) != NULL)
    {
    mcMaster = mafMaster(maf, mf, fileName);
    while (mcMaster->size > maxSize)
        {
        /* break maf into maxSize pieces */
        int end = mcMaster->start + maxSize;
        verbose(3, "Splitting maf %s:%d len %d\n", mcMaster->src,
                                        mcMaster->start, mcMaster->size);
        struct mafAli *subMaf = 
                mafSubset(maf, mcMaster->src, mcMaster->start, end);
        componentCount += 
            processMaf(subMaf, componentHash, &summaryList, mf, fileName);
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
            processMaf(maf, componentHash, &summaryList, mf, fileName);
        }
    mafAliFree(&maf);
    mafCount++;
    }
mafFileFree(&mf);
flushSummaryBlocks(componentHash, &summaryList);
slSort(&summaryList, bedLineCmp);
for (ms = summaryList; ms != NULL; ms = ms->next)
    {
    fprintf(f, "%u\t", hFindBin(ms->chromStart, ms->chromEnd));
    mafSummaryTabOut(ms, f);
    }
verbose(1, "Processed %ld components in %ld mafs from %s\n", 
                componentCount, mafCount, fileName);
if (test)
    return;
verbose(1, "Loading %s into database %s\n", table, database);
hgLoadTabFile(conn, ".", table, &f);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
test = optionExists("test");
mergeGap = optionInt("mergeGap", mergeGap);
minSize = optionInt("minSize", minSize);
maxSize = optionInt("maxSize", maxSize);
if (argc != 4)
    usage();
database = argv[1];
hgLoadMafSummary(argv[2], argv[3]);
return 0;
}
