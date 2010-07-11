/* motifFinder - find largest scoring motif in bed items. */

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
static char *markovTable = "markovModels";
static boolean originalCoordinates = FALSE;
static int topOnly = 0;
struct chromInfo *chromInfo;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "motifFinder - find largest scoring motif in bed items\n"
  "Uses 2nd-markov model for background if table '%s' is available in this assembly.\n"
  "factorName is loaded from table '%s'.\n"
  "usage:\n"
  "   motifFinder assembly factorName file(s)\n"
  "options:\n"
  "   -originalCoordinates\tprint original coordinates, rather than just the coordinates of the highest scoring motif.\n",
  markovTable, motifTable
  );
}

static struct optionSpec options[] = {
    {"originalCoordinates", OPTION_BOOLEAN},
    {"topOnly", OPTION_INT},
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
safef(where, sizeof(where), "name = '%s'", name);
struct dnaMotif *motif = dnaMotifLoadWhere(conn, motifTable, where);
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
        double mark2[5][5][5];
        struct dnaSeq *seq = NULL;
        char *dupe = NULL;
        if (0 == wordCount)
            continue;
        lineFileExpectAtLeast(lf, 3, wordCount);
        dupe = cloneString(line);
        char *chrom = words[0];
        int chromStart = lineFileNeedNum(lf, words, 1);
        chromStart = max(2, chromStart);
        unsigned chromEnd = lineFileNeedNum(lf, words, 2);
        if (chromEnd < 1)
            errAbort("ERROR: line %d:'%s'\nchromEnd is less than 1\n",
		     lf->lineIx, dupe);
        if (chromStart > chromEnd)
            errAbort("ERROR: line %d:'%s'\nchromStart after chromEnd (%d > %d)\n",
                     lf->lineIx, dupe, chromStart, chromEnd);
        length = chromEnd - chromStart;
        dnaLength = length + 4;
        chromSize = getChromSize(ci, chrom);
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
        struct bed6FloatScore *hits = NULL;
        for (i = 0; i < 2; i++)
            {
            char strand = i == 0 ? '+' : '-';
            if(strand == '-')
                reverseComplement(seq->dna, dnaLength);
            for (j = 0; j < length - motif->columnCount + 1; j++)
                // tricky b/c j includes the two bytes on either side of actual sequence.
                {
                double score = dnaMotifBitScoreWithMarkovBg(motif, seq->dna + j, mark2);
                if(score >= minScoreCutoff)
                    {
                    int start;
                    struct bed6FloatScore *hit;
                    AllocVar(hit);
                    if(strand == '-')
                        start = (chromEnd - j) - motif->columnCount;
                    else
                        start = chromStart + j;
                    hit->chrom = cloneString(chrom);
                    hit->chromStart = originalCoordinates ? chromStart : start;
                    hit->chromEnd = originalCoordinates ? chromEnd : start + motif->columnCount;
                    hit->score = score;
                    hit->strand[0] = strand;
                    slAddHead(&hits, hit);
                    }
                verbose(3, "j: %d; score: %.2f\n", j, score);
                }
            }
        if(topOnly)
            slSort(&hits, bed6FloatCmpDesc);
        int count;
        for(count = 1; hits != NULL; count++, hits = hits->next)
            {
            if(topOnly && count > topOnly)
                break;
            printf("%s\t%d\t%d\t%s\t%.2f\t%c\n", chrom, originalCoordinates ? chromStart : hits->chromStart, 
                   originalCoordinates ? chromEnd : hits->chromStart + motif->columnCount, name, hits->score, hits->strand[0]);
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
originalCoordinates = optionExists("originalCoordinates");
minScoreCutoff = optionFloat("minScoreCutoff", log2(99/1));
topOnly = optionInt("topOnly", topOnly);
if (argc < 4)
    usage();
motifFinder(argv[1], argv[2], argc-3, argv+3);
return 0;
}
