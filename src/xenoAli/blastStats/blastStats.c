#include "common.h"
#include "blastParse.h"

int insertStarts;
int insertTotal;
int atMatchTotal;
int matchTotal;
int mismatchTotal;
int blockCount;
int cosmidCount;
int aliSizeTotal;
int contigCount;
int gapAliCount;

void gatherStats(int count, char *fileNames[])
/* Gather stats on all blast files into variables above. */
{
int i, j;
char *fileName;
struct blastFile *bf;
struct blastQuery *bq;
struct blastGappedAli *bga;
struct blastBlock *bb;
int qSize;
char *qSym, *tSym;

for (i = 0; i<count; ++i)
    {
    boolean gotAny = FALSE;
    fileName = fileNames[i];
    uglyf("Processing %s\n", fileName);
    bf = blastFileReadAll(fileName);
    for (bq = bf->queries; bq != NULL; bq = bq->next)
	{
	++contigCount;
	for (bga = bq->gapped; bga != NULL; bga = bga->next)
	    {
	    ++gapAliCount;
	    for (bb = bga->blocks; bb != NULL; bb = bb->next)
		{
		int bInsertStarts = 0;
		int bInsertTotal = 0;
		int bMatches = 0;
		int bMismatches = 0;
		int bAtMatches = 0;
		qSize = bb->qEnd - bb->qStart;
		qSym = bb->qSym;
		tSym = bb->tSym;
		for (j=0; ; ++j)
		    {
		    char q,t;
		    q = qSym[j];
		    t = tSym[j];
		    if (q == 0)
			break;
		    else if (q == t)
			{
			++bMatches;
			if (q == 'a' || q == 't' || q == 'A' || q == 'T')
			    ++bAtMatches;
			}
		    else if (q == '-')
			{
			++bInsertTotal;
			if (j != 0 && qSym[j-1] != '-')
			    ++bInsertStarts;
			}
		    else if (t == '-')
			{
			++bInsertTotal;
			if (j != 0 && tSym[j-1] != '-')
			    ++bInsertStarts;
			}
		    else
			{
			++bMismatches;
			}
		    }
		if (bMatches >= 50)
		    {
		    insertStarts += bInsertStarts;
		    insertTotal += bInsertTotal;
		    atMatchTotal += bAtMatches;
		    matchTotal += bMatches;
		    mismatchTotal += bMismatches;
		    ++blockCount;
		    aliSizeTotal += qSize;
		    gotAny = TRUE;
		    }
		}
	    }
	}
    if (gotAny)
	++cosmidCount;
    blastFileFree(&bf);
    }
}

void printStats(FILE *out)
/* Print statistics from variables gathered above. */
{
fprintf(out, "\nBlast Comparison Data - BLAST side\n");
fprintf(out, "------------------------------------\n");
fprintf(out, "Cosmids w/ alignments %d\n", cosmidCount);
fprintf(out, "Total matches %d\n", matchTotal);
fprintf(out, "Total mismatches %d\n", mismatchTotal);
fprintf(out, "Number of inserts %d\n", insertStarts);
fprintf(out, "Total bases inserted %d\n", insertTotal);
fprintf(out, "Number of alignments %d\n", blockCount);
fprintf(out, "Average alignment size %f\n", (double)aliSizeTotal/blockCount);
fprintf(out, "AT%% of matches %f\n", 100.0 * atMatchTotal/matchTotal);
}


void usage()
/* Print usage and exit. */
{
errAbort(
"blastStats - gather statistics on blast files\n"
"usage:\n"
"    blastStats blastFile(s)");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 2)
    usage();
gatherStats(argc-1, argv+1);
printStats(stdout);
}

