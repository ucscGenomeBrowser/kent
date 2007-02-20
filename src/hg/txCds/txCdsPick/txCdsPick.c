/* txCdsPick - Pick best CDS if any for transcript given evidence.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cdsEvidence.h"
#include "bed.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsPick - Pick best CDS if any for transcript given evidence.\n"
  "usage:\n"
  "   txCdsPick in.bed in.tce in.weights out.gp outPep.fa\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

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

struct hash *loadAndWeighTce(char *fileName, struct hash *weightHash)
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
    slAddHead(&tx->cdsList, cds);
    }
lineFileClose(&lf);

/* Sort all cdsLists by score. */
for (tx = txList; tx != NULL; tx = tx->next)
    slSort(&tx->cdsList, cdsEvidenceCmpScore);
return hash;
}

void txCdsPick(char *inBed, char *inTce, char *inWeights, 
	char *outGp, char *outPep)
/* txCdsPick - Pick best CDS if any for transcript given evidence.. */
{
struct hash *weightHash = hashWeights(inWeights);
verbose(2, "Read %d weights from %s\n", weightHash->elCount, inWeights);
struct hash *txInfoHash = loadAndWeighTce(inTce, weightHash);
verbose(2, "Read info on %d transcripts from %s\n", 
	txInfoHash->elCount, inTce);
struct lineFile *lf = lineFileOpen(inBed, TRUE);
FILE *f = mustOpen(outGp, "w");
char *row[12];
while (lineFileRow(lf, row))
    {
    struct bed *bed = bedLoad12(row);
    struct txInfo *tx = hashFindVal(txInfoHash, bed->name);
    fprintf(f, "Transcript %s\n", bed->name);
    if (tx != NULL)
        {
	struct cdsEvidence *cds;
	for (cds = tx->cdsList; cds != NULL; cds = cds->next)
	    cdsEvidenceTabOut(cds, f);
	}
    bedFree(&bed);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
txCdsPick(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
