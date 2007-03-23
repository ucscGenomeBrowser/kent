/* scopCollapse - Convert SCOP model to SCOP ID. Also make id/name converter file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "rangeTree.h"
#include "bed.h"

static char const rcsid[] = "$Id: scopCollapse.c,v 1.1 2007/03/23 10:43:14 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "scopCollapse - Convert SCOP model to SCOP ID. Also make id/name converter file.\n"
  "usage:\n"
  "   scopCollapse inFeat.tab inModel.tab outFeat.tab outDesc.tab\n"
  "where:\n"
  "   inFeat.tab is a 4 column file:\n"
  "       proteinName start end hmmId\n"
  "   inModel.tab is scop model file of 5 columns\n"
  "       hmmId superfamilyId seedId seedAcc description\n"
  "   outFeat.tab is 4 column:\n"
  "       proteinName start end seedId\n"
  "   outDesc.tab is 3 column:\n"
  "       superfamilyId seedId description\n"
  "This transformation lets us treat pfam and scop the same.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct protInfo
/* Info on a single protein. */
    {
    struct protInfo *next;
    char *name;	/* Name of protein - not allocated here. */
    struct bed4 *featureList;  /* List of features associated with protein. */
    };

int bedCmpName(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart. */
{
const struct bed *a = *((struct bed **)va);
const struct bed *b = *((struct bed **)vb);
return strcmp(a->name, b->name);
}

void outputProt(struct protInfo *prot, FILE *f)
/* Callapse together all features of same type that overlap and output */
{
slSort(&prot->featureList, bedCmpName);
struct bed4 *startFeature, *endFeature;
for (startFeature = prot->featureList; startFeature != NULL; startFeature = endFeature)
    {
    for (endFeature = startFeature->next; endFeature != NULL; endFeature = endFeature->next)
        if (!sameString(startFeature->name, endFeature->name))
	    break;
    struct rbTree *rangeTree = rangeTreeNew();
    struct bed4 *feature;
    for (feature = startFeature; feature != endFeature; feature = feature->next)
	rangeTreeAdd(rangeTree, feature->chromStart, feature->chromEnd);
    struct range *range, *rangeList = rangeTreeList(rangeTree);
    for (range = rangeList; range != NULL; range = range->next)
        fprintf(f, "%s\t%d\t%d\t%s\n", prot->name, range->start, range->end, startFeature->name);
    rangeTreeFree(&rangeTree);
    }
}


void scopCollapse(char *inFeat, char *inModel, char *outFeat, char *outDesc)
/* scopCollapse - Convert SCOP model to SCOP ID. Also make id/name converter file.. */
{
/* Process inModel file, writing three columns to output, and keeping
 * a couple of columns in a hash */
struct hash *modelToSeed = hashNew(18);
struct hash *uniqHash = hashNew(0);
struct lineFile *lf = lineFileOpen(inModel, TRUE);
FILE *f = mustOpen(outDesc, "w");
char *modRow[5];
while (lineFileRowTab(lf, modRow))
    {
    char *seedId = modRow[2];
    hashAdd(modelToSeed, modRow[0], cloneString(seedId) );
    if (!hashLookup(uniqHash, seedId))
        {
	hashAdd(uniqHash, seedId, NULL);
	fprintf(f, "%s\t%s\t%s\n", modRow[1], seedId, modRow[4]);
	}
    }
carefulClose(&f);
lineFileClose(&lf);

/* Process in-feature.  We make up a structure for each protein here. */
struct hash *protHash = hashNew(18);
struct protInfo *prot, *protList = NULL;
lf = lineFileOpen(inFeat, TRUE);
char *featRow[4];
while (lineFileRow(lf, featRow))
    {
    prot = hashFindVal(protHash, featRow[0]);
    if (prot == NULL)
        {
	AllocVar(prot);
	hashAddSaveName(protHash, featRow[0], prot, &prot->name);
	slAddHead(&protList, prot);
	}
    struct bed4 *feature;
    AllocVar(feature);
    feature->chrom = prot->name;
    feature->chromStart = lineFileNeedNum(lf, featRow, 1);
    feature->chromEnd = lineFileNeedNum(lf, featRow, 2);
    feature->name = hashMustFindVal(modelToSeed, featRow[3]);
    slAddHead(&prot->featureList, feature);
    }
lineFileClose(&lf);
slReverse(&protList);

f = mustOpen(outFeat, "w");
for (prot = protList; prot != NULL; prot = prot->next)
    outputProt(prot, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
scopCollapse(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
