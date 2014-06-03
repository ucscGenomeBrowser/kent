/* motifFinder - find largest scoring motif in bed items. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "bed6FloatScore.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "dnaMotif.h"
#include "dnaMotifSql.h"
#include "dnaMarkov.h"
#include "jksql.h"
#include "hdb.h"
#include "chromInfo.h"

static float minScoreCutoff;
static char *motifTable = "transRegCodeMotifPseudoCounts";
static char *markovTable;
static boolean originalCoordinates = FALSE;
static boolean topOnly = FALSE;
static boolean uniformBackground = FALSE;
struct chromInfo *chromInfo;
static float prior = 0.5;                 // our prior belief that there is at least one binding site in a peak
static float priorBackoff = 0.01;         // our prior belief that there are > 1 binding sites in a given peak = PRIOR * priorBackoff^n)

void usage()
/* Explain usage and exit. */
{
errAbort(
  "motifFinder - find largest scoring motif in bed items\n"
  "factorName is loaded from table '%s'.\n"
  "usage:\n"
  "   motifFinder assembly factorName file(s)\n"
  "options:\n"
  "   -markovTable\t\tUse given 2nd-markov model for background\n"
  "   -originalCoordinates\tPrint original coordinates, rather than just the coordinates of the highest scoring motif\n",
  motifTable
  );
}

static struct optionSpec options[] = {
    {"markovTable", OPTION_STRING},
    {"minScoreCutoff", OPTION_FLOAT},
    {"originalCoordinates", OPTION_BOOLEAN},
    {"prior", OPTION_FLOAT},
    {"priorBackoff", OPTION_FLOAT},
    {"topOnly", OPTION_BOOLEAN},
    {"uniformBackground", OPTION_BOOLEAN},
    {NULL, 0},
};

static unsigned getChromSize(struct chromInfo *ci, char *chrom)
{
// XXXX use hash to speedup?
for(; ci != NULL; ci = ci->next)
    {
    if(sameString(ci->chrom, chrom))
        return ci->size;
    }
errAbort("couldn't find getChromSize for chrom '%s'", chrom);
return 0;
}

static int bed6FloatCmpDesc(const void *va, const void *vb)
/* Compare two floats (remember that we have to return an int) */
{
const struct bed6FloatScore *a = *((struct bed6FloatScore **)va);
const struct bed6FloatScore *b = *((struct bed6FloatScore **)vb);
return (int) (1000 * (b->score - a->score));
}

void motifFinder(char *database, char *name, int fileCount, char *files[])
/* motifFinder - find largest scoring motif in bed items. */
{
struct sqlConnection *conn = sqlConnect(database);
int fileNum;
char where[256];
struct chromInfo *ci  = createChromInfoList(NULL, database);
sqlSafefFrag(where, sizeof(where), "name = '%s'", name);
struct dnaMotif *motif = dnaMotifLoadWhere(conn, motifTable, where);
if(markovTable != NULL)
    dnaMotifMakeLog2(motif);
if(motif == NULL)
    errAbort("couldn't find motif '%s'", name);
for (fileNum = 0; fileNum < fileCount; fileNum++)
    {
    char *words[64], *line;
    char **row;
    struct lineFile *lf = lineFileOpen(files[fileNum], TRUE);
    while (lineFileNextReal(lf, &line))
        {
	int dnaLength, i, j, rowOffset, length, wordCount = chopTabs(line, words);
        unsigned chromSize;
        boolean markovFound = FALSE;
        double mark0[5];
        double mark2[5][5][5];
        struct dnaSeq *seq = NULL;
        char *dupe = NULL;
        if (0 == wordCount)
            continue;
        lineFileExpectAtLeast(lf, 3, wordCount);
        dupe = cloneString(line);
        char *chrom = words[0];
        int chromStart = lineFileNeedNum(lf, words, 1);
        if(markovTable != NULL)
            chromStart = max(2, chromStart);
        unsigned chromEnd = lineFileNeedNum(lf, words, 2);
        if (chromEnd < 1)
            errAbort("ERROR: line %d:'%s'\nchromEnd is less than 1\n",
		     lf->lineIx, dupe);
        if (chromStart > chromEnd)
            errAbort("ERROR: line %d:'%s'\nchromStart after chromEnd (%d > %d)\n",
                     lf->lineIx, dupe, chromStart, chromEnd);
        length = chromEnd - chromStart;
        chromSize = getChromSize(ci, chrom);
        if(markovTable == NULL)
            {
            dnaLength = length;
            seq = hDnaFromSeq(database, chrom, chromStart, chromEnd, dnaUpper);
            if(uniformBackground)
                {
                int i;
                mark0[0] = 1;
                for(i = 1; i <= 4; i++)
                    mark0[i] = 0.25;
                }
            else
                {
                dnaMark0(seq, mark0, NULL);
                }
            }
        else
            {
            dnaLength = length + 4;
            if(chromStart - 2 + dnaLength > chromSize)
                // can't do analysis for potential peak hanging off the end of the chrom
                continue;
            seq = hDnaFromSeq(database, chrom, chromStart - 2, chromEnd + 2, dnaUpper);
            struct sqlResult *sr = hRangeQuery(conn, markovTable, chrom, chromStart,
                                               chromStart + 1, NULL, &rowOffset);
            if((row = sqlNextRow(sr)) != NULL)
                {
                dnaMark2Deserialize(row[rowOffset + 3], mark2);
                dnaMarkMakeLog2(mark2);
                markovFound = TRUE;
                }
            else
                errAbort("markov table '%s' is missing; non-markov analysis is current not supported", markovTable);
            sqlFreeResult(&sr);
            }
        struct bed6FloatScore *hits = NULL;
        for (i = 0; i < 2; i++)
            {
            double mark0Copy[5];
            char strand = i == 0 ? '+' : '-';
            for (j = 0; j <= 4; j++)
                mark0Copy[j] = mark0[j];
            if(strand == '-')
                {
                // reverse markov table too!
                double tmp;
                reverseComplement(seq->dna, dnaLength);
                tmp = mark0Copy[1];
                mark0Copy[1] = mark0Copy[3];
                mark0Copy[3] = tmp;
                tmp = mark0Copy[2];
                mark0Copy[2] = mark0Copy[4];
                mark0Copy[4] = tmp;
                }
            for (j = 0; j < length - motif->columnCount + 1; j++)
                // tricky b/c if(markovFound) then seq includes the two bytes on either side of actual sequence.
                {
                double score;
                if(markovFound)
                    score = dnaMotifBitScoreWithMarkovBg(motif, seq->dna + j, mark2);
                else
                    score = dnaMotifBitScoreWithMark0Bg(motif, seq->dna + j, mark0Copy);
                if(score >= minScoreCutoff)
                    {
                    int start;
                    if(strand == '-')
                        start = (chromEnd - j) - motif->columnCount;
                    else
                        start = chromStart + j;
                    struct bed6FloatScore *hit = NULL;

                    // Watch out for overlapping hits (on either strand; yes, I've seen that happen);
                    // we report only the highest scoring hit in this case.
                    // O(n^2) where n == number of motifs in a peak, but I expect n to be almost always very small.
                    if(!originalCoordinates)
                        {
                        for (hit = hits; hit != NULL; hit = hit->next)
                            {
                            if(hit->chromEnd > start && hit->chromStart <= (start + motif->columnCount))
                                {
                                verbose(3, "found overlapping hits: %d-%d overlaps with %d-%d\n", start, start + motif->columnCount, hit->chromStart, hit->chromEnd);
                                break;
                                }
                            }
                        }
                    if(hit == NULL || hit->score < score)
                        {
                        if(hit == NULL)
                            {
                            AllocVar(hit);
                            slAddHead(&hits, hit);
                            hit->chrom = cloneString(chrom);
                            }
                        hit->chromStart = originalCoordinates ? chromStart : start;
                        hit->chromEnd = originalCoordinates ? chromEnd : start + motif->columnCount;
                        hit->score = score;
                        hit->strand[0] = strand;
                        }
                    }
                verbose(3, "j: %d; score: %.2f\n", j, score);
                }
            }
        slSort(&hits, bed6FloatCmpDesc);
        int count;
        float currentPrior = prior;
        for(count = 1; hits != NULL; count++, hits = hits->next)
            {
            if(topOnly && count > topOnly)
                break;
            // Use a progressively weaker prior for hits with lower scores
            verbose(3, "count: %d; score: %.2f; prior: %.2f; log2(prior / (1 - prior)): %.2f\n", count, hits->score, currentPrior, log2(currentPrior / (1 - currentPrior)));
            if(hits->score >= minScoreCutoff - log2(currentPrior / (1 - currentPrior)))
                {
                printf("%s\t%d\t%d\t%s\t%.2f\t%c\n", chrom, originalCoordinates ? chromStart : hits->chromStart, 
                       originalCoordinates ? chromEnd : hits->chromStart + motif->columnCount, name, hits->score, hits->strand[0]);
                currentPrior = count == 1 ? priorBackoff : currentPrior * priorBackoff;
                if(count > 2)
                    verbose(3, "hit for count: %d at %s:%d-%d\n", count, chrom, hits->chromStart, hits->chromStart + motif->columnCount);
                }
            else
                break;
            }
        freeDnaSeq(&seq);
        freeMem(dupe);
        }
    lineFileClose(&lf);
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
markovTable = optionVal("markovTable", NULL);
originalCoordinates = optionExists("originalCoordinates");
minScoreCutoff = optionFloat("minScoreCutoff", 0);
prior = optionFloat("prior", prior);
priorBackoff = optionFloat("priorBackoff", priorBackoff);
topOnly = optionExists("topOnly");
uniformBackground = optionExists("uniformBackground");
if (argc < 4)
    usage();
motifFinder(argv[1], argv[2], argc-3, argv+3);
return 0;
}
