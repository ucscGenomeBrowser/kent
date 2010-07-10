/* motifFinder - find largest scoring motif in bed items. */

#include "common.h"
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

static float minScoreCutoff = 0;
static char *motifTable = "transRegCodeMotifPseudoCounts";
static char *markovTable = "markovModels";
static boolean originalCoordinates = FALSE;
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
        unsigned chromSize, maxStart = 0;
        float maxScore = 0;
        char maxStrand = 0;
        boolean maxScoreInited = FALSE;
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
        for (i = 0; i < 2; i++)
            {
            char strand = i == 0 ? '+' : '-';
            if(strand == '-')
                reverseComplement(seq->dna, dnaLength);
            for (j = 0; j < length - motif->columnCount + 1; j++)
                // tricky b/c j includes the two bytes on either side of actual sequence.
                {
                double score = dnaMotifBitScoreWithMarkovBg(motif, seq->dna + j, mark2);
                if(!maxScoreInited || score > maxScore)
                    {
                    maxScoreInited = TRUE;
                    maxScore = score;
                    maxStrand = strand;
                    if(strand == '-')
                        maxStart = (chromEnd - j) - motif->columnCount;
                    else
                        maxStart = chromStart + j;
                    }
                verbose(3, "j: %d; score: %.2f\n", j, score);
                }
            }
        if(maxScoreInited && maxScore > minScoreCutoff)
            printf("%s\t%d\t%d\t%s\t%.2f\t%c\n", chrom, originalCoordinates ? chromStart : maxStart, 
                   originalCoordinates ? chromEnd : maxStart + motif->columnCount, name, maxScore, maxStrand);
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
if (argc < 4)
    usage();
motifFinder(argv[1], argv[2], argc-3, argv+3);
return 0;
}
