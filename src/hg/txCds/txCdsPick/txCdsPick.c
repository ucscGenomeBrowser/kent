/* txCdsPick - Pick best CDS if any for transcript given evidence.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "genbank.h"
#include "cdsEvidence.h"
#include "cdsPick.h"
#include "bed.h"

/* Variables set via command line. */
struct hash *refToPepHash = NULL;
struct hash *pepToRefHash = NULL;
FILE *exceptionsOut = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsPick - Pick best CDS if any for transcript given evidence.\n"
  "usage:\n"
  "   txCdsPick in.bed in.tce in.weights out.tce out.cdsPick\n"
  "where:\n"
  "   in.bed is the input transcripts, often from txWalk\n"
  "   in.tce is the evidence in cdsEvidence format\n"
  "   in.weights is a file describing how to weigh each source\n"
  "              of evidence. Columns are source, weight.\n"
  "   out.tce is best cdsEvidence line for coding transcripts, with\n"
  "              weight adjusted\n"
  "   out.cdsPick is a tab-separated file in cdsPick format for all transcripts\n"
  "options:\n"
  "   -weightedTce=weighted.tce - output all weighted cdsEvidence here\n"
  "   -refToPep=refToPep.tab - lets refSeq mappings to associated protein have\n"
  "              higher priority.  Highly recommended.\n"
  "   -exceptionsIn=exceptions.tab - input exceptions. A file with info on\n"
  "            selenocysteine and other exceptions.  You get this file by running\n"
  "            files txCdsRaExceptions on ra files parsed out of genbank flat\n"
  "            files.\n"
  "   -exceptionsOut=exceptions.tab - output exceptions. In same format as\n"
  "            input exceptions\n"
  );
}

static struct optionSpec options[] = {
   {"weightedTce", OPTION_STRING},
   {"refToPep", OPTION_STRING},
   {"exceptionsIn", OPTION_STRING},
   {"exceptionsOut", OPTION_STRING},
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

struct weight
/* A named weight. */
    {
    char *type;		/* Not allocated here */
    double value;	/* Weight value */
    };

struct hash *hashWeights(char *in)
/* Return hash full of weights. */
{
struct lineFile *lf = lineFileOpen(in, TRUE);
char *row[2];
struct hash *hash = hashNew(0);
while (lineFileRow(lf, row))
    {
    struct weight *weight;
    AllocVar(weight);
    weight->value = lineFileNeedDouble(lf, row, 1);
    hashAddSaveName(hash, row[0], weight, &weight->type);
    }
lineFileClose(&lf);
return hash;
}

double weightFromHash(struct hash *hash, char *name)
/* Find weight given name. */
{
struct weight *weight = hashFindVal(hash, name);
if (weight == NULL)
    errAbort("Can't find %s in weight file", name);
return weight->value;
}

void hashRefToPep(char *fileName)
/* Create refToPep and pepToRef hashes from reading file. */
{
refToPepHash = hashNew(19);
pepToRefHash = hashNew(19);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
while (lineFileRow(lf, row))
    {
    hashAdd(refToPepHash, row[0], cloneString(row[1]));
    hashAdd(pepToRefHash, row[1], cloneString(row[0]));
    }
}

struct txInfo
/* Information on a transcript */
    {
    struct txInfo *next;	/* Next in list. */
    char *name;			/* Name, not allocated here. */
    struct cdsEvidence *cdsList; /* List of cds evidence. */
    };

int cdsEvidenceCmpScore(const void *va, const void *vb)
/* Compare to sort based on score (descending). */
{
const struct cdsEvidence *a = *((struct cdsEvidence **)va);
const struct cdsEvidence *b = *((struct cdsEvidence **)vb);
return b->score - a->score;
}

struct hash *loadAndWeighTce(char *fileName, struct hash *weightHash,
	char *outTce)
/* Load transcript cds evidence from file into hash of
 * txInfo. */
{
/* Read all tce's in file into list and hash of txInfo. */
struct txInfo *tx, *txList = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = hashNew(18);
char *row[CDSEVIDENCE_NUM_COLS];
while  (lineFileRow(lf, row))
    {
    /* Convert row to cdsEvidence structure. */
    struct cdsEvidence *cds = cdsEvidenceLoad(row);

    tx = hashFindVal(hash, cds->name);
    if (tx == NULL)
        {
	AllocVar(tx);
	hashAddSaveName(hash, cds->name, tx, &tx->name);
	slAddHead(&txList, tx);
	}
    cds->score *= weightFromHash(weightHash, cds->source);
    cds->score *= (cds->end - cds->start) * 0.01;
    if (!cds->startComplete)
        cds->score *= 0.95;
    if (!cds->endComplete)
        cds->score *= 0.95;
    cds->score *= 2.0/(cds->cdsCount+1);

    /* Parse accession out of name.  I wish this were a proper field.... */
    char *acc = strrchr(cds->name, '.');
    if (acc == NULL)
	errAbort("Can't find . before accession line %d of %s", 
		lf->lineIx, lf->fileName);
    acc += 1;	/* Skip over accession part. */
    int accLen = strlen(acc);
    if (accLen < 6 || accLen > 16)
        errAbort("%s doesn't seem to be an accession line %d of %s",
		acc, lf->lineIx, lf->fileName);

    /* Track whether it's refSeq, and the associated protein. */
    char *refSeqAcc = NULL, *refPepAcc = NULL;
    if (refToPepHash != NULL)
        {
	refPepAcc = hashFindVal(refToPepHash, acc);
	if (refPepAcc != NULL)
	    refSeqAcc = acc;
	}

    /* If we are refSeq, then bump our score for matches to our own
     * rna or protein by an factor of 4. */
    if (refSeqAcc != NULL && startsWith("RefSeq", cds->source) 
    	&& sameString(cds->accession, refSeqAcc))
	cds->score *= 10;
    if (refPepAcc != NULL && startsWith("RefPep", cds->source)
        && sameString(cds->accession, refPepAcc))
	cds->score *= 4;
    slAddHead(&tx->cdsList, cds);
    }
lineFileClose(&lf);
slReverse(&txList);

/* Sort all cdsLists by score. */
for (tx = txList; tx != NULL; tx = tx->next)
    {
    slSort(&tx->cdsList, cdsEvidenceCmpScore);
    }

/* Optionally output all weighted evidence. */
char *weightedTce = optionVal("weightedTce", NULL);
if (weightedTce != NULL)
    {
    FILE *f = mustOpen(weightedTce, "w");
    for (tx = txList; tx != NULL; tx = tx->next)
	{
	struct cdsEvidence *cds;
	for (cds = tx->cdsList; cds != NULL; cds = cds->next)
	     cdsEvidenceTabOut(cds, f);
	}
    carefulClose(&f);
    }

return hash;
}

void transferExceptions(char *inName, char *inSource, char *outName, FILE *f)
/* Write out exceptions attatched to inName to file, this time
 * attached to outName. */
{
if (pepToRefHash && startsWith("RefPep", inSource))
    {
    inName = hashFindVal(pepToRefHash, inName);
    if (inName == NULL)
        return;
    }
if (hashLookup(selenocysteineHash, inName))
    fprintf(f, "%s\tselenocysteine\tyes\n", outName);
if (hashLookup(altStartHash, inName))
    fprintf(f, "%s\texception\talternative_start_codon\n", outName);
}

void txCdsPick(char *inBed, char *inTce, char *inWeights, 
	char *outTce, char *outPick)
/* txCdsPick - Pick best CDS if any for transcript given evidence.. */
{
struct hash *weightHash = hashWeights(inWeights);
verbose(2, "Read %d weights from %s\n", weightHash->elCount, inWeights);
struct hash *txInfoHash = loadAndWeighTce(inTce, weightHash, outTce);
verbose(2, "Read info on %d transcripts from %s\n", 
	txInfoHash->elCount, inTce);
struct lineFile *lf = lineFileOpen(inBed, TRUE);
FILE *fTce = mustOpen(outTce, "w");
FILE *fPick = mustOpen(outPick, "w");
char *row[12];
while (lineFileRow(lf, row))
    {
    struct bed *bed = bedLoad12(row);
    struct txInfo *tx = hashFindVal(txInfoHash, bed->name);
    struct cdsPick pick;
    ZeroVar(&pick);
    pick.name = bed->name;
    pick.refSeq = pick.refProt = pick.swissProt = pick.uniProt = pick.genbank = "";
    if (tx != NULL)
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
		if (startsWith("RefSeq", source) && pick.refSeq[0] == 0)
		    pick.refSeq = cds->accession;
		else if (startsWith("RefPep", source) && pick.refProt[0] == 0)
		    pick.refProt = cds->accession;
		else if (sameString("swissProt", source) && pick.swissProt[0] == 0)
		    {
		    pick.swissProt = cds->accession;
		    if (pick.uniProt[0] == 0)
		        pick.uniProt = cds->accession;
		    }
		else if (sameString("genbankCds", source) && pick.genbank[0] == 0)
		    pick.genbank = cds->accession;
		else if (sameString("trembl", source) && pick.uniProt[0] == 0)
		    pick.uniProt = cds->accession;
		else if (sameString("bestorf", source))
		    pick.wBorfScore = cds->score;
		}
	    }

	if (exceptionsOut)
	    transferExceptions(bestCds->accession, bestCds->source, bed->name, exceptionsOut);
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
if (optionExists("refToPep"))
    hashRefToPep(optionVal("refToPep", NULL));
makeExceptionHashes();
char *fileName = optionVal("exceptionsOut", NULL);
if (fileName != NULL)
    {
    exceptionsOut = mustOpen(fileName, "w");
    if (!optionExists("exceptionsIn"))
        errAbort("Must use exceptionsIn flag with exceptionsOut.");
    }
txCdsPick(argv[1], argv[2], argv[3], argv[4], argv[5]);
carefulClose(&exceptionsOut);
return 0;
}
