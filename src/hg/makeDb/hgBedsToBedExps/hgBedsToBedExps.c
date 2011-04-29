/* hgBedsToBedExps - Convert multiple bed files to a single bedExp.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bToBeCfg.h"
#include "bed.h"
#include "bedGraph.h"
#include "rangeTree.h"
#include "errabort.h"

static char const rcsid[] = "$Id: hgBedsToBedExps.c,v 1.6 2010/05/06 18:02:38 kent Exp $";

boolean dupeLetterOk = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgBedsToBedExps - Convert multiple bed files to a single bedExp.\n"
  "usage:\n"
  "   hgBedsToBedExps in.cfg out.bed out.exps\n"
  "where in.cfg is a tab separated file that describes the beds.  The columns are\n"
  "   <factor> <cell name> <cell letter> <db/file> <score col ix> <multiplier> <file/table>\n"
  "options:\n"
  "   -dupeLetterOk - if true don't insist that all cell letters be unique\n"
  );
}

static struct optionSpec options[] = {
   {"dupeLetterOk", OPTION_BOOLEAN},
   {NULL, 0},
};

struct factorInfo
/* Information associated with a single factor. */
    {
    struct factorInfo *next;	/* Next in list. */
    char *factor;		/* Name of factor, not allocated here. */
    struct bToBeCfg *sourceList; /* List of sources associated with factor. */
    };

struct sourceInfo
/* Information on a single source. */
    {
    struct sourceInfo *next;
    char *source;	/* Cell line, tissue, etc. */
    char *sourceId;	/* Short ID (one letter usually) for source */
    int sourceIx;	/* Index of source in list. */
    };

struct sourceInfo *getSources(struct bToBeCfg * cfgList, struct hash *hash)
/* Return list of sources found in cfgList.  Fill hash with these as well, keying
 * by source name. */
{
struct hash *uniqHash = hashNew(0);
struct sourceInfo *sourceList = NULL, *source;
struct bToBeCfg *cfg;
int sourceIx = 0;
for (cfg = cfgList; cfg != NULL; cfg = cfg->next)
    {
    source = hashFindVal(hash, cfg->source);
    if (source == NULL)
        {
	AllocVar(source);
	source->source = cfg->source;
	source->sourceId = cfg->sourceId;
	source->sourceIx = sourceIx++;
	if (!dupeLetterOk)
	    {
	    if (hashLookup(uniqHash, source->sourceId))
		errAbort("Source ID %s associated with two different source names including %s", 
			source->sourceId, source->source);
	    hashAdd(uniqHash, source->sourceId, NULL);
	    }
	slAddHead(&sourceList, source);
	hashAdd(hash, source->source, source);
	}
    else
        {
	if (!sameString(source->sourceId, cfg->sourceId))
	    errAbort("Source %s associated with IDs %s and %s", source->source,
	    	source->sourceId, cfg->sourceId);
	}
    }
hashFree(&uniqHash);
slReverse(&sourceList);
return sourceList;
}

struct factorInfo *bundleFactors(struct bToBeCfg **pCfgList)
/* Return experiments bundled into factors.  Eats up cfgList, moving it in pieces to
 * factors. */
{
struct factorInfo *factorList = NULL, *factor;
struct bToBeCfg *cfg, *next;
struct hash *hash = hashNew(0);
for (cfg = *pCfgList; cfg != NULL; cfg = next)
    {
    next = cfg->next;
    if ((factor = hashFindVal(hash, cfg->factor)) == NULL)
        {
	AllocVar(factor);
	factor->factor = cfg->factor;
	slAddHead(&factorList, factor);
	hashAdd(hash, factor->factor, factor);
	}
    slAddTail(&factor->sourceList, cfg);
    }
hashFree(&hash);
*pCfgList = NULL;
slReverse(&factorList);
return factorList;
}

void outputSources(struct sourceInfo *sourceList, char *fileName)
/* Write out sources to file. */
{
FILE *f = mustOpen(fileName, "w");
struct sourceInfo *source;
for (source = sourceList; source != NULL; source = source->next)
    {
    fprintf(f, "%d\t", source->sourceIx);
    fprintf(f, "%s\t", source->sourceId);
    fprintf(f, "%s\t", source->source);
    fprintf(f, "n/a\t");
    fprintf(f, "n/a\t");
    fprintf(f, "n/a\t");
    fprintf(f, "3\t");
    fprintf(f, "n/a,n/a/,%s,\n", source->source);
    }
carefulClose(&f);
}

struct bedGraph *loadAsBedGraph(struct bToBeCfg *cfg)
/* Load data specified by cfg as a list of bedGraphs and return. */
{
struct bedGraph *bgList = NULL, *bg;
if (sameString(cfg->dataSource, "file"))
    {
    int neededSize = 0, gotSize = 0;
    struct lineFile *lf = lineFileOpen(cfg->dataTable, TRUE);
    char *row[32];
    while ((gotSize = lineFileChopNext(lf, row, ArraySize(row))) > 0)
        {
	if (neededSize == 0)
	    {
	    neededSize = gotSize;
	    if (gotSize >= ArraySize(row))
	        lineFileAbort(lf, "Too many columns (max %d)", (int)ArraySize(row));
	    }
	else
	    lineFileExpectWords(lf, neededSize, gotSize);
	AllocVar(bg);
	bg->chrom = cloneString(row[0]);
	bg->chromStart = lineFileNeedNum(lf, row, 1);
	bg->chromEnd = lineFileNeedNum(lf, row, 2);
	bg->dataValue = lineFileNeedDouble(lf, row, cfg->scoreCol) * cfg->multiplier;
	slAddHead(&bgList, bg);
	}
    lineFileClose(&lf);
    }
else
    {
    errAbort("For now only 'file' is supported as a data source.\n");
    }
slReverse(&bgList);
return bgList;
}

struct sourcedSite
/* A site associated with a single factor and source. */
    {
    struct sourcedSite *next;
    struct sourceInfo *source;	/* Source. */
    struct bedGraph *site;	/* Binding site and strength. */
    };

void outputFactorOnChrom(char *chrom, struct rbTree *tree, struct factorInfo *factor,
	int sourceCount, FILE *f)
/* Output all merged binding sites for this factor on chromosome. */
{
struct slRef *ref, *list = rbTreeItems(tree);
double *levels;
AllocArray(levels, sourceCount);

for (ref = list; ref != NULL; ref = ref->next)
    {
    struct range *range = ref->val;
    struct sourcedSite *site; 

    /* Zero out levels for this site initially. */
    int i;
    for (i=0; i<sourceCount; ++i)
        levels[i] = 0;

    /* Set levels according to sourceSite list. */
    double maxLevel = 0.0;
    for (site = range->val; site != NULL; site = site->next)
	{
	double level = site->site->dataValue;
	if (level > 1000)
	    level = 1000;
	if (level > maxLevel)
	    maxLevel = level;
	levels[site->source->sourceIx] = level;
	}

    /* Output. */
    fprintf(f, "%s\t%d\t%d\t", chrom, range->start, range->end);
    fprintf(f, "%s\t", factor->factor);
    fprintf(f, "%d\t", round(maxLevel));	/* score */
    fprintf(f, ".\t");  /* strand.... */
    fprintf(f, "%d\t%d\t", range->start, range->end);
    fprintf(f, "0\t");  /* itemRgb */
    fprintf(f, "1\t");  /* block count */
    fprintf(f, "%d\t0\t", range->end - range->start);
    fprintf(f, "%d\t", sourceCount);
    for (i=0; i<sourceCount; ++i)
        fprintf(f, "%d,", i);
    fprintf(f, "\t");
    for (i=0; i<sourceCount; ++i)
        fprintf(f, "%d,", round(levels[i]));
    fprintf(f, "\n");
    }

slFreeList(&list);
freeMem(levels);
}

void processFactor(struct factorInfo *factor, struct hash *sourceHash, FILE *f)
/* Load files associated with factor, combine into bed 15, and output to file. */
{
struct hash *chromHash = hashNew(0);
struct rbTree *chromList = NULL, *chromTree;
struct bToBeCfg *cfg;
for (cfg = factor->sourceList; cfg != NULL; cfg = cfg->next)
    {
    struct bedGraph *bg, *bgList = loadAsBedGraph(cfg);
    struct sourceInfo *source = hashMustFindVal(sourceHash, cfg->source);
    struct sourcedSite *site;
    verbose(2, "Loaded %d from %s\n", slCount(bgList), cfg->dataTable);
    for (bg = bgList; bg != NULL; bg = bg->next)
        {
	chromTree = hashFindVal(chromHash, bg->chrom);
	if (chromTree == NULL)
	    {
	    chromTree = rangeTreeNew();
	    slAddHead(&chromList, chromTree);
	    hashAdd(chromHash, bg->chrom, chromTree);
	    }
	struct range *rangeList = rangeTreeAllOverlapping(chromTree, bg->chromStart, bg->chromEnd);
	struct range *newRange, *oldRange;
	lmAllocVar(chromTree->lm, newRange);
	lmAllocVar(chromTree->lm, site);
	site->source = source;
	site->site = bg;
	newRange->start = bg->chromStart;
	newRange->end = bg->chromEnd;
	newRange->val = site;
	for (oldRange = rangeList; oldRange != NULL; oldRange = oldRange->next)
	    {
	    newRange->start = min(newRange->start, oldRange->start);
	    newRange->end = max(newRange->end, oldRange->end);
	    newRange->val = slCat(oldRange->val, newRange->val);
	    rbTreeRemove(chromTree, oldRange);
	    }
	rbTreeAdd(chromTree, newRange);
	}
    }
slReverse(&chromList);

struct hashEl *chromElList, *chromEl;
chromElList = hashElListHash(chromHash);
for (chromEl = chromElList; chromEl != NULL; chromEl = chromEl->next)
    {
    chromTree = chromEl->val;
    outputFactorOnChrom(chromEl->name, chromTree, factor, sourceHash->elCount, f);
    }
slFreeList(&chromElList);

rbTreeFreeList(&chromList);
}

void checkForDupes(struct bToBeCfg *list)
/* Make sure that only have one item for each factor/source combination. */
{
struct hash *hash = newHash(0);
struct bToBeCfg *el;
boolean gotDupe = FALSE;
for (el = list; el != NULL; el = el->next)
    {
    char buf[256];
    safef(buf, sizeof(buf), "%s & %s", el->factor, el->source);
    struct bToBeCfg *old = hashFindVal(hash, buf);
    if (old)
	{
        warn("%s %s used in %s and %s", el->factor, el->source, el->dataTable, old->dataTable);
	gotDupe = TRUE;
	}
    else
        hashAdd(hash, buf, el);
    }
if (gotDupe)
    noWarnAbort();
}

void hgBedsToBedExps(char *inCfg, char *outBed, char *outExp)
/* hgBedsToBedExps - Convert multiple bed files to a single bedExp.. */
{
/* Load up input configuration . */
struct bToBeCfg *cfgList = bToBeCfgLoadAll(inCfg);
checkForDupes(cfgList);
verbose(1, "Loaded %d records from %s\n", slCount(cfgList), inCfg);

/* Find and output all sources used. */
struct hash *sourceHash = hashNew(0);
struct sourceInfo *sourceList = getSources(cfgList, sourceHash);
verbose(1, "Got %d sources\n", slCount(sourceList));
outputSources(sourceList, outExp);

/* Get factors, and then process them one at a time. */
struct factorInfo *factor, *factorList = bundleFactors(&cfgList);
verbose(1, "Got %d factors\n", slCount(factorList));
FILE *f = mustOpen(outBed, "w");
for (factor = factorList; factor != NULL; factor = factor->next)
    processFactor(factor, sourceHash, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
dupeLetterOk = optionExists("dupeLetterOk");
hgBedsToBedExps(argv[1], argv[2], argv[3]);
return 0;
}
