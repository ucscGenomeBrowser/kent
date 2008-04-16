/* txCdsPick - Pick best CDS if any for transcript given evidence.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "genbank.h"
#include "cdsEvidence.h"
#include "cdsPick.h"
#include "bed.h"
#include "txCommon.h"

/* Variables set via command line. */
FILE *exceptionsOut = NULL;
double threshold = 617.9;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsPick - Pick best CDS if any for transcript given evidence.\n"
  "usage:\n"
  "   txCdsPick in.bed in.tce refToPep.tab out.tce out.cdsPick\n"
  "where:\n"
  "   in.bed is the input transcripts, often from txWalk\n"
  "   in.tce is the evidence in cdsEvidence format\n"
  "   refToPep.tab is a two column file with refSeq mRNA ids in first\n"
  "          column and refSeq protein id's in the second column\n"
  "   out.tce is best cdsEvidence line for coding transcripts, with\n"
  "              weight adjusted for refSeq hits\n"
  "   out.cdsPick is a tab-separated file in cdsPick format for all transcripts\n"
  "options:\n"
  "   -exceptionsIn=exceptions.tab - input exceptions. A file with info on\n"
  "            selenocysteine and other exceptions.  You get this file by running\n"
  "            files txCdsRaExceptions on ra files parsed out of genbank flat\n"
  "            files.\n"
  "   -exceptionsOut=exceptions.tab - output exceptions. In same format as\n"
  "            input exceptions\n"
  "   -threshold=0.N - Set score threshold to be a protein. Default %f\n"
  "            This is calculated for maximum separation of negative and\n"
  "            positive examples in training set using bestThreshold on\n"
  "            txCdsPredict scores.\n"
  , threshold
  );
}

double ccdsWeight = 200000;
double refSeqWeight = 100000;
double txCdsPredictWeight = 10000;
double weightedThreshold;

static struct optionSpec options[] = {
   {"exceptionsIn", OPTION_STRING},
   {"exceptionsOut", OPTION_STRING},
   {"threshold", OPTION_DOUBLE},
   {NULL, 0},
};

struct hash *selenocysteineHash = NULL;
struct hash *altStartHash = NULL;

void makeExceptionHashes()
/* Create hash that has accessions using selanocysteine in it
 * if using the exceptions option.  Otherwise the hash will be
 * empty. */
{
char *fileName = optionVal("exceptionsIn", NULL);
if (fileName != NULL)
    genbankExceptionsHash(fileName, &selenocysteineHash, &altStartHash);
else
    selenocysteineHash = altStartHash = hashNew(4);
}

void hashRefToPep(char *fileName, struct hash **retRefToPep, struct hash **retPepToRef)
/* Return hash with protein keys and peptide values. */
{
struct hash *pepToRefHash = hashNew(19);
struct hash *refToPepHash = hashNew(19);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
while (lineFileRow(lf, row))
    {
    hashAdd(pepToRefHash, row[1], cloneString(row[0]));
    hashAdd(refToPepHash, row[0], cloneString(row[1]));
    }
*retRefToPep = refToPepHash;
*retPepToRef = pepToRefHash;
}

struct txCdsInfo
/* Information on a transcript */
    {
    struct txCdsInfo *next;	/* Next in list. */
    char *name;			/* Name, not allocated here. */
    struct cdsEvidence *cdsList; /* List of cds evidence. */
    };

struct hash *loadAndWeighTce(char *fileName, struct hash *refToPepHash,
	struct hash *pepToRefHash)
/* Load transcript cds evidence from file into hash of
 * txCdsInfo. */
{
/* Read all tce's in file into list and hash of txCdsInfo. */
struct txCdsInfo *tx, *txList = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = hashNew(18);
char *row[CDSEVIDENCE_NUM_COLS];
while  (lineFileRow(lf, row))
    {
    /* Convert row to cdsEvidence structure. */
    struct cdsEvidence *cds = cdsEvidenceLoad(row);
    char *acc = txAccFromTempName(cds->name);

    tx = hashFindVal(hash, cds->name);
    if (tx == NULL)
        {
	AllocVar(tx);
	hashAddSaveName(hash, cds->name, tx, &tx->name);
	slAddHead(&txList, tx);
	}

    /* Track whether it's refSeq, and the associated protein. */
    char *refSeqAcc = NULL, *refPepAcc = NULL;

    refPepAcc = hashFindVal(refToPepHash, acc);
    refSeqAcc = hashFindVal(pepToRefHash, acc);
    if (refPepAcc != NULL && refSeqAcc == NULL)
	refSeqAcc = acc;
    if (refSeqAcc != NULL && refPepAcc == NULL)
        refPepAcc = acc;

    /* If we are refSeq, bump our score for matches to our own 
     * bits by a huge factor. */
    if (refPepAcc != NULL && startsWith("RefPep", cds->source)
        && sameString(cds->accession, refPepAcc))
	cds->score += refSeqWeight;
    if (refSeqAcc != NULL && startsWith("RefSeq", cds->source)
        && sameString(cds->accession, refSeqAcc))
	cds->score += refSeqWeight + 100;

    /* If we are CCDS then that's great too. */
    if (sameString("ccds", cds->source))
        cds->score += ccdsWeight;

    /* If we are txCdsPredict bump weight too.  Only RefSeq and txCdsPredict
     * can actually possibly make it over real threshold. */
    if (sameString("txCdsPredict", cds->source))
        cds->score += txCdsPredictWeight;
    slAddHead(&tx->cdsList, cds);
    }
lineFileClose(&lf);
slReverse(&txList);

/* Sort all cdsLists by score. */
for (tx = txList; tx != NULL; tx = tx->next)
    slSort(&tx->cdsList, cdsEvidenceCmpScore);

return hash;
}

void transferExceptions(char *inName, char *inSource, struct hash *pepToRefHash,
	char *outName, FILE *f)
/* Write out exceptions attatched to inName to file, this time
 * attached to outName. */
{
if (startsWith("RefPep", inSource))
    inName = hashMustFindVal(pepToRefHash, inName);
if (hashLookup(selenocysteineHash, inName))
    fprintf(f, "%s\tselenocysteine\tyes\n", outName);
if (hashLookup(altStartHash, inName))
    fprintf(f, "%s\texception\talternative_start_codon\n", outName);
}

void txCdsPick(char *inBed, char *inTce, char *refToPepTab, char *outTce, char *outPick)
/* txCdsPick - Pick best CDS if any for transcript given evidence.. */
{
struct hash *pepToRefHash, *refToPepHash;
hashRefToPep(refToPepTab, &refToPepHash, &pepToRefHash);
struct hash *txCdsInfoHash = loadAndWeighTce(inTce, refToPepHash, pepToRefHash);
verbose(2, "Read info on %d transcripts from %s\n", 
	txCdsInfoHash->elCount, inTce);
struct lineFile *lf = lineFileOpen(inBed, TRUE);
FILE *fTce = mustOpen(outTce, "w");
FILE *fPick = mustOpen(outPick, "w");
char *row[12];
while (lineFileRow(lf, row))
    {
    struct bed *bed = bedLoad12(row);
    struct txCdsInfo *tx = hashFindVal(txCdsInfoHash, bed->name);
    struct cdsPick pick;
    ZeroVar(&pick);
    pick.name = bed->name;
    pick.refSeq = pick.refProt = pick.swissProt = pick.uniProt = pick.ccds = "";
    if (tx != NULL && tx->cdsList->score >= weightedThreshold)
        {
	struct cdsEvidence *cds, *bestCds = tx->cdsList;
	int bestSize = bestCds->end - bestCds->start;
	int minSize = bestSize*0.90;
	cdsEvidenceTabOut(bestCds, fTce);
	pick.start = bestCds->start;
	pick.end = bestCds->end;
	pick.source = bestCds->source;
	pick.score = bestCds->score;
	pick.startComplete = bestCds->startComplete;
	pick.endComplete = bestCds->endComplete;
	for (cds = tx->cdsList; cds != NULL; cds = cds->next)
	    {
	    char *source = cds->source;
	    if (rangeIntersection(bestCds->start, bestCds->end, cds->start, cds->end)
	    	>= minSize)
		{
		if (startsWith("RefPep", source))
		    {
		    if (pick.refProt[0] == 0)
			{
			pick.refProt = cds->accession;
			pick.refSeq = hashMustFindVal(pepToRefHash, cds->accession);
			}
		    }
		else if (startsWith("RefSeq", source))
		    {
		    if (pick.refSeq[0] == 0)
		        pick.refSeq = cds->accession;
		    }
		else if (sameString("swissProt", source))
		    {
		    if (pick.swissProt[0] == 0)
			{
			pick.swissProt = cds->accession;
			if (pick.uniProt[0] == 0)
			    pick.uniProt = cds->accession;
			}
		    }
		else if (sameString("trembl", source))
		    {
		    if (pick.uniProt[0] == 0)
			pick.uniProt = cds->accession;
		    }
		else if (sameString("txCdsPredict", source))
		    {
		    }
		else if (sameString("genbankCds", source))
		    {
		    }
		else if (sameString("ccds", source))
		    {
		    if (pick.ccds[0] == 0)
		        pick.ccds = cds->accession;
		    }
		else
		    errAbort("Unknown source %s", source);
		}
	    }

	if (exceptionsOut)
	    transferExceptions(bestCds->accession, bestCds->source, pepToRefHash,
	    	bed->name, exceptionsOut);
	}
    else
        {
	pick.source = "noncoding";
	}
    cdsPickTabOut(&pick, fPick);
    bedFree(&bed);
    }
carefulClose(&fPick);
carefulClose(&fTce);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
makeExceptionHashes();
char *fileName = optionVal("exceptionsOut", NULL);
if (fileName != NULL)
    {
    exceptionsOut = mustOpen(fileName, "w");
    if (!optionExists("exceptionsIn"))
        errAbort("Must use exceptionsIn flag with exceptionsOut.");
    }
threshold = optionDouble("threshold", threshold);
weightedThreshold = threshold + txCdsPredictWeight;
txCdsPick(argv[1], argv[2], argv[3], argv[4], argv[5]);
carefulClose(&exceptionsOut);
return 0;
}
