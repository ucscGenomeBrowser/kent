/* scopCollapse - Convert SCOP model to SCOP ID. Also make id/name converter file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "rangeTree.h"
#include "bed.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "scopCollapse - Convert SCOP model to SCOP ID. Also make id/name converter file.\n"
  "usage:\n"
  "   scopCollapse inFeat.tab inModel.tab outFeat.tab outDesc.tab outKnownToSuper.tab\n"
  "where:\n"
  "   inFeat.tab is a 6 column file:\n"
  "       proteinName start end hmmId eVal score\n"
  "   inModel.tab is scop model file of 5 columns\n"
  "       hmmId superfamilyId seedId seedAcc description\n"
  "   outFeat.tab is 6 column:\n"
  "       proteinName start end seedId eVal score\n"
  "   outDesc.tab is 3 column:\n"
  "       superfamilyId seedId description\n"
  "   outKnownToSuper.tab is 6 column:\n"
  "       proteinName superfamilyId start end eVal\n"
  "This transformation lets us treat pfam and scop the same.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct protFeature
/* Protein feature. */
    {
    struct protFeature *next;  /* Next in singly linked list. */
    char *protein;	/* Protein name */
    unsigned start;	/* Start position in protein */
    unsigned end;	/* End position in protein */
    char *name;		/* Feature name. */
    double eVal;	/* Expectation value 0.0 - 1.0, usually close to 0. */
    double score; 	/* Another score type. */
    };

struct protInfo
/* Info on a single protein. */
    {
    struct protInfo *next;
    char *name;	/* Name of protein - not allocated here. */
    struct protFeature *featureList;  /* List of features associated with protein. */
    };

int protFeatureCmpName(const void *va, const void *vb)
/* Compare to sort based on name. */
{
const struct protFeature *a = *((struct protFeature **)va);
const struct protFeature *b = *((struct protFeature **)vb);
return strcmp(a->name, b->name);
}

struct protFeature *highestScoringFeature(struct protFeature *start, struct protFeature *end,
	int rangeStart, int rangeEnd)
/* Return highest scoring feature from start up to end. */
{
struct protFeature *bestFeat = NULL, *feat;
double bestScore = -1.0;
for (feat = start; feat != end ; feat = feat->next)
    {
    if (rangeIntersection(rangeStart, rangeEnd, feat->start, feat->end) > 0)
	{
	if (feat->score > bestScore)
	    {
	    bestFeat = feat;
	    bestScore = feat->score;
	    }
	}
    }
return bestFeat;
}

void outputProt(struct protInfo *prot, struct hash *seedToScop, FILE *f, FILE *fKnownTo)
/* Callapse together all features of same type that overlap and output */
{
slSort(&prot->featureList, protFeatureCmpName);
struct protFeature *startFeature, *endFeature;
for (startFeature = prot->featureList; startFeature != NULL; startFeature = endFeature)
    {
    for (endFeature = startFeature->next; endFeature != NULL; endFeature = endFeature->next)
        if (!sameString(startFeature->name, endFeature->name))
	    break;
    struct rbTree *rangeTree = rangeTreeNew();
    struct protFeature *feature;
    for (feature = startFeature; feature != endFeature; feature = feature->next)
	rangeTreeAdd(rangeTree, feature->start, feature->end);
    struct range *range, *rangeList = rangeTreeList(rangeTree);
    for (range = rangeList; range != NULL; range = range->next)
	{
	feature = highestScoringFeature(startFeature, endFeature, range->start, range->end);
        fprintf(f, "%s\t%d\t%d\t%s\n", prot->name, range->start, range->end, startFeature->name);
	fprintf(fKnownTo, "%s\t%s\t%d\t%d\t%g\n",
		prot->name, (char *)hashMustFindVal(seedToScop, startFeature->name),
		range->start, range->end, feature->eVal);
	}

    rangeTreeFree(&rangeTree);
    }
}


void scopCollapse(char *inFeat, char *inModel, char *outFeat, char *outDesc, 
	char *outKnownTo)
/* scopCollapse - Convert SCOP model to SCOP ID. Also make id/name converter file.. */
{
/* Process inModel file, writing three columns to output, and keeping
 * a couple of columns in a hash */
struct hash *modelToSeed = hashNew(18);
struct hash *seedToScop = hashNew(16);
struct lineFile *lf = lineFileOpen(inModel, TRUE);
FILE *f = mustOpen(outDesc, "w");
char *modRow[5];
while (lineFileRowTab(lf, modRow))
    {
    char *seedId = modRow[2];
    hashAdd(modelToSeed, modRow[0], cloneString(seedId) );
    if (!hashLookup(seedToScop, seedId))
        {
	char *scopId = modRow[1];
	hashAdd(seedToScop, seedId, cloneString(scopId));
	fprintf(f, "%s\t%s\t%s\n", scopId, seedId, modRow[4]);
	}
    }
carefulClose(&f);
lineFileClose(&lf);

/* Process in-feature.  We make up a structure for each protein here. */
struct hash *protHash = hashNew(18);
struct protInfo *prot, *protList = NULL;
lf = lineFileOpen(inFeat, TRUE);
char *featRow[6];
while (lineFileRow(lf, featRow))
    {
    prot = hashFindVal(protHash, featRow[0]);
    if (prot == NULL)
        {
	AllocVar(prot);
	hashAddSaveName(protHash, featRow[0], prot, &prot->name);
	slAddHead(&protList, prot);
	}
    struct protFeature *feature;
    AllocVar(feature);
    feature->protein = prot->name;
    feature->start = lineFileNeedNum(lf, featRow, 1);
    feature->end = lineFileNeedNum(lf, featRow, 2);
    feature->name = hashMustFindVal(modelToSeed, featRow[3]);
    feature->eVal = lineFileNeedDouble(lf, featRow, 4);
    feature->score = lineFileNeedDouble(lf, featRow, 5);
    slAddHead(&prot->featureList, feature);
    }
lineFileClose(&lf);
slReverse(&protList);

f = mustOpen(outFeat, "w");
FILE *fKnownTo = mustOpen(outKnownTo, "w");
for (prot = protList; prot != NULL; prot = prot->next)
    outputProt(prot, seedToScop, f, fKnownTo);
carefulClose(&f);
carefulClose(&fKnownTo);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
scopCollapse(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
