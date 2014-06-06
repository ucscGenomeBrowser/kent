/* txCdsRedoUniprotPicks - Update uniprot columns in pick file based on protein/protein alignment 
 * at end of pipeline vs. mrna/protein alignment at start.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cdsPick.h"
#include "psl.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsRedoUniprotPicks - Update uniprot columns in pick file based on protein/protein alignment\n"
  "at end of pipeline vs. mrna/protein alignment at start.\n"
  "usage:\n"
  "   txCdsRedoUniprotPicks old.picks ucscVsUniprot.psl uniCurated.tab new.picks\n"
  "where:\n"
  "    old.picks is file in format described in cdsPick.h\n"
  "    ucscVsUniprot.psl is a protein/protein psl with ucsc in target, uniProt in query\n"
  "    uniCurated.tab is two columns - uniProt acc, and then a 1 if SwissProt, 0 otherwise\n"
  "    new.picks is the updated version of old.picks\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

double scoreAli(struct psl *psl, boolean isCurated)
/* Make up a score.  Return 0 if it looks worthless, otherwise something that gets bigger the
 * better the alignment. */
{
double aliCount = psl->match + psl->misMatch + psl->repMatch;
double tCoverage = aliCount/psl->tSize;
if (tCoverage < 0.5)
    return 0;
double qCoverage = aliCount/psl->qSize;
if (qCoverage < 0.5)
    return 0;
double score = ((double) psl->match + ((double) psl->repMatch)/2.0) 
    - 20.0 * (double) psl->misMatch - 40.0 * (double) psl->qNumInsert
    - 40.0 * (double) psl->tNumInsert;
score *= 10.0;	/* Make the actual score the most important thing */
if (isCurated)  /* Add in curation factor. */
    score += 1.0;
score *= 10.0;	/* Make the coverage of UCSC the next most important */
score += tCoverage;
score *= 10.0;	/* Make the coverage of UniProt the next most important */
score += qCoverage;
return score;
}

struct psl *bestAliInList(struct psl *start, struct psl *end, struct hash *curatedHash)
/* Return best scoring alignment in list. */
{
struct psl *psl, *bestPsl = NULL;
double bestScore = 0;
for (psl = start; psl != end; psl = psl->next)
    {
    boolean isCurated = (hashLookup(curatedHash, psl->qName) != NULL);
    double score = scoreAli(psl, isCurated);
    if (score > bestScore)
        {
	bestScore = score;
	bestPsl = psl;
	}
    }
return bestPsl;
}

void txCdsRedoUniprotPicks(char *oldPickFile, char *pslFile, char *curatedFile, char *newPickFile)
/* txCdsRedoUniprotPicks - Update uniprot columns in pick file based on protein/protein alignment 
 * at end of pipeline vs. mrna/protein alignment at start.. */
{
/* Read in curated file to hash with values just where there are ones. */
struct hash *curatedHash = hashNew(20);
    {
    struct lineFile *lf = lineFileOpen(curatedFile, TRUE);
    char *row[2];
    while (lineFileRow(lf, row))
	{
	char c = row[1][0];
	switch (c)
	     {
	     case '1':
		hashAdd(curatedHash, row[0], NULL);
		break;
	     case '0':
	        break;
	     default:
	        errAbort("Expecting 0 or 1 in second column, line %d of %s", 
			lf->lineIx, lf->fileName);
		break;
	     }
	}
    lineFileClose(&lf);
    }

/* Read in PSL and make hash that just contains the best good alignment for each. */
struct hash *bestHash = hashNew(19);
    {
    struct psl *pslList = pslLoadAll(pslFile);
    slSort(&pslList, pslCmpTarget);
    struct psl *start, *end;
    for (start = pslList; start != NULL; start = end)
	{
	char *tName = start->tName;
	for (end = start->next; end != NULL; end = end->next)
	    if (!sameString(tName, end->tName))
		break;
	struct psl *bestAli = bestAliInList(start, end, curatedHash);
	if (bestAli != NULL)
	    hashAdd(bestHash, tName, bestAli);
	}
    }

/* Loop through old file and replace values. */
FILE *f = mustOpen(newPickFile, "w");
struct lineFile *lf = lineFileOpen(oldPickFile, TRUE);
char *row[CDSPICK_NUM_COLS];
while (lineFileRowTab(lf, row))
    {
    struct cdsPick pick;
    cdsPickStaticLoad(row, &pick);
    pick.uniProt = pick.swissProt = "";
    struct psl *psl = hashFindVal(bestHash, pick.name);
    if (psl != NULL)
        {
	pick.uniProt = psl->qName;
	if (hashLookup(curatedHash, pick.uniProt))
	    pick.swissProt = pick.uniProt;
	}
    cdsPickTabOut(&pick, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
txCdsRedoUniprotPicks(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
