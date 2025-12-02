/* vcfTrack -- handlers for Variant Call Format data. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "bigWarn.h"
#include "dystring.h"
#include "rainbow.h"
#include "errCatch.h"
#include "fa.h"
#include "genePredReader.h"
#include "hacTree.h"
#include "hdb.h"
#include "hgColors.h"
#include "hgTracks.h"
#include "iupac.h"
#include "net.h"
#include "pgSnp.h"
#include "phyloTree.h"
#include "trackHub.h"
#include "trashDir.h"
#include "variantProjector.h"
#include "vcf.h"
#include "vcfUi.h"
#include "knetUdc.h"
#include "udc.h"
#include "memgfx.h"
#include "chromAlias.h"
#include "hgConfig.h"
#include "wigCommon.h"

// Russ Corbett-Detig suggested darker shades for coloring non-synonymous variants green
Color darkerShadesOfGreenOnWhite[EXPR_DATA_SHADES];

static boolean getMinQual(struct trackDb *tdb, double *retMinQual)
/* Return TRUE and set retMinQual if cart contains minimum QUAL filter */
{
if (cartOrTdbBoolean(cart, tdb, VCF_APPLY_MIN_QUAL_VAR, VCF_DEFAULT_APPLY_MIN_QUAL))
    {
    if (retMinQual != NULL)
	*retMinQual = cartOrTdbDouble(cart, tdb, VCF_MIN_QUAL_VAR, VCF_DEFAULT_MIN_QUAL);
    return TRUE;
    }
return FALSE;
}

static boolean minQualFail(struct vcfRecord *record, double minQual)
/* Return TRUE if record's QUAL column value is non-numeric or is less than minQual. */
{
if (isEmpty(record->qual) ||
    (record->qual[0] != '-' && !isdigit(record->qual[0])) ||
    atof(record->qual) < minQual)
    return TRUE;
return FALSE;
}

static boolean getFilterValues(struct trackDb *tdb, struct slName **retValues)
/* Return TRUE and set retValues if cart contains FILTER column values to exclude */
{
if (cartListVarExistsAnyLevel(cart, tdb, FALSE, VCF_EXCLUDE_FILTER_VAR))
    {
    struct slName *selectedValues = cartOptionalSlNameListClosestToHome(cart, tdb, FALSE,
									VCF_EXCLUDE_FILTER_VAR);
    if (retValues != NULL)
	*retValues = selectedValues;
    return TRUE;
    }
return FALSE;
}

static boolean filterColumnFail(struct vcfRecord *record, struct slName *filterValues)
/* Return TRUE if record's FILTER column value(s) matches one of filterValues (from cart). */
{
int i;
for (i = 0;  i < record->filterCount;  i++)
    if (slNameInList(filterValues, record->filters[i]))
	return TRUE;
return FALSE;
}

static boolean getMinFreq(struct trackDb *tdb, double *retMinFreq)
/* Return TRUE and set retMinFreq if cart contains nonzero minimum minor allele frequency. */
{
double minFreq = cartOrTdbDouble(cart, tdb, VCF_MIN_ALLELE_FREQ_VAR, VCF_DEFAULT_MIN_ALLELE_FREQ);
if (minFreq > 0)
    {
    if (retMinFreq != NULL)
	*retMinFreq = minFreq;
    return TRUE;
    }
return FALSE;
}

static boolean minFreqFail(struct vcfRecord *record, double minFreq)
/* Return TRUE if record's INFO include AF (alternate allele frequencies) or AC+AN
 * (alternate allele counts and total count of observed alleles) and the minor allele
 * frequency < minFreq -- or rather, major allele frequency > (1 - minFreq) because
 * variants with > 2 alleles might have some significant minor frequencies along with
 * tiny minor frequencies). */
{
struct vcfFile *vcff = record->file;
boolean gotInfo = FALSE;
double refFreq = 1.0;
double maxAltFreq = 0.0;
int i;
const struct vcfInfoElement *afEl = vcfRecordFindInfo(record, "AF");
const struct vcfInfoDef *afDef = vcfInfoDefForKey(vcff, "AF");
if (afEl != NULL && afDef != NULL && afDef->type == vcfInfoFloat)
    {
    // If INFO includes alt allele freqs, use them directly.
    gotInfo = TRUE;
    for (i = 0;  i < afEl->count;  i++)
	{
	if (afEl->missingData[i])
	    continue;
	double altFreq = afEl->values[i].datFloat;
	refFreq -= altFreq;
	if (altFreq > maxAltFreq)
	    maxAltFreq = altFreq;
	}
    }
else
    {
    // Calculate alternate allele freqs from AC and AN:
    const struct vcfInfoElement *acEl = vcfRecordFindInfo(record, "AC");
    const struct vcfInfoDef *acDef = vcfInfoDefForKey(vcff, "AC");
    const struct vcfInfoElement *anEl = vcfRecordFindInfo(record, "AN");
    const struct vcfInfoDef *anDef = vcfInfoDefForKey(vcff, "AN");
    if (acEl != NULL && acDef != NULL && acDef->type == vcfInfoInteger &&
	anEl != NULL && anDef != NULL && anDef->type == vcfInfoInteger && anEl->count == 1 &&
	anEl->missingData[0] == FALSE)
	{
	gotInfo = TRUE;
	int totalCount = anEl->values[0].datInt;
	for (i = 0;  i < acEl->count;  i++)
	    {
	    if (acEl->missingData[i])
		continue;
	    int altCount = acEl->values[i].datInt;
	    double altFreq = (double)altCount / totalCount;
	    refFreq -= altFreq;
	    if (altFreq < maxAltFreq)
		maxAltFreq = altFreq;
	    }
	}
    else
        // Use MAF for alternate allele freqs from MAF:
        {
        const struct vcfInfoElement *mafEl = vcfRecordFindInfo(record, "MAF");
        const struct vcfInfoDef *mafDef = vcfInfoDefForKey(vcff, "MAF");
        if (mafEl != NULL && mafDef != NULL && mafDef->type == vcfInfoString
        && startsWith("Minor Allele Frequency",mafDef->description))
            {
            // If INFO includes alt allele freqs, use them directly.
            gotInfo = TRUE;

            if (mafEl->count >= 1 && !mafEl->missingData[mafEl->count-1])
                {
                char data[64];
                safecpy(data,sizeof(data),mafEl->values[mafEl->count-1].datString);
                maxAltFreq = atof(lastWordInLine(data));
                refFreq -= maxAltFreq;
                }
            }
        }
    }
if (gotInfo)
    {
    double majorAlFreq = max(refFreq, maxAltFreq);
    if (majorAlFreq > (1.0 - minFreq))
	return TRUE;
    }
return FALSE;
}

static void filterRefOnlyAlleles(struct vcfFile *vcff, struct trackDb *tdb)
/* Drop items from VCF that don't differ from the reference for any of the
 * samples specified in trackDb */
{
struct vcfRecord *rec, *nextRecord, *retList = NULL;
const struct vcfGenotype *gt;

boolean hideOtherSamples = cartUsualBooleanClosestToHome(cart, tdb, FALSE, VCF_PHASED_HIDE_OTHER_VAR, FALSE);
struct slPair *sample, *sampleOrder = vcfPhasedGetSampleOrder(cart, tdb, FALSE, hideOtherSamples);
for (rec = vcff->records; rec != NULL; rec = nextRecord)
    {
    nextRecord = rec->next;
    boolean allRef = TRUE;
    for (sample = sampleOrder; sample != NULL; sample = sample->next)
        {
        gt = vcfRecordFindGenotype(rec, sample->name);
        if (gt && !(gt->hapIxA == 0 && gt->hapIxB == 0) )
            allRef = FALSE;
        }
    if (!allRef)
        slAddHead(&retList, rec);
    }
slReverse(&retList);
vcff->records = retList;
}

static void filterRecords(struct vcfFile *vcff, struct track *tg)
/* If a filter is specified in the cart, remove any records that don't pass filter. Adapt longLabel if something was filtered. */
{
struct trackDb *tdb = tg->tdb;
double minQual = VCF_DEFAULT_MIN_QUAL;
struct slName *filterValues = NULL;
double minFreq = VCF_DEFAULT_MIN_ALLELE_FREQ;
boolean gotQualFilter = getMinQual(tdb, &minQual);
boolean gotFilterFilter = getFilterValues(tdb, &filterValues);
boolean gotMinFreqFilter = getMinFreq(tdb, &minFreq);
int filtOut = 0;
if (gotQualFilter || gotFilterFilter || gotMinFreqFilter) 
    {
    struct vcfRecord *rec, *nextRec, *newList = NULL;
    for (rec = vcff->records;  rec != NULL;  rec = nextRec)
        {
        nextRec = rec->next;
        if (! ((gotQualFilter && minQualFail(rec, minQual)) ||
               (gotFilterFilter && filterColumnFail(rec, filterValues)) ||
               (gotMinFreqFilter && minFreqFail(rec, minFreq)) ))
            slAddHead(&newList, rec);
        else 
            filtOut++;
        }
    slReverse(&newList);
    vcff->records = newList;
    }

labelTrackAsFilteredNumber(tg, filtOut);
}

struct pgSnpVcfStartEnd
/* This extends struct pgSnp by tacking on an original VCF chromStart and End at the end,
 * for use by indelTweakMapItem below.  This can be cast to pgs. */
{
    struct pgSnp pgs;
    unsigned int vcfStart;
    unsigned int vcfEnd;
};

static struct pgSnp *vcfFileToPgSnp(struct vcfFile *vcff, struct trackDb *tdb)
/* Convert vcff's records to pgSnp; don't free vcff until you're done with pgSnp
 * because it contains pointers into vcff's records' chrom. If the trackDb setting
 * sampleName is present, then check whether all the records are phased or not */
{
struct pgSnp *pgsList = NULL;
struct vcfRecord *rec;
int maxLen = 33;
int maxAlCount = 5;
struct slPair *sample = NULL, *phasedSamples = NULL;
if (sameString(tdb->type, "vcfPhasedTrio"))
    {
    boolean hideOtherSamples = cartUsualBooleanClosestToHome(cart, tdb, FALSE, VCF_PHASED_HIDE_OTHER_VAR, FALSE);
    phasedSamples = vcfPhasedGetSampleOrder(cart, tdb, FALSE, hideOtherSamples);
    }

vcff->allPhased = TRUE;
for (rec = vcff->records;  rec != NULL;  rec = rec->next)
    {
    struct pgSnpVcfStartEnd *psvs = needMem(sizeof(*psvs));
    psvs->vcfStart = vcfRecordTrimIndelLeftBase(rec);
    psvs->vcfEnd = vcfRecordTrimAllelesRight(rec);
    for (sample = phasedSamples; sample != NULL; sample = sample->next)
        {
        const struct vcfGenotype *gt = vcfRecordFindGenotype(rec, sample->name);
        if (!gt->isPhased)
            vcff->allPhased = FALSE;
        }
    struct pgSnp *pgs = pgSnpFromVcfRecord(rec);
    memcpy(&(psvs->pgs), pgs, sizeof(*pgs));
    pgs = (struct pgSnp *)psvs; // leak mem
    // Insertion sequences can be quite long; abbreviate here for display.
    int len = strlen(pgs->name);
    if (len > maxLen)
	{
	int maxAlLen = (maxLen / min(rec->alleleCount, maxAlCount)) - 1;
	pgs->name[0] = '\0';
	int i;
	for (i = 0;  i < rec->alleleCount;  i++)
	    {
	    if (i > 0)
		safencat(pgs->name, len+1, "/", 1);
	    if (i >= maxAlCount)
		{
		safecat(pgs->name, len+1, "...");
		pgs->alleleCount = maxAlCount;
		break;
		}
	    if (strlen(rec->alleles[i]) > maxAlLen-3)
		strcpy(rec->alleles[i]+maxAlLen-3, "...");
	    safencat(pgs->name, len+1, rec->alleles[i], maxAlLen);
	    }
	}
    slAddHead(&pgsList, pgs);
    }
slReverse(&pgsList);
return pgsList;
}


// Center-weighted alpha clustering of haplotypes -- see Redmine #3711, #2823 note 7
// It might be nice to use an allele-frequency representation here instead of [ACGTN] strings
// with "N" for missing info or differences, but keep it simple.

struct cwaExtraData
/* Helper data for hacTree clustering of haplotypes by center-weighted alpha distance */
    {
    int center;    // index from which each point's contribution to distance is to be weighted
    int len;       // total length of haplotype strings
    double alpha;  // weighting factor for distance from center
    struct lm *localMem;
    };

// This is the representation of a cluster of up to 65,535 haplotypes of equal length,
// where each variant's alleles are specified as 0 (reference) or 1 (alternate)
// [or possibly 2 for second alternate, but those are rare so I'll ignore them].
// When an individual is heterozygous and unphased for some variant, we need to
// account for missing data.
struct hapCluster
{
    struct hapCluster *next;   // hacTree wants slList of items
    unsigned int *refCounts; // per-variant count of reference alleles observed
    unsigned int *unkCounts; // per-variant count of unknown (or unphased het) alleles
    unsigned int leafCount;  // number of leaves under this node (or 1 if leaf)
    unsigned int gtHapIx;    // if leaf, (genotype index << 1) + hap (0 or 1 for diploid)
};

INLINE boolean isRef(const struct hapCluster *c, int varIx)
// Return TRUE if the leaves of cluster have at least as many reference alleles
// as alternate alleles for variant varIx.
{
unsigned int altCount = c->leafCount - c->refCounts[varIx] - c->unkCounts[varIx];
return (c->refCounts[varIx] >= altCount);
}

static double cwaDistance(const struct slList *item1, const struct slList *item2, void *extraData)
/* Center-weighted alpha sequence distance function for hacTree clustering of haplotype seqs */
// This is inner-loop so I am not doing defensive checks.  Caller must ensure:
// 1. kids's sequences' lengths are both equal to helper->len
// 2. 0 <= helper->center <= len
// 3. 0.0 < helper->alpha <= 1.0
{
const struct hapCluster *kid1 = (const struct hapCluster *)item1;
const struct hapCluster *kid2 = (const struct hapCluster *)item2;
struct cwaExtraData *helper = extraData;
double distance = 0;
double weight = 1; // start at center: alpha to the 0th power
int i;
for (i=helper->center;  i >= 0;  i--)
    {
    if (isRef(kid1, i) != isRef(kid2, i))
	distance += weight;
    weight *= helper->alpha;
    }
weight = helper->alpha; // start at center+1: alpha to the 1st power
for (i=helper->center+1;  i < helper->len;  i++)
    {
    if (isRef(kid1, i) != isRef(kid2, i))
	distance += weight;
    weight *= helper->alpha;
    }
return distance;
}

static struct hapCluster *lmHapCluster(struct cwaExtraData *helper)
/* Use localMem to allocate a new cluster of the given len. */
{
struct hapCluster *c = lmAlloc(helper->localMem, sizeof(struct hapCluster));
c->refCounts = lmAlloc(helper->localMem, helper->len * sizeof(unsigned int));
c->unkCounts = lmAlloc(helper->localMem, helper->len * sizeof(unsigned int));
return c;
}

static struct slList *cwaMerge(const struct slList *item1, const struct slList *item2,
			       void *extraData)
/* Make a consensus haplotype from two input haplotypes, for hacTree clustering by
 * center-weighted alpha distance. */
// This is inner-loop so I am not doing defensive checks.  Caller must ensure that
// kids's sequences' lengths are both equal to helper->len.
{
const struct hapCluster *kid1 = (const struct hapCluster *)item1;
const struct hapCluster *kid2 = (const struct hapCluster *)item2;
struct cwaExtraData *helper = extraData;
struct hapCluster *consensus = lmHapCluster(helper);
consensus->leafCount = kid1->leafCount + kid2->leafCount;
consensus->gtHapIx = kid1->gtHapIx;
int i;
for (i=0;  i < helper->len;  i++)
    {
    consensus->refCounts[i] = kid1->refCounts[i] + kid2->refCounts[i];
    consensus->unkCounts[i] = kid1->unkCounts[i] + kid2->unkCounts[i];
    }
return (struct slList *)consensus;
}

INLINE void hapClusterToString(const struct hapCluster *c, char *s, int len)
/* Write a text representation of hapCluster's alleles into s which is at least len+1 long.  */
{
int i;
for (i=0;  i < len;  i++)
    s[i] = isRef(c, i) ? '0': '1';
s[i] = 0;
}

static int cwaCmp(const struct slList *item1, const struct slList *item2, void *extraData)
/* Convert hapCluster to allele strings for easy comparison by strcmp. */
{
const struct hapCluster *c1 = (const struct hapCluster *)item1;
const struct hapCluster *c2 = (const struct hapCluster *)item2;
struct cwaExtraData *helper = extraData;
char s1[helper->len+1];
char s2[helper->len+1];
hapClusterToString(c1, s1, helper->len);
hapClusterToString(c2, s2, helper->len);
return strcmp(s1, s2);
}

void rSetGtHapOrder(struct hacTree *ht, unsigned int *gtHapOrder, unsigned int *retGtHapEnd)
/* Traverse hacTree and build an ordered array of genotype + haplotype indices. */
{
if (ht->left == NULL && ht->right == NULL)
    {
    struct hapCluster *c = (struct hapCluster *)ht->itemOrCluster;
    gtHapOrder[(*retGtHapEnd)++] = c->gtHapIx;
    }
else if (ht->left == NULL)
    rSetGtHapOrder(ht->right, gtHapOrder, retGtHapEnd);
else if (ht->right == NULL)
    rSetGtHapOrder(ht->left, gtHapOrder, retGtHapEnd);
else
    {
    struct hapCluster *cL = (struct hapCluster *)ht->left->itemOrCluster;
    struct hapCluster *cR = (struct hapCluster *)ht->right->itemOrCluster;
    if (cL->leafCount >= cR->leafCount)
	{
	rSetGtHapOrder(ht->left, gtHapOrder, retGtHapEnd);
	rSetGtHapOrder(ht->right, gtHapOrder, retGtHapEnd);
	}
    else
	{
	rSetGtHapOrder(ht->right, gtHapOrder, retGtHapEnd);
	rSetGtHapOrder(ht->left, gtHapOrder, retGtHapEnd);
	}
    }
}

static unsigned int *clusterHaps(const struct vcfFile *vcff, int centerIx,
				   int startIx, int endIx,
				   unsigned int *retGtHapEnd, struct hacTree **retTree)
/* Given a bunch of VCF records with phased genotypes, build up one haplotype string
 * per chromosome that is the sequence of alleles in all variants (simplified to one base
 * per variant).  Each individual/sample will have two haplotype strings (unless haploid
 * like Y or male X).  Independently cluster the haplotype strings using hacTree with the 
 * center-weighted alpha functions above. Return an array of genotype+haplotype indices
 * in the order determined by the hacTree, and set retGtHapEnd to its length/end. */
{
double alpha = 0.5;
struct lm *lm = lmInit(0);
struct cwaExtraData helper = { centerIx-startIx, endIx-startIx, alpha, lm };
int ploidy = 2; // Assuming diploid genomes here, no XXY, tetraploid etc.
int gtCount = vcff->genotypeCount;
// Make an slList of hapClusters, but allocate in a big block so I can use
// array indexing.
struct hapCluster **hapArray = lmAlloc(lm, sizeof(struct hapCluster *) * gtCount * ploidy);
int i;
for (i=0;  i < ploidy * gtCount;  i++)
    {
    hapArray[i] = lmHapCluster(&helper);
    if (i > 0)
	hapArray[i-1]->next = hapArray[i];
    }
boolean haveHaploid = FALSE;
int varIx;
struct vcfRecord *rec;
for (varIx = 0, rec = vcff->records;  rec != NULL && varIx < endIx;  varIx++, rec = rec->next)
    {
    if (varIx < startIx)
	continue;
    int countIx = varIx - startIx;
    int gtIx;
    for (gtIx=0;  gtIx < gtCount;  gtIx++)
	{
	struct vcfGenotype *gt = &(rec->genotypes[gtIx]);
	struct hapCluster *c1 = hapArray[gtIx];
	struct hapCluster *c2 = hapArray[gtCount + gtIx]; // hardwired ploidy=2
	c1->gtHapIx = gtIx << 1;
	c1->leafCount = 1;
	if (gt->isPhased || gt->isHaploid || (gt->hapIxA == gt->hapIxB))
	    {
	    // first haplotype's counts:
	    if (gt->hapIxA < 0)
		c1->unkCounts[countIx] = 1;
	    else if (gt->hapIxA == 0)
		c1->refCounts[countIx] = 1;
	    if (gt->isHaploid)
		haveHaploid = TRUE;
	    else
		{
		// got second haplotype, fill in its counts:
		c2->gtHapIx = (gtIx << 1) | 1;
		c2->leafCount = 1;
		if (gt->hapIxB < 0)
		    c2->unkCounts[countIx] = 1;
		else if (gt->hapIxB == 0)
		    c2->refCounts[countIx] = 1;
		}
	    }
	else
	    {
	    // Missing data or unphased heterozygote, don't use haplotype info for clustering
	    c2->gtHapIx = (gtIx << 1) | 1;
	    c2->leafCount = 1;
	    c1->unkCounts[countIx] = c2->unkCounts[countIx] = 1;
	    }
	}
    if (haveHaploid)
	{
	// Some array items will have an empty cluster for missing hap2 --
	// trim those from the linked list.
	struct hapCluster *c = hapArray[0];
	while (c != NULL && c->next != NULL)
	    {
	    if (c->next->leafCount == 0)
		c->next = c->next->next;
	    else
		c = c->next;
	    }
	}
    }
struct hacTree *ht = hacTreeFromItems((struct slList *)(hapArray[0]), lm,
				      cwaDistance, cwaMerge, cwaCmp, &helper);
unsigned int *gtHapOrder = needMem(vcff->genotypeCount * ploidy * sizeof(unsigned int));
rSetGtHapOrder(ht, gtHapOrder, retGtHapEnd);
*retTree = ht;
return gtHapOrder;
}

INLINE Color pgSnpColor(char *allele)
/* Color allele by first base according to pgSnp palette. */
{
if (allele[0] == 'A')
    return revCmplDisp ? MG_MAGENTA : MG_RED;
else if (allele[0] == 'C')
    return revCmplDisp ? darkGreenColor : MG_BLUE;
else if (allele[0] == 'G')
    return revCmplDisp ? MG_BLUE : darkGreenColor;
else if (allele[0] == 'T')
    return revCmplDisp ? MG_RED : MG_MAGENTA;
else
    return shadesOfGray[5];
}

INLINE void abbrevAndHandleRC(char *abbrevAl, size_t abbrevAlSize, const char *fullAl)
/* Limit the size of displayed allele to abbrevAlSize-1 and handle reverse-complemented display. */
{
boolean fullLen = strlen(fullAl);
boolean truncating = (fullLen > abbrevAlSize-1);
if (truncating)
    {
    int truncLen = abbrevAlSize - 4;
    if (revCmplDisp)
	{
	safencpy(abbrevAl, abbrevAlSize, (fullAl + fullLen - truncLen), truncLen);
	reverseComplement(abbrevAl, truncLen);
	}
    else
	safencpy(abbrevAl, abbrevAlSize, fullAl, truncLen);
    safecat(abbrevAl, abbrevAlSize, "...");
    }
else
    {
    safecpy(abbrevAl, abbrevAlSize, fullAl);
    if (revCmplDisp)
	reverseComplement(abbrevAl, fullLen);
    }
}

INLINE void gtSummaryString(struct vcfRecord *rec, struct dyString *dy)
// Make pgSnp-like mouseover text, but with genotype counts instead of allele counts.
{
if (isNotEmpty(rec->name) && !sameString(rec->name, "."))
    dyStringPrintf(dy, "%s: ", rec->name);
if (rec->alleleCount < 2)
    {
    dyStringAppendC(dy, '?');
    return;
    }
int *gtCounts = NULL, *alCounts = NULL;;
int phasedGts = 0, diploidCount = 0;
vcfCountGenotypes(rec, &gtCounts, &alCounts, &phasedGts, &diploidCount);
size_t abbrevSize = 16;
char displayAls[rec->alleleCount][abbrevSize];
int i;
for (i = 0;  i < rec->alleleCount;  i++)
    abbrevAndHandleRC(displayAls[i], sizeof displayAls[i], rec->alleles[i]);
if (diploidCount == 0)
    {
    // No diploid genotypes, just allele counts.
    for (i = 0;  i < rec->alleleCount;  i++)
        {
        if (i > 0)
            dyStringAppendC(dy, ' ');
        dyStringPrintf(dy, "%s:%d", displayAls[i], alCounts[i]);
        }
    }
else
    {
    dyStringPrintf(dy, "%s/%s:%d", displayAls[0], displayAls[0], gtCounts[0]);
    for (i = 1;  i < rec->alleleCount + 1;  i++)
        {
        int j;
        for (j = 0;  j <= i;  j++)
            {
            int gtIx = vcfGenotypeIndex(j, i);
            if (gtCounts[gtIx] > 0)
                {
                char *alJ = (j == rec->alleleCount) ? "?" : displayAls[j];
                char *alI = (i == rec->alleleCount) ? "?" : displayAls[i];
                dyStringPrintf(dy, "; %s/%s:%d", alJ, alI, gtCounts[gtIx]);
                }
            }
        }
    }
}

void mapBoxForCenterVariant(struct vcfRecord *rec, struct hvGfx *hvg, struct track *tg,
			    int xOff, int yOff, int width)
/* Special mouseover for center variant */
{
struct dyString *dy = dyStringNew(0);
unsigned int chromStartMap = vcfRecordTrimIndelLeftBase(rec);
unsigned int chromEndMap = vcfRecordTrimAllelesRight(rec);
gtSummaryString(rec, dy);
dyStringAppend(dy, "   Haplotypes sorted on ");
char *centerChrom = cartOptionalStringClosestToHome(cart, tg->tdb, FALSE, "centerVariantChrom");
if (centerChrom == NULL || !sameString(chromName, centerChrom))
    dyStringAppend(dy, "middle variant by default. ");
else
    dyStringAppend(dy, "this variant. ");
dyStringAppend(dy, "To anchor sorting to a different variant, click on that variant and "
	       "then click on the 'Use this variant' button below the variant name.");
const double scale = scaleForPixels(width);
int x1 = round((double)(rec->chromStart-winStart)*scale) + xOff;
int x2 = round((double)(rec->chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
if (w <= 1)
    {
    x1--;
    w = 3;
    }
mapBoxHgcOrHgGene(hvg, chromStartMap, chromEndMap, x1, yOff, w, tg->height, tg->track,
		  rec->name, dy->string, NULL, TRUE, NULL);
}

// These are initialized when we start drawing, then constant.
static Color purple = 0;
static Color undefYellow = 0;

enum hapColorMode
    {
    altOnlyMode,
    refAltMode,
    baseMode,
    functionMode
    };

static Color colorByAltShade(int refs, int alts, int unks, Color colorShades[], int colorShadeCount)
/* Coloring alternate alleles only by shades of color: shade by proportion of alts */
{
if (unks > (refs + alts))
    return undefYellow;
int total = refs + alts + unks;
int shadeIx = (alts * colorShadeCount) / total;
if (shadeIx == colorShadeCount)
    shadeIx--;
return colorShades[shadeIx];
}

static Color colorByAltOnly(int refs, int alts, int unks)
/* Coloring alternate alleles only: shade by proportion of alt alleles */
{
return colorByAltShade(refs, alts, unks, shadesOfGray, maxShade+1);
}

static Color colorByRefAlt(int refs, int alts, int unks)
/* Color blue for reference allele, red for alternate allele, gray for unknown, purple
 * for reasonably mixed. */
{
const int fudgeFactor = 4; // Threshold factor for calling one color or the other when mixed
if (unks > (refs + alts))
    return undefYellow;
if (alts > fudgeFactor * refs)
    return MG_RED;
if (refs > fudgeFactor * alts)
    return MG_BLUE;
return purple;
}

static Color colorByBase(int refs, int alts, int unks, char *refAl, char *altAl)
/* Color gray for unknown or mixed, otherwise pgSnpColor of predominant allele. */
{
const int fudgeFactor = 4; // Threshold for calling for one color or the other when mixed
if (unks > (refs + alts))
    return undefYellow;
if (alts > fudgeFactor * refs)
    return pgSnpColor(altAl);
if (refs > fudgeFactor * alts)
    return pgSnpColor(refAl);
return shadesOfGray[5];
}

static struct dnaSeq *genePredToGenomicSequence(struct genePred *pred, struct seqWindow *gSeqWin)
/* Return concatenated genomic sequence of exons of pred. */
{
int txLen = 0;
int i;
for (i=0; i < pred->exonCount; i++)
    txLen += (pred->exonEnds[i] - pred->exonStarts[i]);
char *seq = needMem(txLen + 1);
int offset = 0;
for (i=0; i < pred->exonCount; i++)
    {
    int exonStart = pred->exonStarts[i];
    int exonEnd = pred->exonEnds[i];
    int exonLen = exonEnd - exonStart;
    seqWindowCopy(gSeqWin, exonStart, exonLen, seq+offset, txLen+1-offset);
    offset += exonLen;
    }
if(pred->strand[0] == '-')
    reverseComplement(seq, txLen);
struct dnaSeq *txSeq = NULL;
AllocVar(txSeq);
txSeq->name = cloneString(pred->name);
txSeq->dna = seq;
txSeq->size = txLen;
return txSeq;
}

struct txInfo
/* Transcript sequence and alignment needed for prediction of functional effect/consequences */
    {
    struct txInfo *next;
    struct psl *psl;            // alignment of transcript to genome
    struct dnaSeq *txSeq;       // transcript sequence
    struct genbankCds *cds;     // transcript CDS (possible none)
    struct dnaSeq *protSeq;     // transcript protein sequence (possibly NULL)

    };

static struct txInfo *txInfoFromGenePred(struct genePred *gp, struct seqWindow *gSeqWin)
/* Use gp and gSeqWin to construct transcript alignment, sequence and protein sequence if applic. */
{
struct txInfo *txi;
AllocVar(txi);
AllocVar(txi->cds);
genePredToCds(gp, txi->cds);
txi->txSeq = genePredToGenomicSequence(gp, gSeqWin);
int chromSize = 0;  // unused
txi->psl = genePredToPsl(gp, chromSize, txi->txSeq->size);
pslRemoveFrameShifts(txi->psl);
vpExpandIndelGaps(txi->psl, gSeqWin, txi->txSeq);
txi->protSeq = NULL;
if (txi->cds->end > txi->cds->start && txi->cds->startComplete)
    {
    txi->protSeq = translateSeq(txi->txSeq, txi->cds->start, FALSE);
    aaSeqZToX(txi->protSeq);
    }
return txi;
}

static struct hash *hashExtNcbiRefSeq(struct sqlConnection *conn)
/* Despite the seq/extFile structure, there is only one external file and we don't want to
 * keep opening and closing the file each time.  Just in case there are multiple files someday,
 * hash extNcbiRefSeq id to open udc file handle(s). */
{
struct hash *extNcbi = hashNew(0);
char query[1024];
sqlSafef(query, sizeof query, "select id, path from extNcbiRefSeq");
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *extId = row[0];
    char *path = row[1];
    struct udcFile *udcF = udcFileOpen(path, NULL);
    hashAdd(extNcbi, extId, udcF);
    }
sqlFreeResult(&sr);
return extNcbi;
}

static void freeExtNcbiHash(struct hash **pExtNcbi)
/* Clean up hash of open udcFiles created by hashExtNcbiRefSeq. */
{
if (pExtNcbi && *pExtNcbi)
    {
    struct hash *hash = *pExtNcbi;
    struct hashCookie cookie = hashFirst(hash);
    struct hashEl *hel;
    while ((hel = hashNext(&cookie)) != NULL)
        udcFileClose((struct udcFile **)&hel->val);
    hashFree(pExtNcbi);
    }
}

static struct txInfo *txInfoInitFromPsl(struct sqlConnection *conn, char *pslTable,
                                        char *extraWhere, struct hash **retTxiHash)
/* Alloc and return a list of txInfo with only psl populated, from pslTable in the current window.
 * Also return a hash of transcript ID to txInfo (retTxiHash). */
{
struct txInfo *txiList = NULL;
struct hash *txiHash = hashNew(0);
int binOffset = 0;
int start = max(0, winStart - GPRANGE);
int end = winEnd + GPRANGE;
struct sqlResult *sr = hRangeQuery(conn, pslTable, chromName, start, end, extraWhere, &binOffset);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct txInfo *txi;
    AllocVar(txi);
    txi->psl = pslLoad(row+binOffset);
    slAddHead(&txiList, txi);
    hashAdd(txiHash, txi->psl->qName, txi);
    }
sqlFreeResult(&sr);
*retTxiHash = txiHash;
return txiList;
}

static void txiInfoAppendIdList(struct dyString *query, struct txInfo *txiList)
/* Append a paren-enclosed list of quoted transcript IDs to query. */
{
sqlDyStringPrintf(query, "(");
struct txInfo *txi;
for (txi = txiList;  txi != NULL;  txi = txi->next)
    {
    if (txi != txiList)
        sqlDyStringPrintf(query, ", ");
    sqlDyStringPrintf(query, "'%s'", txi->psl->qName);
    }
sqlDyStringPrintf(query, ")");
}

static void txInfoAddCdsFromQuery(struct hash *txiHash, struct sqlConnection *conn, char *query)
/* Perform query; results have two fields, transcript name (which must be found in txiHash) and
 * GenBank-formatted CDS string.  For reach row from the query, parse the CDS string into the cds
 * in the struct txInfo for the appropriate transcript. */
{
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    char *cdsStr = row[1];
    struct txInfo *txi = hashMustFindVal(txiHash, name);
    AllocVar(txi->cds);
    genbankCdsParse(cdsStr, txi->cds);
    }
sqlFreeResult(&sr);
}

static struct txInfo *txInfoLoadNcbiRefSeq(struct seqWindow *gSeqWin, struct trackDb *gTdb)
/* Load ncbiRefSeq[Curated] PSL (+ CDS and sequence) in current window and make txInfo for each. */
{
struct txInfo *txiList = NULL;
if (!trackHubDatabase(database) && hDbHasNcbiRefSeq(database))
    {
    struct sqlConnection *conn = hAllocConn(database);
    struct hash *txiHash = hashNew(0);
    char extraWhere[1024];
    char *extra = NULL;
    if (sameString(gTdb->track, "ncbiRefSeqCurated"))
	{
        sqlSafef(extraWhere, sizeof extraWhere, "qName not like 'X%%'");
	extra = extraWhere;
	}
    else if (sameString(gTdb->track, "ncbiRefSeqPredicted"))
	{
        sqlSafef(extraWhere, sizeof extraWhere, "qName like 'X%%'");
	extra = extraWhere;
	}
    txiList = txInfoInitFromPsl(conn, "ncbiRefSeqPsl", extra, &txiHash);
    if (txiList)
        {
        // Now get CDS for each psl/txi:
        struct dyString *query = sqlDyStringCreate("select * from ncbiRefSeqCds where id in ");
        txiInfoAppendIdList(query, txiList);
        txInfoAddCdsFromQuery(txiHash, conn, query->string);
        // Now get transcript sequence for each psl/txi:
        struct hash *extNcbi = hashExtNcbiRefSeq(conn);
        dyStringClear(query);
        sqlDyStringPrintf(query, "select acc,extFile,file_offset,file_size from seqNcbiRefSeq "
                          "where acc in ");
        txiInfoAppendIdList(query, txiList);
        struct sqlResult *sr = sqlGetResult(conn, query->string);
        char **row;
        while ((row = sqlNextRow(sr)) != NULL)
            {
            char *name = row[0];
            char *extId = row[1];
            off_t offset = sqlLongLong(row[2]);
            size_t size = sqlUnsigned(row[3]);
            struct udcFile *udcF = hashMustFindVal(extNcbi, extId);
            char *buf = needMem(size+1);
            udcSeek(udcF, offset);
            udcRead(udcF, buf, size);
            struct txInfo *txi = hashMustFindVal(txiHash, name);
            txi->txSeq = faSeqFromMemText(buf, TRUE);
            }
        sqlFreeResult(&sr);
        freeExtNcbiHash(&extNcbi);
        // Now get protein sequence (if applicable) for each psl/txi:
        dyStringClear(query);
        sqlDyStringPrintf(query, "select id, protAcc, seq from ncbiRefSeqLink nl, ncbiRefSeqPepTable np "
                          "where nl.protAcc = np.name and id in ");
        txiInfoAppendIdList(query, txiList);
        sr = sqlGetResult(conn, query->string);
        while ((row = sqlNextRow(sr)) != NULL)
            {
            char *txId = row[0];
            char *protId = row[1];
            char *protSeq = cloneString(row[2]);
            struct txInfo *txi = hashMustFindVal(txiHash, txId);
            txi->protSeq = newDnaSeq(protSeq, strlen(protSeq), protId);
            }
        sqlFreeResult(&sr);
        hFreeConn(&conn);
        }
    }
return txiList;
}

static struct txInfo *txInfoLoadRefGene(struct seqWindow *gSeqWin, struct trackDb *gTdb)
/* Load refSeqAli PSL (+ genbank CDS and sequence) in current window and make txInfo for each. */
{
struct txInfo *txiList = NULL;
if (!trackHubDatabase(database))
    {
    initGenbankTableNames(database);
    struct sqlConnection *conn = hAllocConn(database);
    struct hash *txiHash = NULL;
    txiList = txInfoInitFromPsl(conn, "refSeqAli", NULL, &txiHash);
    if (txiList)
        {
        // Now get CDS for each psl/txi:
        struct dyString *query = sqlDyStringCreate("select i.acc, c.name from %s i, %s c "
                                                   "where c.id = i.cds and i.acc in ",
                                                   gbCdnaInfoTable, cdsTable);
        txiInfoAppendIdList(query, txiList);
        txInfoAddCdsFromQuery(txiHash, conn, query->string);
        // Now get transcript and translated sequence for each psl/txi:
        struct txInfo *txi;
        for (txi = txiList;  txi != NULL;  txi = txi->next)
            {
            txi->txSeq = hGenBankGetMrna(database, txi->psl->qName, NULL);
            if (txi->cds->end > txi->cds->start && txi->cds->startComplete)
                {
                txi->protSeq = translateSeq(txi->txSeq, txi->cds->start, FALSE);
                aaSeqZToX(txi->protSeq);
                }
            }
        }
    }
return txiList;
}

static struct txInfo *txInfoLoadBigGenePred(struct seqWindow *gSeqWin, struct trackDb *gTdb)
/* Load up bigGenePred items in current window and make txInfo for each. */
{
struct txInfo *txiList = NULL;
char *fileName = cloneString(trackDbSetting(gTdb, "bigDataUrl"));
if (fileName == NULL)
    fileName = cloneString(trackDbSetting(gTdb, "bigGeneDataUrl"));
if (isNotEmpty(fileName))
    {
    struct bbiFile *bbi =  bigBedFileOpenAlias(hReplaceGbdb(fileName), chromAliasFindAliases);
    struct lm *lm = lmInit(0);
    struct bigBedInterval *bbList = bigBedIntervalQuery(bbi, chromName, winStart,
                                                        winEnd, 0, lm);
    struct bigBedInterval *bb;
    for (bb = bbList; bb != NULL; bb = bb->next)
        {
        struct genePredExt *gp = genePredFromBigGenePred(chromName, bb);
        struct txInfo *txi = txInfoFromGenePred((struct genePred *)gp, gSeqWin);
        slAddHead(&txiList, txi);
        }
    bbiFileClose(&bbi);
    lmCleanup(&lm);
    }
return txiList;
}

static struct txInfo *txInfoLoadGenePred(struct seqWindow *gSeqWin, struct trackDb *gTdb)
/* Load genePreds in current window and make txInfo for each. */
{
struct txInfo *txiList = NULL;
if (! trackHubDatabase(database))
    {
    struct sqlConnection *conn = hAllocConn(database);
    struct genePredReader *gpr = genePredReaderRangeQuery(conn, gTdb->table,
                                                          chromName, winStart, winEnd, NULL);
    struct genePred *gp = NULL;
    while ((gp = genePredReaderNext(gpr)) != NULL)
        {
        struct txInfo *txi = txInfoFromGenePred(gp, gSeqWin);
        slAddHead(&txiList, txi);
        }
    genePredReaderFree(&gpr);
    hFreeConn(&conn);
    }
return txiList;
}

static struct txInfo *txInfoLoad(struct seqWindow *gSeqWin, struct trackDb *tdb)
/* Return a list of transcript sequences and alignments for all transcripts in the current
 * window for all enabled gene tracks. */
{
struct txInfo *txiList = NULL;
char *geneTrack = cartOrTdbString(cart, tdb, "geneTrack", NULL);
struct track *gTrack = hashFindVal(trackHash, geneTrack);
if (gTrack)
    {
    struct trackDb *gTdb = gTrack->tdb;
    // If independent transcript sequence and PSL are available, use them.
    if (startsWith("ncbiRefSeq", geneTrack))
        {
        txiList = txInfoLoadNcbiRefSeq(gSeqWin, gTdb);
        }
    else if (sameString(geneTrack, "refGene"))
        {
        txiList = txInfoLoadRefGene(gSeqWin, gTdb);
        }
    else if (sameString(gTdb->type, "genePred") || sameString(gTdb->type, "bigGenePred"))
        {
        // If the track is visible, then struct genePred items have already been loaded so
        // just use them.  Otherwise load up.
        if (gTrack->items)
            {
            struct linkedFeatures *lf;
            for (lf = gTrack->items;  lf != NULL;  lf = lf->next)
                {
                struct genePred *gp = lf->original;
                struct txInfo *txi = txInfoFromGenePred(gp, gSeqWin);
                slAddHead(&txiList, txi);
                }
            }
        else if (sameString(gTdb->type, "bigGenePred"))
            txiList = txInfoLoadBigGenePred(gSeqWin, gTdb);
        else
            txiList = txInfoLoadGenePred(gSeqWin, gTdb);
        }
    else
        errAbort("VCF txInfoLoad: expecting type 'genePred' or 'bigGenePred' for track '%s' "
                 "in geneTrack setting, but got type '%s'",
                 geneTrack, gTdb->type);
    }
else if (trackImgOnly)
    {
    // For AJAX requests to redraw a single track, we have not loaded the whole trackDb,
    // so see if we can find tdb for geneTrack, and load its items.
    struct trackDb *gTdb = hTrackDbForTrack(database, geneTrack);
    if (gTdb)
        {
        if (startsWith("ncbiRefSeq", geneTrack))
            txiList = txInfoLoadNcbiRefSeq(gSeqWin, gTdb);
        else if (sameString(geneTrack, "refGene"))
            txiList = txInfoLoadRefGene(gSeqWin, gTdb);
        else if (sameString(gTdb->type, "bigGenePred"))
            txiList = txInfoLoadBigGenePred(gSeqWin, gTdb);
        else
            txiList = txInfoLoadGenePred(gSeqWin, gTdb);
        }
    }
return txiList;
}

static enum soTerm functionForRecord(struct vcfRecord *rec, struct seqWindow *gSeqWin,
                                     struct txInfo *txiList)
/* Return the most severe functional consequence of rec for any transcript in txiList. */
{
struct lm *lm = lmInit(0);
// Can't use rec->chrom because that might be just "1" instead of "chr1":
struct bed3 variantBed3 = { NULL, chromName, rec->chromStart, rec->chromEnd };
enum soTerm maxImpactTerm = soUnknown;
struct txInfo *txi;
for (txi = txiList;  txi != NULL;  txi = txi->next)
    {
    int alIx;
    // Sometimes reference allele is actually a change to the transcript
    for (alIx = 0;  alIx < rec->alleleCount;  alIx++)
        {
        char *allele = rec->alleles[alIx];
        // Watch out for weird symbolic alleles like "<INS:ME:ALU>".
        if (sameString(allele, "<DEL>"))
            allele = "";
        else if (allele[0] == '<')
            continue;
        // Ignore IUPAC ambiguous values in alts (can leak in from hgPhyloPlace)
        if (anyIupac(allele))
            continue;
        struct vpTx *vpTx = vpGenomicToTranscript(gSeqWin, &variantBed3, allele,
                                                  txi->psl, txi->txSeq);
        struct vpPep *vpPep = vpTranscriptToProtein(vpTx, txi->cds, txi->txSeq, txi->protSeq);
        struct gpFx *gpFx = vpTranscriptToGpFx(vpTx, txi->psl, txi->cds, txi->txSeq, vpPep,
                                               txi->protSeq, lm);
        enum soTerm term = gpFx->soNumber;
        if (soTermCmp(&term, &maxImpactTerm) < 0)
            maxImpactTerm = term;
        vpPepFree(&vpPep);
        vpTxFree(&vpTx);
        }
    }
lmCleanup(&lm);
return maxImpactTerm;
}

// tg->height needs an extra pixel at the bottom; it's eaten by the clipping rectangle:
#define CLIP_PAD 1

static void drawOneRec(struct vcfRecord *rec, unsigned int *gtHapOrder, unsigned int gtHapCount,
		       struct track *tg, struct hvGfx *hvg, int xOff, int yOff, int width,
		       boolean isClustered, boolean isCenter, enum hapColorMode colorMode,
                       enum soTerm funcTerm)
/* Draw a stack of genotype bars for this record */
{
unsigned int chromStartMap = vcfRecordTrimIndelLeftBase(rec);
unsigned int chromEndMap = vcfRecordTrimAllelesRight(rec);
const double scale = scaleForPixels(width);
int x1 = round((double)(rec->chromStart-winStart)*scale) + xOff;
int x2 = round((double)(rec->chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
if (w <= 1)
    {
    x1--;
    w = 3;
    }
// When coloring mode is altOnly, we draw one extra pixel row at the top & one at bottom
// to show the locations of variants, since the reference alleles are invisible:
int extraPixel = 0;
int hapHeight = tg->height - CLIP_PAD;
if (colorMode == altOnlyMode || colorMode == functionMode)
    {
    hvGfxLine(hvg, x1, yOff, x2, yOff, (isClustered ? purple : shadesOfGray[5]));
    extraPixel = 1;
    hapHeight -= extraPixel*2;
    }
double hapsPerPix = (double)gtHapCount / hapHeight;
int pixIx;
for (pixIx = 0;  pixIx < hapHeight;  pixIx++)
    {
    int gtHapOrderIxStart = (int)(hapsPerPix * pixIx);
    int gtHapOrderIxEnd = round(hapsPerPix * (pixIx + 1));
    if (gtHapOrderIxEnd == gtHapOrderIxStart)
	gtHapOrderIxEnd++;
    int unks = 0, refs = 0, alts = 0;
    int gtHapOrderIx;
    for (gtHapOrderIx = gtHapOrderIxStart;  gtHapOrderIx < gtHapOrderIxEnd;  gtHapOrderIx++)
	{
	int gtHapIx = gtHapOrder[gtHapOrderIx];
	int hapIx = gtHapIx & 1;
	int gtIx = gtHapIx >>1;
	struct vcfGenotype *gt = &(rec->genotypes[gtIx]);
	if (gt->isPhased || gt->isHaploid || (gt->hapIxA == gt->hapIxB))
	    {
	    int alIx = hapIx ? gt->hapIxB : gt->hapIxA;
	    if (alIx < 0)
		unks++;
	    else if (alIx > 0)
		alts++;
	    else
		refs++;
	    }
	else
	    unks++;
	}
    int y = yOff + extraPixel + pixIx;
    Color col;
    if (colorMode == baseMode)
	col = colorByBase(refs, alts, unks, rec->alleles[0], rec->alleles[1]);
    else if (colorMode == refAltMode)
	col = colorByRefAlt(refs, alts, unks);
    else if (colorMode == functionMode)
        {
        Color *funcShades = shadesOfGray;
        int funcShadeCount = maxShade+1;
        Color funcColor = colorFromSoTerm(funcTerm);
        if (funcColor == MG_RED)
            {
            funcShades = shadesOfRedOnWhite;
            funcShadeCount = ArraySize(shadesOfRedOnWhite);
            }
        else if (funcColor == MG_GREEN)
            {
            funcShades = darkerShadesOfGreenOnWhite;
            funcShadeCount = ArraySize(darkerShadesOfGreenOnWhite);
            }
        else if (funcColor == MG_BLUE)
            {
            funcShades = shadesOfBlueOnWhite;
            funcShadeCount = ArraySize(shadesOfBlueOnWhite);
            }
        col = colorByAltShade(refs, alts, unks, funcShades, funcShadeCount);
        }
    else
	col = colorByAltOnly(refs, alts, unks);
    if (col != MG_WHITE)
	hvGfxLine(hvg, x1, y, x2, y, col);
    }
int yBot = yOff + tg->height - CLIP_PAD - 1;
if (isCenter)
    {
    if (colorMode == altOnlyMode || colorMode == functionMode)
	{
	// Colorful outline to distinguish this variant:
	hvGfxLine(hvg, x1-1, yOff, x1-1, yBot, purple);
	hvGfxLine(hvg, x2+1, yOff, x2+1, yBot, purple);
	hvGfxLine(hvg, x1-1, yOff, x2+1, yOff, purple);
	hvGfxLine(hvg, x1-1, yBot, x2+1, yBot, purple);
	}
    else
	{
	// Thick black lines to distinguish this variant:
	hvGfxBox(hvg, x1-3, yOff, 3, tg->height, MG_BLACK);
	hvGfxBox(hvg, x2, yOff, 3, tg->height, MG_BLACK);
	hvGfxLine(hvg, x1-2, yOff, x2+2, yOff, MG_BLACK);
	hvGfxLine(hvg, x1-2, yBot, x2+2, yBot, MG_BLACK);
	}
    // Mouseover was handled already by mapBoxForCenterVariant
    }
else
    {
    struct dyString *dy = dyStringNew(0);
    gtSummaryString(rec, dy);
    mapBoxHgcOrHgGene(hvg, chromStartMap, chromEndMap, x1, yOff, w, tg->height, tg->track,
		      rec->name, dy->string, NULL, TRUE, NULL);
    }
if (colorMode == altOnlyMode || colorMode == functionMode)
    hvGfxLine(hvg, x1, yBot, x2, yBot, (isClustered ? purple : shadesOfGray[5]));
}

static int getCenterVariantIx(struct track *tg, int seqStart, int seqEnd,
			      struct vcfRecord *records)
// If the user hasn't specified a local variant/position to use as center,
// just use the median variant in window.
{
int defaultIx = (slCount(records)-1) / 2;
char *centerChrom = cartOptionalStringClosestToHome(cart, tg->tdb, FALSE, "centerVariantChrom");
if (centerChrom != NULL && sameString(chromName, centerChrom))
    {
    int centerPos = cartUsualIntClosestToHome(cart, tg->tdb, FALSE, "centerVariantPos", -1);
    int winSize = seqEnd - seqStart;
    if (centerPos > (seqStart - winSize) && centerPos < (seqEnd + winSize))
	{
	int i;
	struct vcfRecord *rec;
	for (rec = records, i = 0;  rec != NULL;  rec = rec->next, i++)
	    if (rec->chromStart >= centerPos)
		return i;
	return i-1;
	}
    }
return defaultIx;
}

/* Data used when drawing mouseover titles for clusters:  */
struct titleHelper
    {
    char *track;           // For imageV2's map item code
    int startIx;           // Index of first record (variant) used in clustering
    int centerIx;          // Index of center record (variant) used in clustering
    int endIx;             // Half-open index of last record (variant) used in clustering
    int nRecords;          // Total number of records in position range
    char **refs;           // Array of reference alleles for records used in clustering
    char **alts;           // Array of alternate alleles for records used in clustering
                           // (refs[0] and alts[0] are from record at startIx)
    };

void addClusterMapItem(struct hacTree *ht, int x1, int y1, int x2, int y2, struct titleHelper *helper)
/* If using imageV2, add mouseover text (no link) with info about this cluster. */
{
if (theImgBox && curImgTrack)
    {
    struct dyString *dy = dyStringNew(0);
    struct hapCluster *c = (struct hapCluster *)ht->itemOrCluster;
    dyStringPrintf(dy, "N=%d ", c->leafCount);
    while (dyStringLen(dy) < 7)
	dyStringAppendC(dy, ' ');
    if (helper->startIx > 0)
	dyStringAppend(dy, "...");
    int i, nHapsForClustering = helper->endIx - helper->startIx;
    for (i=0;  i < nHapsForClustering;  i++)
	{
	boolean isCenter = (helper->startIx+i == helper->centerIx);
	char *allele = isRef(c, i) ? helper->refs[i] : helper->alts[i];
	if (isCenter)
	    dyStringAppendC(dy, '[');
	int altCount = c->leafCount - c->refCounts[i] - c->unkCounts[i];
	if (c->refCounts[i] > 0 && altCount > 0)
	    dyStringAppendC(dy, '*');
	else if (strlen(allele) == 1)
	    dyStringAppendC(dy, allele[0]);
	else
	    dyStringPrintf(dy, "(%s)", allele);
	if (isCenter)
	    dyStringAppendC(dy, ']');
	}
    if (helper->endIx < helper->nRecords)
	dyStringAppend(dy, "...");
    imgTrackAddMapItem(curImgTrack, TITLE_BUT_NO_LINK, dy->string,
		       x1, y1, x2, y2, helper->track, dy->string);
    }
}

/* Pixel y offset return type for recursive tree-drawing: */
enum yRetType
    {
    yrtMidPoint,
    yrtStart,
    yrtEnd,
    };

/* Callback for calculating y (in pixels) for a cluster node: */
typedef int yFromNodeFunc(const struct slList *itemOrCluster, void *extraData,
			  enum yRetType yType);

static int rDrawTreeInLabelArea(struct hacTree *ht, struct hvGfx *hvg, enum yRetType yType, int x,
				yFromNodeFunc *yFromNode, void *yh, struct titleHelper *th,
				boolean drawRectangle)
/* Recursively draw the haplotype clustering tree in the left label area.
 * Returns pixel height for use at non-leaf levels of tree. */
{
const int branchW = 4;
int labelEnd = leftLabelX + leftLabelWidth;
if (yType == yrtStart || yType == yrtEnd)
    {
    // We're just getting vertical span of a leaf cluster, not drawing any lines.
    int yLeft, yRight;
    if (ht->left)
	yLeft = rDrawTreeInLabelArea(ht->left, hvg, yType, x, yFromNode, yh, th, drawRectangle);
    else
	yLeft = yFromNode(ht->itemOrCluster, yh, yType);
    if (ht->right)
	yRight = rDrawTreeInLabelArea(ht->right, hvg, yType, x, yFromNode, yh, th, drawRectangle);
    else
	yRight = yFromNode(ht->itemOrCluster, yh, yType);
    if (yType == yrtStart)
	return min(yLeft, yRight);
    else
	return max(yLeft, yRight);
    }
// Otherwise yType is yrtMidPoint.  If we have 2 children, we'll be drawing some lines:
if (ht->left != NULL && ht->right != NULL)
    {
    int midY;
    if (ht->childDistance == 0 || x+(2*branchW) > labelEnd)
	{
	// Treat this as a leaf cluster.
	// Recursing twice is wasteful. Could be avoided if this, and yFromNode,
	// returned both yStart and yEnd. However, the time to draw a tree of
	// 2188 hap's (1kG phase1 interim) is in the noise, so I consider it
	// not worth the effort of refactoring to save a sub-millisecond here.
	int yStartLeft = rDrawTreeInLabelArea(ht->left, hvg, yrtStart, x+branchW,
					      yFromNode, yh, th, drawRectangle);
	int yEndLeft = rDrawTreeInLabelArea(ht->left, hvg, yrtEnd, x+branchW,
					    yFromNode, yh, th, drawRectangle);
	int yStartRight = rDrawTreeInLabelArea(ht->right, hvg, yrtStart, x+branchW,
					       yFromNode, yh, th, drawRectangle);
	int yEndRight = rDrawTreeInLabelArea(ht->right, hvg, yrtEnd, x+branchW,
					     yFromNode, yh, th, drawRectangle);
	int yStart = min(yStartLeft, yStartRight);
	int yEnd = max(yEndLeft, yEndRight);
	midY = (yStart + yEnd) / 2;
	Color col = (ht->childDistance == 0) ? purple : MG_BLACK;
	if (drawRectangle || ht->childDistance != 0)
	    {
	    hvGfxLine(hvg, x+branchW, yStart, x+branchW, yEnd-1, col);
	    hvGfxLine(hvg, x+branchW, yStart, labelEnd, yStart, col);
	    hvGfxLine(hvg, x+branchW, yEnd-1, labelEnd, yEnd-1, col);
	    }
	else
	    {
	    hvGfxLine(hvg, x, midY, x+1, midY, col);
	    hvGfxLine(hvg, x+1, midY, labelEnd-1, yStart, col);
	    hvGfxLine(hvg, x+1, midY, labelEnd-1, yEnd-1, col);
	    }
	addClusterMapItem(ht, x, yStart, labelEnd, yEnd-1, th);
	}
    else
	{
	int leftMid = rDrawTreeInLabelArea(ht->left, hvg, yrtMidPoint, x+branchW,
					   yFromNode, yh, th, drawRectangle);
	int rightMid = rDrawTreeInLabelArea(ht->right, hvg, yrtMidPoint, x+branchW,
					    yFromNode, yh, th, drawRectangle);
	midY = (leftMid + rightMid) / 2;
	hvGfxLine(hvg, x+branchW, leftMid, x+branchW, rightMid, MG_BLACK);
	addClusterMapItem(ht, x, min(leftMid, rightMid), x+branchW-1, max(leftMid, rightMid), th);
	}
    if (drawRectangle || ht->childDistance != 0)
	hvGfxLine(hvg, x, midY, x+branchW, midY, MG_BLACK);
    return midY;
    }
else if (ht->left != NULL)
    return rDrawTreeInLabelArea(ht->left, hvg, yType, x, yFromNode, yh, th, drawRectangle);
else if (ht->right != NULL)
    return rDrawTreeInLabelArea(ht->right, hvg, yType, x, yFromNode, yh, th, drawRectangle);
// Leaf node -- return pixel height. Draw a line if yType is midpoint.
int y = yFromNode(ht->itemOrCluster, yh, yType);
if (yType == yrtMidPoint && x < labelEnd)
    {
    hvGfxLine(hvg, x, y, labelEnd, y, purple);
    addClusterMapItem(ht, x, y, labelEnd, y+1, th);
    }
return y;
}

struct yFromNodeHelper
/* Pre-computed mapping from cluster nodes' gtHapIx to pixel heights. */
    {
    unsigned int gtHapCount;
    unsigned int *gtHapIxToPxStart;
    unsigned int *gtHapIxToPxEnd;
    };

void initYFromNodeHelper(struct yFromNodeHelper *helper, int yOff, int height,
			 unsigned int gtHapCount, unsigned int *gtHapOrder,
			 int genotypeCount)
/* Build a mapping of genotype and haplotype to pixel y coords. */
{
helper->gtHapCount = gtHapCount;
helper->gtHapIxToPxStart = needMem(genotypeCount * 2 * sizeof(unsigned int));
helper->gtHapIxToPxEnd = needMem(genotypeCount * 2 * sizeof(unsigned int));
double pxPerHap = (double)height / gtHapCount;
int i;
for (i = 0;  i < gtHapCount;  i++)
    {
    int yStart = round(i * pxPerHap);
    int yEnd = round((i+1) * pxPerHap);
    if (yEnd == yStart)
	yEnd++;
    int gtHapIx = gtHapOrder[i];
    helper->gtHapIxToPxStart[gtHapIx] = yOff + yStart;
    helper->gtHapIxToPxEnd[gtHapIx] = yOff + yEnd;
    }
}

static int yFromHapNode(const struct slList *itemOrCluster, void *extraData,
			enum yRetType yType)
/* Extract the gtHapIx from hapCluster (hacTree node item), find out its relative order
 * and translate that to a pixel height. */
{
unsigned int gtHapIx = ((const struct hapCluster *)itemOrCluster)->gtHapIx;
struct yFromNodeHelper *helper = extraData;
int y;
if (yType == yrtStart)
    y = helper->gtHapIxToPxStart[gtHapIx];
else if (yType == yrtEnd)
    y = helper->gtHapIxToPxEnd[gtHapIx];
else
    y = (helper->gtHapIxToPxStart[gtHapIx] + helper->gtHapIxToPxEnd[gtHapIx]) / 2;
return y;
}

void initTitleHelper(struct titleHelper *th, char *track, int startIx, int centerIx, int endIx,
		     int nRecords, struct vcfFile *vcff)
/* Set up info including arrays of ref & alt alleles for cluster mouseover. */
{
th->track = track;
th->startIx = startIx;
th->centerIx = centerIx;
th->endIx = endIx;
th->nRecords = nRecords;
int len = endIx - startIx;
AllocArray(th->refs, len);
AllocArray(th->alts, len);
struct vcfRecord *rec;
int i;
for (rec = vcff->records, i = 0;  rec != NULL && i < endIx;  rec = rec->next, i++)
    {
    if (i < startIx)
	continue;
    char refAl[16];
    abbrevAndHandleRC(refAl, sizeof(refAl), rec->alleles[0]);
    th->refs[i-startIx] = vcfFilePooledStr(vcff, refAl);
    char altAl1[16];
    abbrevAndHandleRC(altAl1, sizeof(altAl1), rec->alleles[1]);
    tolowers(altAl1);
    th->alts[i-startIx] = vcfFilePooledStr(vcff, altAl1);
    }
}

static void drawTreeInLabelArea(struct hacTree *ht, struct hvGfx *hvg, int yOff, int clipHeight,
				struct yFromNodeHelper *yHelper, struct titleHelper *titleHelper,
				boolean drawRectangle)
/* Draw the haplotype clustering in the left label area (as much as fits there). */
{
// Figure out which hvg to use, save current clipping, and clip to left label coords:
struct hvGfx *hvgLL = (hvgSide != NULL) ? hvgSide : hvg;
int clipXBak, clipYBak, clipWidthBak, clipHeightBak;
hvGfxGetClip(hvgLL, &clipXBak, &clipYBak, &clipWidthBak, &clipHeightBak);
hvGfxUnclip(hvgLL);
hvGfxSetClip(hvgLL, leftLabelX, yOff, leftLabelWidth, clipHeight);
// Draw the tree:
int x = leftLabelX;
(void)rDrawTreeInLabelArea(ht, hvgLL, yrtMidPoint, x, yFromHapNode, yHelper, titleHelper,
			   drawRectangle);
// Restore the prior clipping:
hvGfxUnclip(hvgLL);
hvGfxSetClip(hvgLL, clipXBak, clipYBak, clipWidthBak, clipHeightBak);
}

static void ignoreEm(char *format, va_list args)
/* Ignore warnings from genotype parsing -- when there's one, there
 * are usually hundreds more just like it. */
{
}

static enum hapColorMode getColorMode(struct trackDb *tdb)
/* Get the hap-cluster coloring mode from cart & tdb. */
{
enum hapColorMode colorMode = altOnlyMode;
char *colorBy = cartOrTdbString(cart, tdb, VCF_HAP_COLORBY_VAR, VCF_DEFAULT_HAP_COLORBY);
if (sameString(colorBy, VCF_HAP_COLORBY_ALTONLY))
    colorMode = altOnlyMode;
else if (sameString(colorBy, VCF_HAP_COLORBY_REFALT))
    colorMode = refAltMode;
else if (sameString(colorBy, VCF_HAP_COLORBY_BASE))
    colorMode = baseMode;
else
    {
    char *geneTrack = cartOrTdbString(cart, tdb, "geneTrack", NULL);
    if (isNotEmpty(geneTrack) && sameString(colorBy, VCF_HAP_COLORBY_FUNCTION))
        colorMode = functionMode;
    }
return colorMode;
}

#define GENE_SIZE_FUDGE 2500000

static boolean vcfHapClusterDrawInit(struct track *tg, struct vcfFile *vcff, struct hvGfx *hvg,
                                     enum hapColorMode *retHapColorMode,
                                     struct seqWindow **retGSeqWin, struct txInfo **retTxiList)
/* Parse vcff's genotypes and get ready to draw haplotypes.  Return FALSE if nothing to draw. */
{
if (vcff->records == NULL)
    return FALSE;
undefYellow = hvGfxFindRgb(hvg, &undefinedYellowColor);
if (retHapColorMode)
    *retHapColorMode = getColorMode(tg->tdb);
pushWarnHandler(ignoreEm);
struct vcfRecord *rec;
for (rec = vcff->records;  rec != NULL;  rec = rec->next)
    {
    vcfParseGenotypesGtOnly(rec);
    }
popWarnHandler();
if (*retHapColorMode == functionMode)
    {
    if (!exprBedColorsMade)
        {
        makeRedGreenShades(hvg);
        // Make darkerShadesOfGreenOnWhite for local use
        static struct rgbColor white  = {255, 255, 255, 255};
        static struct rgbColor darkerGreen  = {0, 210, 0, 255};
        hvGfxMakeColorGradient(hvg, &white, &darkerGreen,  EXPR_DATA_SHADES,
                               darkerShadesOfGreenOnWhite);
        }
    int gStart = winStart - GENE_SIZE_FUDGE;
    if (gStart < 0)
        gStart = 0;
    int gEnd = winEnd + GENE_SIZE_FUDGE;
    int chromSize = hChromSize(database, chromName);
    if (gEnd > chromSize)
        gEnd = chromSize;
    *retGSeqWin = chromSeqWindowNew(database, chromName, gStart, gEnd);
    *retTxiList = txInfoLoad(*retGSeqWin, tg->tdb);
    }
else
    {
    *retGSeqWin = NULL;
    *retTxiList = NULL;
    }
return TRUE;
}

static void vcfHapClusterDraw(struct track *tg, int seqStart, int seqEnd,
			      struct hvGfx *hvg, int xOff, int yOff, int width,
			      MgFont *font, Color color, enum trackVisibility vis)
/* Split samples' chromosomes (haplotypes), cluster them by center-weighted
 * alpha similarity, and draw in the order determined by clustering. */
{
struct vcfFile *vcff = tg->extraUiData;
enum hapColorMode colorMode;
struct seqWindow *gSeqWin;
struct txInfo *txiList;
if (!vcfHapClusterDrawInit(tg, vcff, hvg, &colorMode, &gSeqWin, &txiList))
    return;
purple = hvGfxFindColorIx(hvg, 0x99, 0x00, 0xcc);
unsigned int gtHapCount = 0;
int nRecords = slCount(vcff->records);
int centerIx = getCenterVariantIx(tg, seqStart, seqEnd, vcff->records);
// Limit the number of variants that we compare, to keep from timing out:
// (really what we should limit is the number of distinct haplo's passed to hacTree!)
// In the meantime, this should at least be a cart var...
int maxVariantsPerSide = 50;
int startIx = max(0, centerIx - maxVariantsPerSide);
int endIx = min(nRecords, centerIx+1 + maxVariantsPerSide);
struct hacTree *ht = NULL;
unsigned int *gtHapOrder = clusterHaps(vcff, centerIx, startIx, endIx, &gtHapCount, &ht);
struct vcfRecord *centerRec = NULL;
struct vcfRecord *rec;
int ix;
// Unlike drawing order (last drawn is on top), the first mapBox gets priority,
// so map center variant before drawing & mapping other variants!
for (rec = vcff->records, ix=0;  rec != NULL;  rec = rec->next, ix++)
    {
    if (ix == centerIx)
	{
	centerRec = rec;
	mapBoxForCenterVariant(rec, hvg, tg, xOff, yOff, width);
	break;
	}
    }
for (rec = vcff->records, ix=0;  rec != NULL;  rec = rec->next, ix++)
    {
    boolean isClustered = (ix >= startIx && ix < endIx);
    if (ix != centerIx)
        {
        enum soTerm funcTerm = soUnknown;
        if (colorMode == functionMode)
            funcTerm = functionForRecord(rec, gSeqWin, txiList);
	drawOneRec(rec, gtHapOrder, gtHapCount, tg, hvg, xOff, yOff, width, isClustered, FALSE,
		   colorMode, funcTerm);
        }
    }
// Draw the center rec on top, outlined with black lines, to make sure it is very visible:
enum soTerm funcTerm = soUnknown;
if (colorMode == functionMode)
    funcTerm = functionForRecord(centerRec, gSeqWin, txiList);
drawOneRec(centerRec, gtHapOrder, gtHapCount, tg, hvg, xOff, yOff, width, TRUE, TRUE,
	   colorMode, funcTerm);
// Draw as much of the tree as can fit in the left label area:
int extraPixel = (colorMode == altOnlyMode || colorMode == functionMode) ? 1 : 0;
int hapHeight = tg->height- CLIP_PAD - 2*extraPixel;
struct yFromNodeHelper yHelper = {0, NULL, NULL};
initYFromNodeHelper(&yHelper, yOff+extraPixel, hapHeight, gtHapCount, gtHapOrder,
		    vcff->genotypeCount);
struct titleHelper titleHelper = { NULL, 0, 0, 0, 0, NULL, NULL };
initTitleHelper(&titleHelper, tg->track, startIx, centerIx, endIx, nRecords, vcff);
char *treeAngle = cartOrTdbString(cart, tg->tdb, VCF_HAP_TREEANGLE_VAR, VCF_DEFAULT_HAP_TREEANGLE);
boolean drawRectangle = sameString(treeAngle, VCF_HAP_TREEANGLE_RECTANGLE);
drawTreeInLabelArea(ht, hvg, yOff+extraPixel, hapHeight+CLIP_PAD, &yHelper, &titleHelper,
		    drawRectangle);
}

static void drawSampleLabels(struct vcfFile *vcff, struct hvGfx *hvg,
                             boolean isAllDiploid, int yStart, int height,
                             unsigned int *gtHapOrder, int gtHapCount, MgFont *font, Color color,
                             char *track)
/* Draw sample names as left labels. */
{
// Figure out which hvg to use, save current clipping, and clip to left label coords:
struct hvGfx *hvgLL = (hvgSide != NULL) ? hvgSide : hvg;
int clipXBak, clipYBak, clipWidthBak, clipHeightBak;
hvGfxGetClip(hvgLL, &clipXBak, &clipYBak, &clipWidthBak, &clipHeightBak);
hvGfxUnclip(hvgLL);
hvGfxSetClip(hvgLL, leftLabelX, yStart, leftLabelWidth, height);
if (isAllDiploid)
    {
    double pxPerGt = (double)height / vcff->genotypeCount;
    if (pxPerGt < tl.fontHeight + 1)
        warn("track %s: drawSampleLabels called with insufficient height", track);
    int gtIx;
    for (gtIx = 0;  gtIx < vcff->genotypeCount;  gtIx++)
        {
        int y = gtIx * pxPerGt;
        hvGfxTextRight(hvgLL, leftLabelX, y+yStart, leftLabelWidth-1, (int)pxPerGt,
                       color, font, vcff->genotypeIds[gtIx]);
        }
    }
else
    {
    double pxPerHt = (double)height / gtHapCount;
    if (pxPerHt < tl.fontHeight + 1)
        warn("track %s: drawSampleLabels called with insufficient height", track);
    int orderIx;
    for (orderIx = 0;  orderIx < gtHapCount;  orderIx++)
        {
        int gtHapIx = gtHapOrder[orderIx];
        int gtIx = (gtHapIx >> 1);
        int y = gtIx * pxPerHt;
        hvGfxTextRight(hvgLL, leftLabelX, y+yStart, leftLabelWidth-1, (int)pxPerHt,
                       color, font, vcff->genotypeIds[gtIx]);
        }
    }
// Restore the prior clipping:
hvGfxUnclip(hvgLL);
hvGfxSetClip(hvgLL, clipXBak, clipYBak, clipWidthBak, clipHeightBak);
}

static void drawSampleTitles(struct vcfFile *vcff, int yStart, int height,
                             unsigned int *gtHapOrder, int gtHapCount, char *track)
/* Draw mouseover labels / titles with the samples that are drawn at each pixel y offset */
{
double hapPerPx = (double)gtHapCount / height;
int labelEnd = leftLabelX + leftLabelWidth;
struct dyString *dy = dyStringNew(0);
int y;
for (y = 0;  y < height;  y++)
    {
    dyStringClear(dy);
    int gtHapStart = y * hapPerPx;
    int gtHapEnd = (y + 1) * hapPerPx;
    if (gtHapEnd == gtHapStart)
        gtHapEnd++;
    char *lastSample = NULL;
    int gtHapIx;
    for (gtHapIx = gtHapStart;  gtHapIx < gtHapEnd;  gtHapIx++)
        {
        int gtIx = (gtHapOrder[gtHapIx] >> 1);
        char *sample = vcff->genotypeIds[gtIx];
        if (!lastSample || differentString(sample, lastSample))
            {
            if (isNotEmpty(dy->string))
                dyStringAppend(dy, ", ");
            dyStringAppend(dy, sample);
            lastSample = sample;
            }
        }
    imgTrackAddMapItem(curImgTrack, TITLE_BUT_NO_LINK, dy->string,
		       leftLabelX, y+yStart, labelEnd, y+yStart+1, track, dy->string);
    }
}

static unsigned int *gtHapOrderFromGtOrder(struct vcfFile *vcff,
                                             boolean *retIsAllDiploid, int *retGtHapCount)
{
int ploidy = 2; // Assuming diploid genomes here, no XXY, tetraploid etc.
int gtCount = vcff->genotypeCount;
boolean isAllDiploid = TRUE;
unsigned int *gtHapOrder = needMem(gtCount * ploidy * sizeof(unsigned int));
int orderIx = 0;
int gtIx;
// Determine the number of chromosome rows; for chrX, can be mix of diploid and haploid.
for (gtIx=0;  gtIx < gtCount;  gtIx++)
    {
    int gtHapIx = (gtIx << 1);
    gtHapOrder[orderIx] = gtHapIx;
    orderIx++;
    struct vcfRecord *rec;
    for (rec = vcff->records;  rec != NULL;  rec = rec->next)
        {
        struct vcfGenotype *gt = &(rec->genotypes[gtIx]);
        if (!gt->isHaploid)
            {
            gtHapOrder[orderIx] = gtHapIx+1;
            orderIx++;
            break;
            }
        else
            isAllDiploid = FALSE;
        }
    }
if (retIsAllDiploid)
    *retIsAllDiploid = isAllDiploid;
if (retGtHapCount)
    *retGtHapCount = orderIx;
return gtHapOrder;
}

static void vcfGtHapFileOrderDraw(struct track *tg, int seqStart, int seqEnd,
                                  struct hvGfx *hvg, int xOff, int yOff, int width,
                                  MgFont *font, Color color, enum trackVisibility vis)
/* Draw rows in the same fashion as vcfHapClusterDraw, but instead of clustering, use the
 * order in which samples appear in the VCF file. */
{
struct vcfFile *vcff = tg->extraUiData;
enum hapColorMode colorMode;
struct seqWindow *gSeqWin;
struct txInfo *txiList;
if (!vcfHapClusterDrawInit(tg, vcff, hvg, &colorMode, &gSeqWin, &txiList))
    return;
boolean isAllDiploid;
int gtHapCount;
unsigned int *gtHapOrder = gtHapOrderFromGtOrder(vcff, &isAllDiploid, &gtHapCount);
struct vcfRecord *rec;
for (rec = vcff->records;  rec != NULL;  rec = rec->next)
    {
    enum soTerm funcTerm = soUnknown;
    if (colorMode == functionMode)
        funcTerm = functionForRecord(rec, gSeqWin, txiList);
    drawOneRec(rec, gtHapOrder, gtHapCount, tg, hvg, xOff, yOff, width, FALSE, FALSE,
               colorMode, funcTerm);
    }
// If height is sufficient, draw sample names as left labels; otherwise make mouseover titles
// with sample names for each pixel y offset.
int extraPixel = (colorMode == altOnlyMode || colorMode == functionMode) ? 1 : 0;
int hapHeight = tg->height - CLIP_PAD - 2*extraPixel;
int minHeightForLabels;
if (isAllDiploid)
    minHeightForLabels = vcff->genotypeCount * (tl.fontHeight + 1);
else
    minHeightForLabels = gtHapCount * (tl.fontHeight + 1);
if (hapHeight >= minHeightForLabels)
    drawSampleLabels(vcff, hvg, isAllDiploid, yOff+extraPixel, hapHeight, gtHapOrder, gtHapCount,
                     font, color, tg->track);
else
    drawSampleTitles(vcff, yOff+extraPixel, hapHeight, gtHapOrder, gtHapCount, tg->track);
}

static struct phyloTree *getTreeFromFile(struct trackDb *tdb)
/* Get the filename that follows trackDb setting 'hapClusterMethod treeFile ' and read it in
 * as a phyloTree. */
{
char *hapMethod = cloneString(trackDbSetting(tdb, VCF_HAP_METHOD_VAR));
if (! startsWithWord(VCF_HAP_METHOD_TREE_FILE, nextWord(&hapMethod)))
    errAbort("getTreeFromFile: expected trackDb setting " VCF_HAP_METHOD_VAR "to start with '"
             VCF_HAP_METHOD_TREE_FILE "' followed by a file name, but got '%s'", hapMethod);
char *fileName = hReplaceGbdb(nextWord(&hapMethod));
return phyloOpenTree(fileName);
}

struct hash *makeSampleToIx(struct vcfFile *vcff)
/* Alloc & return a hash of sample names to genotype indices in vcff. */
{
struct hash *sampleToIx = hashNew(0);
int gtIx;
for (gtIx = 0;  gtIx < vcff->genotypeCount;  gtIx++)
    hashAddInt(sampleToIx, vcff->genotypeIds[gtIx], gtIx);
return sampleToIx;
}

static int gtIxFromSample(struct hash *sampleToIx, char *sample, char *vcfFileName)
/* Look up sample's genotype index in sampleToIx, die complaining about vcfFileName if not found. */
{
int gtIx = hashIntValDefault(sampleToIx, sample, -1);
if (gtIx < 0)
    errAbort("gtIxFromSample: sample '%s' not found in VCF file %s", sample, vcfFileName);
return gtIx;
}

static void rSetGtHapOrderFromTree(struct phyloTree *node, struct vcfFile *vcff,
                                   struct hash *sampleToIx,
                                   unsigned int *gtHapOrder, int *pGtHapCount,
                                   unsigned int *leafOrderToHapOrderStart,
                                   unsigned int *leafOrderToHapOrderEnd, int *pLeafCount)
/* Set gtHapOrder to sample gt & hap indices in the order we encounter the samples in tree. */
{
if (node->numEdges > 0)
    {
    int ix;
    for (ix = 0;  ix < node->numEdges;  ix++)
        rSetGtHapOrderFromTree(node->edges[ix], vcff, sampleToIx, gtHapOrder, pGtHapCount,
                               leafOrderToHapOrderStart, leafOrderToHapOrderEnd, pLeafCount);
    }
else
    {
    int gtIx = gtIxFromSample(sampleToIx, node->ident->name, vcff->fileOrUrl);
    int gtHapIx = (gtIx << 1);
    gtHapOrder[*pGtHapCount] = gtHapIx;
    leafOrderToHapOrderStart[*pLeafCount] = leafOrderToHapOrderEnd[*pLeafCount] = *pGtHapCount;
    *pGtHapCount += 1;
    struct vcfRecord *rec;
    for (rec = vcff->records;  rec != NULL;  rec = rec->next)
        {
        struct vcfGenotype *gt = &(rec->genotypes[gtIx]);
        if (!gt->isHaploid)
            {
            gtHapOrder[*pGtHapCount] = gtHapIx+1;
            leafOrderToHapOrderEnd[*pLeafCount] = *pGtHapCount;
            *pGtHapCount += 1;
            break;
            }
        }
    *pLeafCount += 1;
    }
}

static unsigned int *gtHapOrderFromTree(struct vcfFile *vcff, struct phyloTree *tree,
                                          unsigned int **retLeafOrderToHapOrderStart,
                                          unsigned int **retLeafOrderToHapOrderEnd,
                                          int *retGtHapCount)
/* Alloc & return gtHapOrder, set to samples in the order we encounter them in tree.
 * Also build up maps of leaf order to low and high gtHapIx, for drawing the tree later. */
{
struct hash *sampleToIx = makeSampleToIx(vcff);
int ploidy = 2; // Assuming diploid genomes here, no XXY, tetraploid etc.
int gtCount = vcff->genotypeCount;
unsigned int *gtHapOrder = needMem(gtCount * ploidy * sizeof(unsigned int));
*retLeafOrderToHapOrderStart = needMem(gtCount * sizeof(unsigned int));
*retLeafOrderToHapOrderEnd = needMem(gtCount * sizeof(unsigned int));
*retGtHapCount = 0;
int leafCount = 0;
rSetGtHapOrderFromTree(tree, vcff, sampleToIx, gtHapOrder, retGtHapCount,
                       *retLeafOrderToHapOrderStart, *retLeafOrderToHapOrderEnd, &leafCount);
if (leafCount != vcff->genotypeCount)
    errAbort("gtHapOrderFromTree: tree has %d leaves, but VCF file %s has %d genotype columns",
             leafCount, vcff->fileOrUrl, vcff->genotypeCount);
return gtHapOrder;
}


// Relative coordinates for tree layout, to be transformed into pixel coords later.
struct nodeCoords
    {
    double rank;   // Centerpoint of children's rank in terms of hap order (0 = top haplotype)
    int depth;     // Maximum child depth + 1
    };

static int phyloTreeAddNodeCoords(struct phyloTree *node,
                                  unsigned int *leafOrderToHapOrderStart,
                                  unsigned int *leafOrderToHapOrderEnd,
                                  int leafIx)
/* Recursively annotate node and descendants with nodeCoords to prepare for drawing the tree. */
{
struct nodeCoords *coords;
AllocVar(coords);
node->priv = coords;
if (node->numEdges > 0)
    {
    double minRank = -1, maxRank = 0;
    int maxDepth = 0;
    int ix;
    for (ix = 0;  ix < node->numEdges;  ix++)
        {
        struct phyloTree *child = node->edges[ix];
        leafIx = phyloTreeAddNodeCoords(child, leafOrderToHapOrderStart, leafOrderToHapOrderEnd,
                                        leafIx);
        struct nodeCoords *childCoords = child->priv;
        if (minRank < 0 || childCoords->rank < minRank)
            minRank = childCoords->rank;
        if (childCoords->rank > maxRank)
            maxRank = childCoords->rank;
        if (childCoords->depth > maxDepth)
            maxDepth = childCoords->depth;
        }
    coords->rank = (minRank + maxRank) / 2.0;
    coords->depth = maxDepth + 1;
    }
else
    {
    // Leaf (sample)
    double rankStart = leafOrderToHapOrderStart[leafIx];
    double rankEnd = leafOrderToHapOrderStart[leafIx];
    coords->rank = (rankStart + rankEnd) / 2.0;
    leafIx++;
    coords->depth = 0;
    }
return leafIx;
}

static int colorCmp(const void *pa, const void *pb)
/* Compare two colors for sorting by numeric value. */
{
const Color ca = *(Color *)pa;
const Color cb = *(Color *)pb;
return ca - cb;
}

static Color colorFromChildColors(Color *childColors, int childCount, Color defaultCol)
/* If the majority of children have the same color, then return that color, otherwise defaultCol. */
{
Color childColCopy[childCount];
memcpy(childColCopy, childColors, sizeof childColCopy);
qsort(childColCopy, childCount, sizeof(*childColCopy), colorCmp);
Color maxRunColor = childCount > 0 ? childColors[0] : defaultCol;
int runLength = 1, maxRunLength = 1;
int ix;
for (ix = 1;  ix < childCount;  ix++)
    {
    if (childColors[ix] == childColors[ix-1])
        {
        runLength++;
        }
    else
        {
        if (runLength > maxRunLength)
            {
            maxRunLength = runLength;
            maxRunColor = childColors[ix-1];
            }
        runLength = 1;
        }
    }
if (runLength > maxRunLength)
    {
    maxRunLength = runLength;
    maxRunColor = childColors[ix-1];
    }
if (maxRunLength > (childCount>>1))
    return maxRunColor;
return defaultCol;
}

static Color rDrawPhyloTreeInLabelArea(struct phyloTree *node, struct hvGfx *hvg, int x,
                                       int yOff, double pxPerHap, MgFont *font,
                                       struct hash *highlightSamples, struct hash *sampleColors)
/* Recursively draw the tree in the left label area. */
{
const int branchW = 8;
int labelEnd = leftLabelX + leftLabelWidth;
Color color = MG_BLACK;
if (!sampleColors)
    {
    // Misuse the branch length value as RGB color (if it's the typical small number, will still
    // draw as approximately black):
    unsigned int rgb = node->ident->length;
    color = hvGfxFindColorIx(hvg, ((rgb>>16)&0xff), ((rgb>>8)&0xff), (rgb&0xff) );
    }
if (node->numEdges > 0)
    {
    // Draw each child and a horizontal line to child
    int minY = -1, maxY = 0;
    Color childColors[node->numEdges];
    int ix;
    for (ix = 0;  ix < node->numEdges;  ix++)
        {
        struct phyloTree *child = node->edges[ix];
        childColors[ix] = rDrawPhyloTreeInLabelArea(child, hvg, x+branchW, yOff, pxPerHap, font,
                                                    highlightSamples, sampleColors);
        struct nodeCoords *childCoords = child->priv;
        int childY = yOff + ((0.5 + childCoords->rank) * pxPerHap);
        hvGfxLine(hvg, x, childY, x+branchW, childY, childColors[ix]);
        if (minY < 0 || childY < minY)
            minY = childY;
        if (childY > maxY)
            maxY = childY;
        }
    // Draw a vertical line to connect the children
    if (sampleColors != NULL)
        color = colorFromChildColors(childColors, node->numEdges, color);
    hvGfxLine(hvg, x, minY, x, maxY, color);
    }
else
    {
    // Leaf node -- draw a horizontal line, and label if there is space to right of tree
    struct nodeCoords *coords = node->priv;
    int yLine = yOff + ((0.5 + coords->rank) * pxPerHap);
    int yBox = yLine - pxPerHap / 2;
    int yText = yLine - tl.fontHeight / 2;
    // Dunno why but the default font seems to draw with the baseline at y while the other fonts
    // draw with the mid line at y.
    if (sameOk(tl.textSize, "8"))
        yText += 2;
    if (highlightSamples && node->ident->name && hashLookup(highlightSamples, node->ident->name))
        hvGfxBox(hvg, leftLabelX, yBox, leftLabelWidth, pxPerHap,
                 hvGfxFindAlphaColorIx(hvg, 170, 255, 255, 128));
    if (sampleColors != NULL)
        color = (Color)hashIntValDefault(sampleColors, node->ident->name, MG_BLACK);
    hvGfxLine(hvg, x, yLine, x+branchW, yLine, color);
    int textX = x + branchW + 3;
    if (pxPerHap >= tl.fontHeight+1 && textX < labelEnd)
        hvGfxText(hvg, textX, yText, MG_BLACK, font, node->ident->name);
    }
return color;
}

static void drawPhyloTreeInLabelArea(struct phyloTree *tree, struct hvGfx *hvg, int yOff,
                                     int clipHeight, int gtHapCount,
                                     MgFont *font, struct hash *highlightSamples,
                                     struct hash *sampleColors)
{
struct hvGfx *hvgLL = (hvgSide != NULL) ? hvgSide : hvg;
int clipXBak, clipYBak, clipWidthBak, clipHeightBak;
hvGfxGetClip(hvgLL, &clipXBak, &clipYBak, &clipWidthBak, &clipHeightBak);
hvGfxUnclip(hvgLL);
hvGfxSetClip(hvgLL, leftLabelX, yOff, leftLabelWidth, clipHeight);
// Draw the tree:
int x = leftLabelX;
double pxPerHap = (double)clipHeight / gtHapCount;
rDrawPhyloTreeInLabelArea(tree, hvgLL, x, yOff, pxPerHap, font, highlightSamples, sampleColors);
// Restore the prior clipping:
hvGfxUnclip(hvgLL);
hvGfxSetClip(hvgLL, clipXBak, clipYBak, clipWidthBak, clipHeightBak);
}

static void rHighlightSampleRows(struct phyloTree *node, struct hvGfx *hvg, int yOff,
                                 double pxPerHap, struct hash *highlightSamples)
/* For each leaf node, if it is in highlightSamples then draw a highlight box for it. */
{
if (node->numEdges > 0)
    {
    int ix;
    for (ix = 0;  ix < node->numEdges;  ix++)
        {
        struct phyloTree *child = node->edges[ix];
        rHighlightSampleRows(child, hvg, yOff, pxPerHap, highlightSamples);
        }
    }
else
    {
    // leaf node; highlight if it's in highlightSamples
    if (node->ident->name && hashLookup(highlightSamples, node->ident->name))
        {
        struct nodeCoords *coords = node->priv;
        int y = yOff + (coords->rank * pxPerHap);
        hvGfxBox(hvg, insideX, y, insideWidth, pxPerHap, hvGfxFindAlphaColorIx(hvg, 170, 255, 255, 128));
        }
    }
}

static struct hash *getSampleColors(struct trackDb *tdb)
/* Return a hash of sample names to colors if specified in tdb, or NULL if none specified. */
{
struct hash *sampleColors = NULL;
char *setting = cartOrTdbString(cart, tdb, VCF_SAMPLE_COLOR_FILE, NULL);
if (isNotEmpty(setting))
    {
    // If the setting has not been set in the cart then we're getting the trackDb setting which
    // may specify a list of files and possibly labels like "Thing_one=file1 Thing_two=file2".
    // In that case, pick out the first file.
    if (strchr(setting, '=') || strchr(setting, ' '))
        {
        setting = nextWord(&setting);
        char *eq = (strchr(setting, '='));
        if (eq)
            setting = eq+1;
        }
    char *fileName = hReplaceGbdb(setting);
    struct lineFile *lf = netLineFileMayOpen(fileName);
    if (lf)
        {
        sampleColors = hashNew(0);
        char *line;
        while (lineFileNextReal(lf, &line))
            {
            char *words[3];
            int wordCount = chopTabs(line, words);
            lineFileExpectWords(lf, 2, wordCount);
            char *sample = words[0];
            char *colorStr = words[1];
            unsigned int rgb = bedParseColor(colorStr);
            Color color = bedColorToGfxColor(rgb);
            hashAddInt(sampleColors, sample, color);
            }
        lineFileClose(&lf);
        }
    else
        warn("Can't open sampleColorFile '%s'", fileName);
    }
return sampleColors;
}

static struct hash *getHighlightSamples(struct trackDb *tdb)
/* Return a hash of node IDs to highlight in the phylo tree display, or NULL if none specified. */
{
struct hash *highlightSamples = NULL;
char *setting = cartOrTdbString(cart, tdb, "highlightIds", NULL);
if (isNotEmpty(setting))
    {
    struct slName *list = slNameListFromComma(setting);
    highlightSamples = hashFromSlNameList(list);
    }
return highlightSamples;
}

static void vcfGtHapTreeFileDraw(struct track *tg, int seqStart, int seqEnd,
                                 struct hvGfx *hvg, int xOff, int yOff, int width,
                                 MgFont *font, Color color, enum trackVisibility vis)
/* Draw rows in the same fashion as vcfHapClusterDraw, but instead of clustering, use the
 * order in which samples appear in the VCF file. */
{
struct vcfFile *vcff = tg->extraUiData;
enum hapColorMode colorMode;
struct seqWindow *gSeqWin;
struct txInfo *txiList;
if (!vcfHapClusterDrawInit(tg, vcff, hvg, &colorMode, &gSeqWin, &txiList))
    return;
struct phyloTree *tree = getTreeFromFile(tg->tdb);
if (tree == NULL)
    {
    warn("No tree in file '%s'", trackDbSetting(tg->tdb, VCF_HAP_METHOD_VAR));
    vcfGtHapFileOrderDraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
    return;
    }
int gtHapCount;
unsigned int *leafOrderToHapOrderStart, *leafOrderToHapOrderEnd;
unsigned int *gtHapOrder = gtHapOrderFromTree(vcff, tree,
                                                &leafOrderToHapOrderStart, &leafOrderToHapOrderEnd,
                                                &gtHapCount);
// Figure out rank (vertical position) and depth (horizontal position) of every node in tree:
phyloTreeAddNodeCoords(tree, leafOrderToHapOrderStart, leafOrderToHapOrderEnd, 0);
int extraPixel = (colorMode == altOnlyMode || colorMode == functionMode) ? 1 : 0;
int hapHeight = tg->height - CLIP_PAD - 2*extraPixel;
struct hash *highlightSamples = getHighlightSamples(tg->tdb);
if (highlightSamples)
    {
    double pxPerHap = (double)hapHeight / gtHapCount;
    rHighlightSampleRows(tree, hvg, yOff+extraPixel, pxPerHap, highlightSamples);
    }
struct vcfRecord *rec;
for (rec = vcff->records;  rec != NULL;  rec = rec->next)
    {
    enum soTerm funcTerm = soUnknown;
    if (colorMode == functionMode)
        funcTerm = functionForRecord(rec, gSeqWin, txiList);
    drawOneRec(rec, gtHapOrder, gtHapCount, tg, hvg, xOff, yOff, width, FALSE, FALSE,
               colorMode, funcTerm);
    }
struct hash *sampleColors = getSampleColors(tg->tdb);
drawPhyloTreeInLabelArea(tree, hvg, yOff+extraPixel, hapHeight, gtHapCount, font, highlightSamples,
                         sampleColors);
drawSampleTitles(vcff, yOff+extraPixel, hapHeight, gtHapOrder, gtHapCount, tg->track);
}

static int vcfHapClusterTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return height of haplotype graph (2 * #samples * lineHeight);
 * 2 because we're assuming diploid genomes here, no XXY, tetraploid etc. */
{
int height;
if ((height = setupForWiggle(tg, vis)) != 0)
    return height;

const struct vcfFile *vcff = tg->extraUiData;
if (vcff->records == NULL)
    return 0;
int ploidy = sameString(chromName, "chrY") ? 1 : 2;
int simpleHeight = ploidy * vcff->genotypeCount * tg->lineHeight;
int defaultHeight = min(simpleHeight, VCF_DEFAULT_HAP_HEIGHT);
char *tdbHeight = trackDbSettingOrDefault(tg->tdb, VCF_HAP_HEIGHT_VAR, NULL);
if (isNotEmpty(tdbHeight))
    defaultHeight = atoi(tdbHeight);
int cartHeight = cartOrTdbInt(cart, tg->tdb, VCF_HAP_HEIGHT_VAR, defaultHeight);
if (tg->visibility == tvSquish)
    cartHeight /= 2;
enum hapColorMode colorMode = getColorMode(tg->tdb);
int extraPixel = (colorMode == altOnlyMode || colorMode == functionMode) ? 1 : 0;
int totalHeight = cartHeight + CLIP_PAD + 2*extraPixel;
tg->height = min(totalHeight, maximumTrackHeight(tg));
return tg->height;
}

static char *vcfHapClusterTrackName(struct track *tg, void *item)
/* If someone asks for itemName/mapItemName, just send name of track like wiggle. */
{
return tg->track;
}

static void vcfHapClusterOverloadMethods(struct track *tg, struct vcfFile *vcff)
/* If we confirm at load time that we can draw a haplotype graph, use
 * this to overwrite the methods for the rest of execution: */
{
tg->heightPer = (tg->visibility == tvSquish) ? (tl.fontHeight/4) : (tl.fontHeight / 2);
tg->lineHeight = tg->heightPer + 1;
char *hapMethod = cartOrTdbString(cart, tg->tdb, VCF_HAP_METHOD_VAR, VCF_DEFAULT_HAP_METHOD);
if (sameString(hapMethod, VCF_HAP_METHOD_FILE_ORDER))
    tg->drawItems = vcfGtHapFileOrderDraw;
else if (startsWithWord(VCF_HAP_METHOD_TREE_FILE, hapMethod))
    tg->drawItems = vcfGtHapTreeFileDraw;
else
    tg->drawItems = vcfHapClusterDraw;
tg->totalHeight = vcfHapClusterTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemName = vcfHapClusterTrackName;
tg->mapItemName = vcfHapClusterTrackName;
tg->itemStart = tgItemNoStart;
tg->itemEnd = tgItemNoEnd;
tg->mapsSelf = TRUE;
tg->extraUiData = vcff;
}

static void indelTweakMapItem(struct track *tg, struct hvGfx *hvg, void *item,
        char *itemName, char *mapItemName, int start, int end, int x, int y, int width, int height)
/* Pass the original vcf chromStart to pgSnpMapItem, so if we have trimmed an identical
 * first base from item's alleles and start, we will still pass the correct start to hgc. */
{
struct pgSnpVcfStartEnd *psvs = item;
pgSnpMapItem(tg, hvg, item, itemName, mapItemName, psvs->vcfStart, psvs->vcfEnd,
	     x, y, width, height);
}

static void vcfPhasedLoadItems(struct track *tg)
/* Load up one individuals phased genotypes in window */
{
char *fileOrUrl = trackDbSetting(tg->tdb, "bigDataUrl");
char *tbiFileOrUrl = trackDbSetting(tg->tdb, "bigDataIndex"); // unrelated to mysql

/* Figure out url or file name. */
if (!fileOrUrl)
    {
    struct sqlConnection *conn = hAllocConnTrack(database, tg->tdb);
    fileOrUrl = bbiNameFromSettingOrTableChrom(tg->tdb, conn, tg->table, chromName);
    hFreeConn(&conn);
    }

if (isEmpty(fileOrUrl))
    return;
int vcfMaxErr = -1;
struct vcfFile *vcff = NULL;

/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    vcff = vcfTabixFileAndIndexMayOpen(fileOrUrl, tbiFileOrUrl, chromName, winStart, winEnd, vcfMaxErr, -1);
    if (vcff != NULL)
        {
        filterRecords(vcff, tg);
        filterRefOnlyAlleles(vcff, tg->tdb); // remove items that don't differ from reference

        // TODO: in multi-region mode, different windows end up with different sets of variants where
        // all are phased or not, which throws off track heights in each window. Similar to hapCluster
        // mode, just switch to pgSnp view when in multi-region for now.
        if (slCount(windows) > 1 || tg->visibility == tvDense)
            pgSnpMethods(tg);
        tg->items = vcfFileToPgSnp(vcff, tg->tdb);
            // pgSnp bases coloring/display decision on count of items:
        tg->extraUiData = vcff;
        tg->customInt = slCount(tg->items);
        // Don't vcfFileFree here -- we are using its string pointers!
        }
    else
        {
        if (tbiFileOrUrl)
            errAbort("Unable to open VCF file/URL '%s' with tabix index '%s'", fileOrUrl, tbiFileOrUrl);
        else
            errAbort("Unable to open VCF file/URL '%s'", fileOrUrl);
        }
    }
errCatchEnd(errCatch);
if (errCatch->gotError || vcff == NULL)
    {
    if (isNotEmpty(errCatch->message->string))
        tg->networkErrMsg = cloneString(errCatch->message->string);
    tg->drawItems = bigDrawWarning;
    tg->totalHeight = bigWarnTotalHeight;
    }
errCatchFree(&errCatch);
}

static void vcfPhasedAddMapBox(struct hvGfx *hvg, struct vcfRecord *rec, struct pgSnpVcfStartEnd *psvs, char *text, int x, int y, int width, int height, struct track *track)
// Add mapbox for a tick in the vcfPhased track
{
mapBoxHgcOrHgGene(hvg, psvs->vcfStart, psvs->vcfEnd, x, y, width, height, track->track, rec->name, text, NULL, TRUE, NULL);
}

struct hapDistanceMatrixCell
{
/* The pair of alleles and how distant they are */
    struct hapDistanceMatrixCell *next;
    char *sampleId; // name of this sample
    char *otherId; // name of other sample
    int alleleIx; // index into vcfRecord->genotypes
    int otherAlleleIx; // index into vcfRecord->genotypes for other sample
    double dist;   // distance between this sample/allele and another sample/allele
};

struct hapDistanceMatrix
{
/* A row of hapDistanceMatrixCells, which store the distance computed by cwaDistance()
 * between two different alleles */
    struct hapDistanceMatrix *next; // next row in matrix
    char *sampleId;  // name of this sample
    int alleleIx; // allele id of this sample, 0 or 1
    struct hapDistanceMatrixCell *row;  // a row of cwaDistance scores
};

static int hapDistanceMatrixCellCmp(const void *item1, const void *item2)
{
const struct hapDistanceMatrixCell *cell1 = *(struct hapDistanceMatrixCell **)item1;
const struct hapDistanceMatrixCell *cell2 = *(struct hapDistanceMatrixCell **)item2;
return cell1->dist > cell2->dist;
}

static struct hapDistanceMatrixCell *findClosestChildAlleleToParent(char *parent, char *child,
                                struct hapDistanceMatrix *matrix)
/* Get all cells belonging to a certain parent onto a list and sort by distance */
{
struct hapDistanceMatrix *vec = NULL;
struct hapDistanceMatrixCell *cellList = NULL, *cell = NULL;
for (vec = matrix; vec != NULL; vec = vec->next)
    {
    for (cell = vec->row; cell != NULL; cell = cell->next)
        {
        if (sameString(cell->sampleId, parent))
            {
            struct hapDistanceMatrixCell *tmp = needMem(sizeof(struct hapDistanceMatrixCell));
            tmp->next = NULL;
            tmp->sampleId = cloneString(cell->sampleId);
            tmp->alleleIx = cell->alleleIx;
            tmp->otherId = cloneString(vec->sampleId);
            tmp->otherAlleleIx = vec->alleleIx;
            tmp->dist = cell->dist;
            slAddHead(&cellList, tmp);
            }
        }
    }
slSort(&cellList, hapDistanceMatrixCellCmp);
return cellList;
}

static unsigned int toggleInt(unsigned int s)
/* Add or subtract one */
{
return s & 1 ? s - 1 : s + 1;
}

static void fillOutHapOrder(unsigned int  *hapOrder, unsigned int hapCount, struct hapDistanceMatrixCell *c1, struct hapDistanceMatrixCell *c2, char **sampleDrawOrder)
/* Assign indices to hapOrder in the order we should draw the alleles. Allows for the second parent
 * cell to be NULL */
{
if (c1 == NULL)
    errAbort("Error: fillOutHapOrder passed NULL parent");
int numSamplesToDraw = hapCount / 2;
char *childSampleName = c1->otherId;
int childIx = stringArrayIx(childSampleName, sampleDrawOrder, numSamplesToDraw);
int i;
for (i = 0; i < numSamplesToDraw; i++)
    {
    char *thisSample = sampleDrawOrder[i];
    short hapIx = 2*i;
    if (i != childIx) // fill out the parent indices, which may be above or below the child
        {
        struct hapDistanceMatrixCell *thisCell = c2 != NULL && sameString(c2->sampleId,thisSample) ? c2 : c1;
        if (i < childIx)
            {
            hapOrder[hapIx] = toggleInt(thisCell->alleleIx);
            hapOrder[hapIx+1] = thisCell->alleleIx;
            }
        else // below the child
            {
            hapOrder[hapIx] = thisCell->alleleIx;
            hapOrder[hapIx+1] = toggleInt(thisCell->alleleIx);
            }
        }
    else // fill out the child indices
        {
        if (i == 0)
            {
            if (c2)
                hapOrder[hapIx] = c2->otherAlleleIx;
            else
                hapOrder[hapIx] = toggleInt(c1->otherAlleleIx);
            hapOrder[hapIx+1] = c1->otherAlleleIx;
            }
        else if (i == 1)
            {
            hapOrder[hapIx] = c1->otherAlleleIx;
            if (c2)
                hapOrder[hapIx+1] = c2->otherAlleleIx;
            else
                hapOrder[hapIx+1] = toggleInt(c1->otherAlleleIx);
            }
        else
            {
            hapOrder[hapIx] = c2->otherAlleleIx;
            hapOrder[hapIx+1] = c1->otherAlleleIx;
            }
        }
    }
}

static void maybeRollBackCell(struct hapDistanceMatrixCell *c1, struct hapDistanceMatrixCell *c2,
                              struct hapDistanceMatrixCell *c1Copy, struct hapDistanceMatrixCell *c2Copy)
/* At this point c1->otherAlleleIx != c2->otherAlleleIx, however, it may be the case that c2 has a
 * better scoring match to c1->otherAlleleIx, so try to find it and reset the values */
{
struct hapDistanceMatrixCell *tmp = c2;
while (tmp->otherAlleleIx != c1->otherAlleleIx)
    tmp = tmp->next;
if (tmp->dist < c1->dist)
    {
    *c2 = *tmp;
    *c1 = *c1Copy;
    }
}

static void setHapOrderFromMatrix(unsigned int *hapOrder, unsigned int hapCount,
                                struct hapDistanceMatrix *matrix, struct hapCluster **hapArray,
                                struct vcfFile *vcff, char *childName, char **sampleDrawOrder)
/* Given a matrix where each row is an allele of the child, and each column
 * is the distance from the child allele to a parent allele, fill out the order
 * in which the haplotypes will be drawn. The order are the indexes into rec->genotypes. */
{
char *parentNames[(hapCount/2) - 1];
struct hapDistanceMatrixCell *c1 = NULL;
struct hapDistanceMatrixCell *c2 = NULL;
int i, ix = 0;
if (hapCount > 2) // is there a trio or at least part of one?
    {
    for (i = 0; i < hapCount/2 ; i++)
        if (!sameString(sampleDrawOrder[i], childName))
            parentNames[ix++] = sampleDrawOrder[i];

    // For a parent/child combo, find the most likely set of transmitted
    // alleles. Depending on the number of variants in the window and the
    // haplotypes in question, both parents can tie as the person who
    // contributed a given allele
    c1 = findClosestChildAlleleToParent(parentNames[0], childName, matrix);
    c2 = findClosestChildAlleleToParent(parentNames[1], childName, matrix); // NULL if only one parent
    if (c1 && c2)
        {
        if (c1->otherAlleleIx == c2->otherAlleleIx)
            {
            struct hapDistanceMatrixCell *c1Copy = c1;
            struct hapDistanceMatrixCell *c2Copy = c2;
            if (c1->dist >= c2->dist)
                {
                while (c1->dist >= c2->dist && c1->otherAlleleIx == c2->otherAlleleIx)
                    c1 = c1->next;
                // at this point c1->otherAlleleIx != c2>otherAlleleIx, BUT c2 may have a better
                // scoring match to this allele and we shouldn't have advanced c1 to begin with!
                // This case is exercised at the following location (chr1:53896598-53897933) in
                // the following file: /gbdb/hg38/1000Genomes/trio/NA12878_1463_CEU/NA12878_1463_CEUTrio.chr1.vcf.gz
                // where the child's haplotypes are identical to both of the mother's haplotypes!
                maybeRollBackCell(c1, c2, c1Copy, c2Copy);
                }
            else
                {
                while (c1->dist < c2->dist && c1->otherAlleleIx == c2->otherAlleleIx)
                    c2 = c2->next;
                // similar to above case but swap c1 and c2
                maybeRollBackCell(c2, c1, c2Copy, c1Copy);
                }
            }
        }
    fillOutHapOrder(hapOrder, hapCount, c1, c2, sampleDrawOrder);
    }
else
    {
    hapOrder[0] = matrix->alleleIx;
    hapOrder[1] = matrix->next->alleleIx;
    }
}

static void assignHapArrayIx(int *ret, struct hapCluster **hapArray, struct vcfFile *vcff, char *sample, boolean doChild, int hapArrayLen)
{
int i;
struct vcfRecord *rec = vcff->records;
int ix = 0; // index into ret
for (i = 0; i < hapArrayLen; i++)
    {
    struct hapCluster *hc = hapArray[i];
    struct vcfGenotype *gt = &(rec->genotypes[hc->gtHapIx >> 1]);
    if (!doChild && !sameString(gt->id, sample))
        ret[ix++] = i;
    else if (doChild && sameString(gt->id, sample))
        ret[ix++] = i;
    }
}

static struct hapDistanceMatrix *fillOutDistanceMatrix(struct hapCluster **hapArray, struct vcfFile *vcff, char *sample, struct cwaExtraData *helper, int gtCount)
/* Allocates and fill out a struct hapDistanceMatrix, one row per child allele, and a
 * hapDistanceMatrixCell per parent allele */
{
int parGtCount = (gtCount - 1) * 2;
int i,j;
struct vcfRecord *rec = vcff->records;
struct hapDistanceMatrix *matrix = NULL;
int childHapArrayIndices[2];
int parentHapArrayIndices[parGtCount];
assignHapArrayIx(childHapArrayIndices, hapArray, vcff, sample, TRUE, gtCount * 2);
assignHapArrayIx(parentHapArrayIndices, hapArray, vcff, sample, FALSE, gtCount * 2);
for (i = 0; i < 2; i++)
    {
    struct hapDistanceMatrix *row = needMem(sizeof(struct hapDistanceMatrix));
    struct hapCluster *hcChild = hapArray[childHapArrayIndices[i]];
    struct vcfGenotype *gt = &(rec->genotypes[hcChild->gtHapIx >> 1]);
    row->row = NULL;
    row->sampleId = cloneString(gt->id);
    row->alleleIx = hcChild->gtHapIx;
    for (j = 0; j < parGtCount; j++)
        {
        struct hapDistanceMatrixCell *cell = needMem(sizeof(struct hapDistanceMatrixCell));
        struct hapCluster *hcParent = hapArray[parentHapArrayIndices[j]];
        struct vcfGenotype *parGt = &(rec->genotypes[hcParent->gtHapIx >> 1]);
        cell->sampleId = cloneString(parGt->id);
        cell->alleleIx = hcParent->gtHapIx;
        cell->dist = cwaDistance((struct slList *)hcChild, (struct slList *)hcParent, helper);
        slAddHead(&(row->row), cell);
        }
    slAddHead(&matrix, row);
    }
return matrix;
}

unsigned int *computeHapDist(struct vcfFile *vcff, int centerIx, int startIx, int endIx, char *sample, int gtCount, char **sampleDrawOrder)
// similar to clusterHaps(), but instead of making a hacTree at the end, call cwaDistance
// on each of the pairs in hapArray to make a distance matrix, then compute a hapOrder from that
{
double alpha = 0.5;
struct lm *lm = lmInit(0);
struct cwaExtraData helper = { centerIx-startIx, endIx-startIx, alpha, lm };
struct hapCluster **hapArray = lmAlloc(lm, sizeof(struct hapCluster *) * gtCount * 2);
int i;
for (i=0;  i < 2 * gtCount;  i++)
    {
    hapArray[i] = lmHapCluster(&helper);
    if (i > 0)
        hapArray[i-1]->next = hapArray[i];
    }
struct vcfRecord *rec;
int varIx;
boolean haveHaploid;
for (varIx = 0, rec = vcff->records;  rec != NULL && varIx < endIx;  varIx++, rec = rec->next)
    {
    if (varIx < startIx)
        continue;
    int countIx = varIx - startIx;
    int gtIx;
    for (gtIx=0;  gtIx < gtCount;  gtIx++)
        {
        // VCF may contain genotypes other than the few we are interested, so sampleDrawOrder[gtIx]
        // may not be the right index in rec->genotypes we want, look it up here
        const struct vcfGenotype *gt = vcfRecordFindGenotype(rec, sampleDrawOrder[gtIx]);
        int ix = stringArrayIx(sampleDrawOrder[gtIx], vcff->genotypeIds, vcff->genotypeCount);
        struct hapCluster *c1 = hapArray[gtIx];
        struct hapCluster *c2 = hapArray[gtCount + gtIx]; // hardwired ploidy=2
        c1->gtHapIx = ix << 1;
        c1->leafCount = 1;
        if (gt->isPhased || gt->isHaploid || (gt->hapIxA == gt->hapIxB))
            {
            // first haplotype's counts:
            if (gt->hapIxA < 0)
                c1->unkCounts[countIx] = 1;
            else if (gt->hapIxA == 0)
                c1->refCounts[countIx] = 1;
            if (gt->isHaploid)
                haveHaploid = TRUE;
            else
                {
                // got second haplotype, fill in its counts:
                c2->gtHapIx = (ix << 1) | 1;
                c2->leafCount = 1;
                if (gt->hapIxB < 0)
                    c2->unkCounts[countIx] = 1;
                else if (gt->hapIxB == 0)
                    c2->refCounts[countIx] = 1;
                }
            }
        else
            {
            // Missing data or unphased heterozygote, don't use haplotype info for clustering
            c2->gtHapIx = (ix << 1) | 1;
            c2->leafCount = 1;
            c1->unkCounts[countIx] = c2->unkCounts[countIx] = 1;
            }
        }
    if (haveHaploid)
        {
        // Some array items will have an empty cluster for missing hap2 --
        // trim those from the linked list.
        struct hapCluster *c = hapArray[0];
        while (c != NULL && c->next != NULL)
            {
            if (c->next->leafCount == 0)
                c->next = c->next->next;
            else
                c = c->next;
            }
        }
    }

// now fill out a distance matrix based on the cwaDistance between all the pairs in hapArray
struct hapDistanceMatrix *hapDistMatrix = fillOutDistanceMatrix(hapArray, vcff, sample, &helper, gtCount);
unsigned int hapCount = 2 * gtCount;
unsigned int *hapOrder = needMem(sizeof(unsigned int) * hapCount);
setHapOrderFromMatrix(hapOrder, hapCount, hapDistMatrix, hapArray, vcff, sample, sampleDrawOrder);
return hapOrder;
}

static void setTransmitOrder(unsigned int *gtHapOrder, char **transmitOrder, int gtHapCount, struct vcfRecord *rec, char **sampleOrder, char *childSample)
/* Fill out an array of "transmitted", "untransmitted" strings for mouseover and color lookup */
{
int i;
int childSampleIx = stringArrayIx(childSample, sampleOrder, gtHapCount / 2);
for (i = 0; i < gtHapCount; i++)
    {
    int gtIx = gtHapOrder[i];
    struct vcfGenotype *gt = &(rec->genotypes[gtIx >> 1]);
    int alleleSampleIx = stringArrayIx(gt->id, sampleOrder, gtHapCount / 2);
    if (alleleSampleIx != childSampleIx)
        if (alleleSampleIx > childSampleIx)
            {
            transmitOrder[i] = i % 2 == 0 ? "transmitted" : "untransmitted";
            }
        else
            {
            transmitOrder[i] = i % 2 == 0 ? "untransmitted" : "transmitted";
            }
    else
        transmitOrder[i] = "child";
    }
}

enum phasedColorMode
    {
    noColorMode,
    mendelDiffMode,
    deNovoOnlyMode,
    phasedFunctionMode
    };

static enum phasedColorMode getPhasedColorMode(struct trackDb *tdb)
/* Get the coloring mode for phased trio tracks */
{
enum phasedColorMode colorMode = noColorMode;
char *colorBy = cartOrTdbString(cart, tdb, VCF_PHASED_COLORBY_VAR, VCF_PHASED_COLORBY_VAR);
if (sameString(colorBy, VCF_PHASED_COLORBY_MENDEL_DIFF))
    colorMode = mendelDiffMode;
else if (sameString(colorBy, VCF_PHASED_COLORBY_DE_NOVO))
    colorMode = deNovoOnlyMode;
else
    {
    char *geneTrack = cartOrTdbString(cart, tdb, "geneTrack", NULL);
    if (isNotEmpty(geneTrack) && sameString(colorBy, VCF_PHASED_COLORBY_FUNCTION))
        colorMode = phasedFunctionMode;
    }
return colorMode;
}

static int getTickColor(struct track *track, struct vcfRecord *rec, int alleleIx, int nameIx, int gtHapCount, unsigned int *gtHapOrder, char *childSampleName, char *sampleOrder[], enum phasedColorMode colorMode, enum soTerm funcTerm)
/* Return the color we should use for this variant */
{
// if there are no parents to draw then no concept of transmitted vs unstransmitted
Color col = MG_BLACK;
if (colorMode == noColorMode)
    col = MG_BLACK;
else if (colorMode == phasedFunctionMode)
    {
    col = colorFromSoTerm(funcTerm);
    }
else
    {
    if (gtHapCount <= 2)
        col = MG_BLACK;
    else
        {
        char *sampleName = sampleOrder[nameIx];
        if (!sameString(sampleName, childSampleName)) // parents only have shading in funcMode
            col = MG_BLACK;
        else // maybe draw child ticks red
            {
            struct vcfGenotype *childGt = &(rec->genotypes[gtHapOrder[alleleIx] >> 1]);
            int childHapIx = gtHapOrder[alleleIx] & 1 ? childGt->hapIxB : childGt->hapIxA;
            int parIx = alleleIx;
            int numSamples = gtHapCount / 2;
            boolean isEven = (alleleIx % 2) == 0;
            const int adjacentLineSet = 1;
            // how many haplotype "lines" away is the second parent if we are drawing in either
            // parent1,parent2,child or child,parent1,parent2 order:
            const int otherLineSet = 4;
            if (nameIx == 0)
                {
                if (!isEven)
                    parIx = alleleIx + adjacentLineSet;
                else if (numSamples > 2)
                        parIx = alleleIx + otherLineSet;
                }
            else if (nameIx == 1)
                {
                if (isEven)
                    parIx = alleleIx - adjacentLineSet;
                else if (numSamples > 2) // don't compare the allele the child to a missing parent
                    parIx = alleleIx + adjacentLineSet;
                }
            else
                {
                if (isEven)
                    parIx = alleleIx - adjacentLineSet;
                else
                    parIx = alleleIx - otherLineSet;
                }
            struct vcfGenotype *parentGt = &(rec->genotypes[gtHapOrder[parIx] >> 1]);
            if (parentGt->isPhased || (parentGt->hapIxA == 1 && parentGt->hapIxB == 1))
                {
                int transmittedIx = gtHapOrder[parIx] & 1 ? parentGt->hapIxB : parentGt->hapIxA;
                int untransmittedIx = gtHapOrder[parIx] & 1 ? parentGt->hapIxA : parentGt->hapIxB;
                col = MG_BLACK;
                if (colorMode == mendelDiffMode)
                    {
                    if (rec->alleles[childHapIx] != rec->alleles[transmittedIx] &&
                        rec->alleles[childHapIx] == rec->alleles[untransmittedIx])
                        col = MG_RED;
                    }
                else
                    {
                    if (rec->alleles[childHapIx] != rec->alleles[transmittedIx] &&
                        rec->alleles[childHapIx] != rec->alleles[untransmittedIx])
                        col = MG_RED;
                    }
                }
            }
        }
    }
return col;
}

void vcfPhasedDrawOneRecord(struct track *track, struct hvGfx *hvg, struct vcfRecord *rec,
                            void *item, unsigned int *gtHapOrder, int gtHapCount, int xOff,
                            int yOffsets[], char *sampleOrder[], char *childSample, double scale,
                            char *transmitOrder[], enum phasedColorMode colorMode,
                            enum soTerm funcTerm)
// Draw a record's haplotypes on the appropriate lines
{
int i;
int x1 = round((double)(rec->chromStart-winStart)*scale) + xOff;
int x2 = round((double)(rec->chromEnd-winStart)*scale) + xOff;
struct pgSnpVcfStartEnd *psvs = (struct pgSnpVcfStartEnd *)item;
int w = x2-x1;
if (w < 1)
    w = 1;
int tickHeight = track->itemHeight(track, track->items);
for (i = 0; i < gtHapCount ; i++)
    {
    struct vcfGenotype *gt = &(rec->genotypes[gtHapOrder[i] >> 1]);
    int nameIx = stringArrayIx(gt->id, sampleOrder, track->customInt);
    struct dyString *mouseover = dyStringNew(0);
    int tickColor = getTickColor(track, rec, i, nameIx, gtHapCount, gtHapOrder, childSample, sampleOrder, colorMode, funcTerm);
    if (gt->isPhased || (gt->hapIxA == 1 && gt->hapIxB == 1)) // if phased or homozygous alt
        {
        int alIx = gtHapOrder[i] & 1 ? gt->hapIxB : gt->hapIxA;
        dyStringPrintf(mouseover, "%s ", gt->id);
        if (alIx != 0) // non-reference allele
            {
            if (sameString(childSample, gt->id)) // we're drawing child ticks
                {
                dyStringPrintf(mouseover, "allele: %s", rec->alleles[alIx]);
                }
            else
                {
                if (gt->isPhased)
                    dyStringPrintf(mouseover, "likely %s allele: %s", transmitOrder[i], rec->alleles[alIx]);
                else
                    {
                    int otherIx = (alIx == gt->hapIxB ? gt->hapIxA : gt->hapIxB);
                    dyStringPrintf(mouseover, "unphased alleles: %s/%s", rec->alleles[alIx], rec->alleles[otherIx]);
                    }
                }
            int y = yOffsets[i] - (tickHeight/2);
            hvGfxBox(hvg, x1, y, w, tickHeight, tickColor);
            vcfPhasedAddMapBox(hvg, rec, psvs, mouseover->string, x1, y, w, tickHeight, track);
            }
        }
    else if (gt->hapIxA != 0 || gt->hapIxB != 0)// draw the tick between the two haplotype lines
        {
        int yMid = ((yOffsets[2*nameIx] + yOffsets[(2*nameIx)+1]) / 2); // midpoint of two haplotype lines
        hvGfxBox(hvg, x1, yMid - (tickHeight / 2), w, tickHeight, tickColor);
        dyStringPrintf(mouseover, "%s unphased genotype: %s/%s", gt->id, rec->alleles[0], rec->alleles[1]);
        vcfPhasedAddMapBox(hvg, rec, psvs, mouseover->string, x1, yMid, w, tickHeight, track);
        }
    }
}

static void vcfPhasedAddLabel(struct track *track, struct hvGfx *hvg, char *label, int trackY, int labelY, MgFont *font, Color color)
// Add a VCF Sample name to the left label of the haplotype representation
{
int clipXBak, clipYBak, clipWidthBak, clipHeightBak;
struct hvGfx *hvgLL = (hvgSide != NULL) ? hvgSide : hvg;
hvGfxGetClip(hvgLL, &clipXBak, &clipYBak, &clipWidthBak, &clipHeightBak);
hvGfxUnclip(hvgLL);
hvGfxSetClip(hvgLL, leftLabelX, trackY, leftLabelWidth, track->height);

hvGfxTextRight(hvgLL, leftLabelX, labelY, leftLabelWidth, track->lineHeight, color,
    font, label);

// Restore the prior clipping:
hvGfxUnclip(hvgLL);
hvGfxSetClip(hvgLL, clipXBak, clipYBak, clipWidthBak, clipHeightBak);
}

static void vcfPhasedSetupHaplotypesLines(struct track *track, struct hvGfx *hvg, int xOff,
                            int yOff, int width, int *retYOffsets, struct slPair *sampleNames,
                            char *childSample, MgFont *font)
/* Setup the background for drawing the ticks, the two haplotype lines for each sample, and the
 * transparent gray box to help distinguish between consecutive samples */
{
int sampleHeight = round(track->height / track->customInt);
double yHap1 = track->lineHeight; // relative offset of first haplotype line
double yHap2 = sampleHeight - track->lineHeight; // relative offset of second line
struct slPair *name;
int i, y1, y2;
struct rgbColor yellow = lightRainbowAtPos(0.2);
int transYellow = hvGfxFindAlphaColorIx(hvg, yellow.r, yellow.g, yellow.b, 100);

boolean useDefaultLabel = FALSE;
if (cartVarExistsAnyLevel(cart, track->tdb, FALSE, VCF_PHASED_DEFAULT_LABEL_VAR))
    useDefaultLabel = cartUsualBooleanClosestToHome(cart, track->tdb, FALSE, VCF_PHASED_DEFAULT_LABEL_VAR, FALSE);

boolean useAliasLabel = trackDbSettingOn(track->tdb, VCF_PHASED_TDB_USE_ALT_NAMES);
if (cartVarExistsAnyLevel(cart, track->tdb, FALSE, VCF_PHASED_ALIAS_LABEL_VAR))
    useAliasLabel = cartUsualBooleanClosestToHome(cart, track->tdb, FALSE, VCF_PHASED_ALIAS_LABEL_VAR, FALSE);

for (name = sampleNames, i = 0; name != NULL; name = name->next, i++)
    {
    y1 = yOff + yHap1 + (i * sampleHeight);
    y2 = yOff + yHap2 + (i * sampleHeight);
    retYOffsets[2*i] = y1;
    retYOffsets[(2*i) + 1] = y2;
    // make the background of every other lane light yellow
    if (sameString(childSample, name->name))
        {
        hvGfxBox(hvg, xOff, y1-(track->lineHeight), width, (y2 + track->lineHeight) - (y1-track->lineHeight), transYellow);
        }
    hvGfxLine(hvg, xOff, y1, xOff+width, y1, MG_BLACK);
    hvGfxLine(hvg, xOff, y2, xOff+width, y2, MG_BLACK);
    struct dyString *label = dyStringNew(0);
    boolean hasAlias = isNotEmpty((char *)name->val);
    dyStringPrintf(label, "%s%s%s",
        useDefaultLabel ? name->name : "",
        useDefaultLabel && useAliasLabel && hasAlias ? "/" : "",
        useAliasLabel && hasAlias ? (char *)name->val : "");
    vcfPhasedAddLabel(track, hvg, label->string, yOff, round(((y1 + y2) / 2) - (track->lineHeight / 2)), font, MG_BLACK);
    }
}

static void vcfPhasedDrawItems(struct track *track, int seqStart, int seqEnd,
			      struct hvGfx *hvg, int xOff, int yOff, int width,
			      MgFont *font, Color color, enum trackVisibility vis)
/* Split samples' chromosomes (haplotypes), cluster them by parents, and
 * draw them all along a line representing each chromosome*/
{
struct vcfFile *vcff = track->extraUiData;
if (vcff->records == NULL)
    return;

struct txInfo *txiList = NULL;
struct seqWindow *gSeqWin = chromSeqWindowNew(database, chromName, seqStart, seqEnd);
enum phasedColorMode colorMode = getPhasedColorMode(track->tdb);
if (colorMode == phasedFunctionMode)
    txiList = txInfoLoad(gSeqWin, track->tdb);
const double scale = scaleForPixels(width);
boolean hideOtherSamples = cartUsualBooleanClosestToHome(cart, track->tdb, FALSE, VCF_PHASED_HIDE_OTHER_VAR, FALSE);
struct slPair *pair, *sampleNames = vcfPhasedGetSampleOrder(cart, track->tdb, FALSE, hideOtherSamples);
int gtCount = slCount(sampleNames);
int gtHapCount = gtCount * 2;
int yOffsets[gtHapCount]; // y offsets of each haplotype line
char *sampleOrder[gtCount]; // order of sampleName lines
int i;
for (pair = sampleNames, i = 0; pair != NULL && i < gtCount; pair = pair->next, i++)
    sampleOrder[i] = pair->name;

char *childSample = cloneString(trackDbSetting(track->tdb, VCF_PHASED_CHILD_SAMPLE_SETTING));
char *pt = strchr(childSample, '|');
if (pt != NULL)
    *pt = '\0';

// set up the "haplotype" lines and the transparent yellow box to delineate samples
vcfPhasedSetupHaplotypesLines(track, hvg, xOff, yOff, width, yOffsets, sampleNames, childSample, font);

// maybe sort the variants by haplotype then draw ticks
unsigned int *hapOrder = needMem(sizeof(short) * gtHapCount);
int nRecords = slCount(vcff->records);
int centerIx = getCenterVariantIx(track, seqStart, seqEnd, vcff->records);
int startIx = 0;
int endIx = nRecords;
hapOrder = computeHapDist(vcff, centerIx, startIx, endIx, childSample, gtCount, sampleOrder);
struct vcfRecord *rec = NULL;
struct slList *item = NULL;

// array of "trans", "untrans", etc for mouseovers
char *transmitOrder[gtHapCount];
setTransmitOrder(hapOrder, transmitOrder, gtHapCount, vcff->records, sampleOrder, childSample);

for (rec = vcff->records, item = track->items; rec != NULL && item != NULL; rec = rec->next, item = item->next)
    {
    enum soTerm funcTerm = soUnknown;
    if (colorMode == phasedFunctionMode)
        funcTerm = functionForRecord(rec, gSeqWin, txiList);
    vcfPhasedDrawOneRecord(track, hvg, rec, item, hapOrder, gtHapCount, xOff, yOffsets, sampleOrder, childSample, scale, transmitOrder, colorMode, funcTerm);
    }
}

static int vcfPhasedItemHeight(struct track *tg, void *item)
{
return tg->heightPer;
}

int vcfPhasedTrackHeight(struct track *tg, enum trackVisibility vis)
{
const struct vcfFile *vcff = tg->extraUiData;
// when doing composite track height, vcfPhasedLoadItems won't have been called yet!
if (vis == tvDense)
    return pgSnpHeight(tg, vis);
if (!vcff || vcff->records == NULL)
    return 0;
boolean hideOtherSamples = cartUsualBooleanClosestToHome(cart, tg->tdb, FALSE, VCF_PHASED_HIDE_OTHER_VAR, FALSE);
int totalSamples = slCount(vcfPhasedGetSampleOrder(cart, tg->tdb, FALSE, hideOtherSamples));
tg->lineHeight = tl.fontHeight + 1;
tg->heightPer = tl.fontHeight;
// if all variants in view are phased, then 3 lines per sample,
// else 4 lines. The extra 2 is for clear separation
int heightPerSample;
if (vcff->allPhased)
    heightPerSample = (3 * tg->lineHeight) + 2;
else
    heightPerSample = (4 * tg->lineHeight);
tg->height = totalSamples * heightPerSample;
tg->itemHeight = vcfPhasedItemHeight;
// custom int is reserved for doing pgSnp coloring but as far as I can tell is
// never actually used?
tg->customInt = totalSamples;
return tg->height;
}

void vcfPhasedMethods(struct track *track)
/* Load items from a VCF of one individuals phased genotypes */
{
knetUdcInstall();
pgSnpMethods(track);
track->drawItems = vcfPhasedDrawItems;
// Disinherit next/prev flag and methods since we don't support next/prev:
track->nextExonButtonable = FALSE;
track->nextPrevExon = NULL;
track->nextPrevItem = NULL;
track->loadItems = vcfPhasedLoadItems;
track->totalHeight = vcfPhasedTrackHeight;
track->itemName = vcfHapClusterTrackName;
track->mapsSelf = TRUE;
}

static unsigned vcfMaxItems()
/* Get the maximum number of items to grab from a vcf file.  Defaults to ten thousand. */
{
static boolean set = FALSE;
static unsigned maxItems = 0;

if (!set)
    {
    char *maxItemsStr = cfgOptionDefault("vcfMaxItems", "10000");

    maxItems = sqlUnsigned(maxItemsStr);
    }

return maxItems;
}

static void vcfTabixLoadItems(struct track *tg)
/* Load items in window from VCF file using its tabix index file. */
{
char *fileOrUrl = trackDbSetting(tg->tdb, "bigDataUrl");
char *tbiFileOrUrl = trackDbSetting(tg->tdb, "bigDataIndex"); // unrelated to mysql

if (!fileOrUrl)
    {
    struct sqlConnection *conn = hAllocConnTrack(database, tg->tdb);
    fileOrUrl = bbiNameFromSettingOrTableChrom(tg->tdb, conn, tg->table, chromName);
    hFreeConn(&conn);
    }

if (isEmpty(fileOrUrl))
    return;
fileOrUrl = hReplaceGbdb(fileOrUrl);
int vcfMaxErr = -1;
struct vcfFile *vcff = NULL;
boolean hapClustEnabled = cartOrTdbBoolean(cart, tg->tdb, VCF_HAP_ENABLED_VAR, TRUE);
if (slCount(windows)>1)
    hapClustEnabled = FALSE;  // haplotype sorting display not currently available with multiple windows.
/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    vcff = vcfTabixFileAndIndexMayOpenExt(fileOrUrl, tbiFileOrUrl, chromName, winStart, winEnd, vcfMaxErr, vcfMaxItems(), 
        "Too many items in region.Zoom in to view track.");
    if (vcff != NULL)
	{
	filterRecords(vcff, tg);
        int vis = tdbVisLimitedByAncestors(cart,tg->tdb,TRUE,TRUE);
        boolean doWiggle = checkIfWiggling(cart, tg);
	if (!doWiggle && hapClustEnabled && vcff->genotypeCount > 1 &&
	    (vis == tvPack || vis == tvSquish))
	    vcfHapClusterOverloadMethods(tg, vcff);
	else
	    {
	    tg->items = vcfFileToPgSnp(vcff, tg->tdb);
	    // pgSnp bases coloring/display decision on count of items:
	    tg->customInt = slCount(tg->items);
	    }
	// Don't vcfFileFree here -- we are using its string pointers!
	}
    else
        {
        if (tbiFileOrUrl)
            errAbort("Unable to open VCF file/URL '%s' with tabix index '%s'", fileOrUrl, tbiFileOrUrl);
        else
            errAbort("Unable to open VCF file/URL '%s'", fileOrUrl);
        }
    }
errCatchEnd(errCatch);
if (errCatch->gotError || vcff == NULL)
    {
    if (isNotEmpty(errCatch->message->string))
	tg->networkErrMsg = cloneString(errCatch->message->string);
    tg->drawItems = bigDrawWarning;
    tg->totalHeight = bigWarnTotalHeight;
    }
errCatchFree(&errCatch);
}

void vcfTabixMethods(struct track *track)
/* Methods for VCF + tabix files. */
{
knetUdcInstall();
pgSnpMethods(track);
track->mapItem = indelTweakMapItem;
// Disinherit next/prev flag and methods since we don't support next/prev:
track->nextExonButtonable = FALSE;
track->nextPrevExon = NULL;
track->nextPrevItem = NULL;
track->loadItems = vcfTabixLoadItems;
track->canPack = TRUE;
}


static void vcfLoadItems(struct track *tg)
/* Load items in window from VCF file. */
{
int vcfMaxErr = -1;
struct vcfFile *vcff = NULL;
boolean hapClustEnabled = cartOrTdbBoolean(cart, tg->tdb, VCF_HAP_ENABLED_VAR, TRUE);
if (slCount(windows)>1)
    hapClustEnabled = FALSE;  // haplotype sorting display not currently available with multiple windows.
char *table = tg->table;
struct customTrack *ct = tg->customPt;
struct sqlConnection *conn;
if (ct == NULL)
    conn = hAllocConnTrack(database, tg->tdb);
else
    {
    conn = hAllocConn(CUSTOM_TRASH);
    table = ct->dbTableName;
    }
char *vcfFile = bbiNameFromSettingOrTable(tg->tdb, conn, table);
hFreeConn(&conn);
/* protect against parse error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    vcff = vcfFileMayOpen(vcfFile, chromName, winStart, winEnd, vcfMaxErr, -1, TRUE);
    if (vcff != NULL)
	{
	filterRecords(vcff, tg);
        int vis = tdbVisLimitedByAncestors(cart,tg->tdb,TRUE,TRUE);
	if (hapClustEnabled && vcff->genotypeCount > 1 && vcff->genotypeCount < 3000 &&
	    (vis == tvPack || vis == tvSquish))
	    vcfHapClusterOverloadMethods(tg, vcff);
	else
	    {
	    tg->items = vcfFileToPgSnp(vcff, tg->tdb);
	    // pgSnp bases coloring/display decision on count of items:
	    tg->customInt = slCount(tg->items);
	    }
	// Don't vcfFileFree here -- we are using its string pointers!
	}
    else
        errAbort("Unable to open VCF file '%s'", vcfFile);
    }
errCatchEnd(errCatch);
if (errCatch->gotError || vcff == NULL)
    {
    if (isNotEmpty(errCatch->message->string))
	tg->networkErrMsg = cloneString(errCatch->message->string);
    tg->drawItems = bigDrawWarning;
    tg->totalHeight = bigWarnTotalHeight;
    }
errCatchFree(&errCatch);
}

void vcfMethods(struct track *track)
/* Methods for Variant Call Format. */
{
pgSnpMethods(track);
track->mapItem = indelTweakMapItem;
// Disinherit next/prev flag and methods since we don't support next/prev:
track->nextExonButtonable = FALSE;
track->nextPrevExon = NULL;
track->nextPrevItem = NULL;
track->loadItems = vcfLoadItems;
track->canPack = TRUE;
}
