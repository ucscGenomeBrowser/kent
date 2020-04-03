/* vcfTrack -- handlers for Variant Call Format data. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "bigWarn.h"
#include "dystring.h"
#include "errCatch.h"
#include "hacTree.h"
#include "hdb.h"
#include "hgColors.h"
#include "hgTracks.h"
#include "pgSnp.h"
#include "phyloTree.h"
#include "trashDir.h"
#include "vcf.h"
#include "vcfUi.h"
#include "knetUdc.h"
#include "udc.h"


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

static void filterRecords(struct vcfFile *vcff, struct trackDb *tdb)
/* If a filter is specified in the cart, remove any records that don't pass filter. */
{
double minQual = VCF_DEFAULT_MIN_QUAL;
struct slName *filterValues = NULL;
double minFreq = VCF_DEFAULT_MIN_ALLELE_FREQ;
boolean gotQualFilter = getMinQual(tdb, &minQual);
boolean gotFilterFilter = getFilterValues(tdb, &filterValues);
boolean gotMinFreqFilter = getMinFreq(tdb, &minFreq);
if (! (gotQualFilter || gotFilterFilter || gotMinFreqFilter) )
    return;

struct vcfRecord *rec, *nextRec, *newList = NULL;
for (rec = vcff->records;  rec != NULL;  rec = nextRec)
    {
    nextRec = rec->next;
    if (! ((gotQualFilter && minQualFail(rec, minQual)) ||
	   (gotFilterFilter && filterColumnFail(rec, filterValues)) ||
	   (gotMinFreqFilter && minFreqFail(rec, minFreq)) ))
	slAddHead(&newList, rec);
    }
slReverse(&newList);
vcff->records = newList;
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
 * because it contains pointers into vcff's records' chrom. */
{
struct pgSnp *pgsList = NULL;
struct vcfRecord *rec;
int maxLen = 33;
int maxAlCount = 5;
for (rec = vcff->records;  rec != NULL;  rec = rec->next)
    {
    struct pgSnpVcfStartEnd *psvs = needMem(sizeof(*psvs));
    psvs->vcfStart = vcfRecordTrimIndelLeftBase(rec);
    psvs->vcfEnd = vcfRecordTrimAllelesRight(rec);
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
    unsigned short *refCounts; // per-variant count of reference alleles observed
    unsigned short *unkCounts; // per-variant count of unknown (or unphased het) alleles
    unsigned short leafCount;  // number of leaves under this node (or 1 if leaf)
    unsigned short gtHapIx;    // if leaf, (genotype index << 1) + hap (0 or 1 for diploid)
};

INLINE boolean isRef(const struct hapCluster *c, int varIx)
// Return TRUE if the leaves of cluster have at least as many reference alleles
// as alternate alleles for variant varIx.
{
unsigned short altCount = c->leafCount - c->refCounts[varIx] - c->unkCounts[varIx];
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
c->refCounts = lmAlloc(helper->localMem, helper->len * sizeof(unsigned short));
c->unkCounts = lmAlloc(helper->localMem, helper->len * sizeof(unsigned short));
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

void rSetGtHapOrder(struct hacTree *ht, unsigned short *gtHapOrder, unsigned short *retGtHapEnd)
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

static unsigned short *clusterHaps(const struct vcfFile *vcff, int centerIx,
				   int startIx, int endIx,
				   unsigned short *retGtHapEnd, struct hacTree **retTree)
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
unsigned short *gtHapOrder = needMem(vcff->genotypeCount * ploidy * sizeof(unsigned short));
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
    baseMode
    };

static Color colorByAltOnly(int refs, int alts, int unks)
/* Coloring alternate alleles only: shade by proportion of alt alleles to refs, unknowns */
{
if (unks > (refs + alts))
    return undefYellow;
int grayIx = hGrayInRange(alts, 0, alts+refs+unks, maxShade+1) - 1; // undo force to 1
return shadesOfGray[grayIx];
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

// tg->height needs an extra pixel at the bottom; it's eaten by the clipping rectangle:
#define CLIP_PAD 1

static void drawOneRec(struct vcfRecord *rec, unsigned short *gtHapOrder, unsigned short gtHapCount,
		       struct track *tg, struct hvGfx *hvg, int xOff, int yOff, int width,
		       boolean isClustered, boolean isCenter, enum hapColorMode colorMode)
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
if (colorMode == altOnlyMode)
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
    else
	col = colorByAltOnly(refs, alts, unks);
    if (col != MG_WHITE)
	hvGfxLine(hvg, x1, y, x2, y, col);
    }
int yBot = yOff + tg->height - CLIP_PAD - 1;
if (isCenter)
    {
    if (colorMode == altOnlyMode)
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
if (colorMode == altOnlyMode)
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
		       x1, y1, x2, y2, helper->track);
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
    unsigned short gtHapCount;
    unsigned short *gtHapIxToPxStart;
    unsigned short *gtHapIxToPxEnd;
    };

void initYFromNodeHelper(struct yFromNodeHelper *helper, int yOff, int height,
			 unsigned short gtHapCount, unsigned short *gtHapOrder,
			 int genotypeCount)
/* Build a mapping of genotype and haplotype to pixel y coords. */
{
helper->gtHapCount = gtHapCount;
helper->gtHapIxToPxStart = needMem(genotypeCount * 2 * sizeof(unsigned short));
helper->gtHapIxToPxEnd = needMem(genotypeCount * 2 * sizeof(unsigned short));
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
unsigned short gtHapIx = ((const struct hapCluster *)itemOrCluster)->gtHapIx;
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
return colorMode;
}

static boolean vcfHapClusterDrawInit(struct track *tg, struct vcfFile *vcff, struct hvGfx *hvg,
                                     enum hapColorMode *retHapColorMode)
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
    vcfParseGenotypes(rec);
popWarnHandler();
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
if (!vcfHapClusterDrawInit(tg, vcff, hvg, &colorMode))
    return;
purple = hvGfxFindColorIx(hvg, 0x99, 0x00, 0xcc);
unsigned short gtHapCount = 0;
int nRecords = slCount(vcff->records);
int centerIx = getCenterVariantIx(tg, seqStart, seqEnd, vcff->records);
// Limit the number of variants that we compare, to keep from timing out:
// (really what we should limit is the number of distinct haplo's passed to hacTree!)
// In the meantime, this should at least be a cart var...
int maxVariantsPerSide = 50;
int startIx = max(0, centerIx - maxVariantsPerSide);
int endIx = min(nRecords, centerIx+1 + maxVariantsPerSide);
struct hacTree *ht = NULL;
unsigned short *gtHapOrder = clusterHaps(vcff, centerIx, startIx, endIx, &gtHapCount, &ht);
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
	drawOneRec(rec, gtHapOrder, gtHapCount, tg, hvg, xOff, yOff, width, isClustered, FALSE,
		   colorMode);
    }
// Draw the center rec on top, outlined with black lines, to make sure it is very visible:
drawOneRec(centerRec, gtHapOrder, gtHapCount, tg, hvg, xOff, yOff, width, TRUE, TRUE,
	   colorMode);
// Draw as much of the tree as can fit in the left label area:
int extraPixel = (colorMode == altOnlyMode) ? 1 : 0;
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
                             unsigned short *gtHapOrder, int gtHapCount, MgFont *font, Color color,
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
                             unsigned short *gtHapOrder, int gtHapCount, char *track)
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
		       leftLabelX, y+yStart, labelEnd, y+yStart+1, track);
    }
}

static unsigned short *gtHapOrderFromGtOrder(struct vcfFile *vcff,
                                             boolean *retIsAllDiploid, int *retGtHapCount)
{
int ploidy = 2; // Assuming diploid genomes here, no XXY, tetraploid etc.
int gtCount = vcff->genotypeCount;
boolean isAllDiploid = TRUE;
unsigned short *gtHapOrder = needMem(gtCount * ploidy * sizeof(unsigned short));
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
if (!vcfHapClusterDrawInit(tg, vcff, hvg, &colorMode))
    return;
boolean isAllDiploid;
int gtHapCount;
unsigned short *gtHapOrder = gtHapOrderFromGtOrder(vcff, &isAllDiploid, &gtHapCount);
struct vcfRecord *rec;
for (rec = vcff->records;  rec != NULL;  rec = rec->next)
    drawOneRec(rec, gtHapOrder, gtHapCount, tg, hvg, xOff, yOff, width, FALSE, FALSE,
               colorMode);
// If height is sufficient, draw sample names as left labels; otherwise make mouseover titles
// with sample names for each pixel y offset.
int extraPixel = (colorMode == altOnlyMode) ? 1 : 0;
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
char *fileName = nextWord(&hapMethod);
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
                                   unsigned short *gtHapOrder, int *pGtHapCount,
                                   unsigned short *leafOrderToHapOrderStart,
                                   unsigned short *leafOrderToHapOrderEnd, int *pLeafCount)
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

static unsigned short *gtHapOrderFromTree(struct vcfFile *vcff, struct phyloTree *tree,
                                          unsigned short **retLeafOrderToHapOrderStart,
                                          unsigned short **retLeafOrderToHapOrderEnd,
                                          int *retGtHapCount)
/* Alloc & return gtHapOrder, set to samples in the order we encounter them in tree.
 * Also build up maps of leaf order to low and high gtHapIx, for drawing the tree later. */
{
struct hash *sampleToIx = makeSampleToIx(vcff);
int ploidy = 2; // Assuming diploid genomes here, no XXY, tetraploid etc.
int gtCount = vcff->genotypeCount;
unsigned short *gtHapOrder = needMem(gtCount * ploidy * sizeof(unsigned short));
*retLeafOrderToHapOrderStart = needMem(gtCount * sizeof(unsigned short));
*retLeafOrderToHapOrderEnd = needMem(gtCount * sizeof(unsigned short));
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
                                  unsigned short *leafOrderToHapOrderStart,
                                  unsigned short *leafOrderToHapOrderEnd,
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

static void rDrawPhyloTreeInLabelArea(struct phyloTree *node, struct hvGfx *hvg, int x,
                                      int yOff, double pxPerHap, MgFont *font,
                                      boolean drawRectangle)
/* Recursively draw the tree in the left label area. */
{
const int branchW = 4;
int labelEnd = leftLabelX + leftLabelWidth;
// Misuse the branch length value as RGB color (if it's the typical small number, will still
// draw as approximately black):
unsigned int rgb = node->ident->length;
Color color = MAKECOLOR_32( ((rgb>>16)&0xff), ((rgb>>8)&0xff), (rgb&0xff) );

if (node->numEdges > 0)
    {
    // Draw each child and a horizontal line to child
    int minY = -1, maxY = 0;
    int ix;
    for (ix = 0;  ix < node->numEdges;  ix++)
        {
        struct phyloTree *child = node->edges[ix];
        rDrawPhyloTreeInLabelArea(child, hvg, x+branchW, yOff, pxPerHap, font, drawRectangle);
        struct nodeCoords *childCoords = child->priv;
        int childY = yOff + ((0.5 + childCoords->rank) * pxPerHap);
        hvGfxLine(hvg, x, childY, x+branchW, childY, color);
        if (minY < 0 || childY < minY)
            minY = childY;
        if (childY > maxY)
            maxY = childY;
        }
    // Draw a vertical line to connect the children
    hvGfxLine(hvg, x, minY, x, maxY, color);
    }
else
    {
    // Leaf node -- draw a horizontal line, and label if there is space to right of tree
    struct nodeCoords *coords = node->priv;
    int y = yOff + ((0.5 + coords->rank) * pxPerHap);
    hvGfxLine(hvg, x, y, x+branchW, y, color);
    int textX = x + branchW + 3;
    if (pxPerHap >= tl.fontHeight+1 && textX < labelEnd)
        hvGfxText(hvg, textX, y, MG_BLACK, font, node->ident->name);
    }
}

static void drawPhyloTreeInLabelArea(struct phyloTree *tree, struct hvGfx *hvg, int yOff,
                                     int clipHeight, int gtHapCount,
                                     MgFont *font, boolean drawRectangle,
                                     unsigned short *leafOrderToHapOrderStart,
                                     unsigned short *leafOrderToHapOrderEnd)
{
struct hvGfx *hvgLL = (hvgSide != NULL) ? hvgSide : hvg;
int clipXBak, clipYBak, clipWidthBak, clipHeightBak;
hvGfxGetClip(hvgLL, &clipXBak, &clipYBak, &clipWidthBak, &clipHeightBak);
hvGfxUnclip(hvgLL);
hvGfxSetClip(hvgLL, leftLabelX, yOff, leftLabelWidth, clipHeight);
// Figure out rank (vertical position) and depth (horizontal position) of every node in tree:
phyloTreeAddNodeCoords(tree, leafOrderToHapOrderStart, leafOrderToHapOrderEnd, 0);
// Draw the tree:
int x = leftLabelX;
double pxPerHap = (double)clipHeight / gtHapCount;
rDrawPhyloTreeInLabelArea(tree, hvgLL, x, yOff, pxPerHap, font, drawRectangle);
// Restore the prior clipping:
hvGfxUnclip(hvgLL);
hvGfxSetClip(hvgLL, clipXBak, clipYBak, clipWidthBak, clipHeightBak);
}

static void vcfGtHapTreeFileDraw(struct track *tg, int seqStart, int seqEnd,
                                 struct hvGfx *hvg, int xOff, int yOff, int width,
                                 MgFont *font, Color color, enum trackVisibility vis)
/* Draw rows in the same fashion as vcfHapClusterDraw, but instead of clustering, use the
 * order in which samples appear in the VCF file. */
{
struct vcfFile *vcff = tg->extraUiData;
enum hapColorMode colorMode;
if (!vcfHapClusterDrawInit(tg, vcff, hvg, &colorMode))
    return;
struct phyloTree *tree = getTreeFromFile(tg->tdb);
int gtHapCount;
unsigned short *leafOrderToHapOrderStart, *leafOrderToHapOrderEnd;
unsigned short *gtHapOrder = gtHapOrderFromTree(vcff, tree,
                                                &leafOrderToHapOrderStart, &leafOrderToHapOrderEnd,
                                                &gtHapCount);
struct vcfRecord *rec;
for (rec = vcff->records;  rec != NULL;  rec = rec->next)
    drawOneRec(rec, gtHapOrder, gtHapCount, tg, hvg, xOff, yOff, width, FALSE, FALSE,
               colorMode);
int extraPixel = (colorMode == altOnlyMode) ? 1 : 0;
int hapHeight = tg->height - CLIP_PAD - 2*extraPixel;
char *treeAngle = cartOrTdbString(cart, tg->tdb, VCF_HAP_TREEANGLE_VAR, VCF_DEFAULT_HAP_TREEANGLE);
boolean drawRectangle = sameString(treeAngle, VCF_HAP_TREEANGLE_RECTANGLE);
drawPhyloTreeInLabelArea(tree, hvg, yOff+extraPixel, hapHeight, gtHapCount, font,
                         drawRectangle, leafOrderToHapOrderStart, leafOrderToHapOrderEnd);
drawSampleTitles(vcff, yOff+extraPixel, hapHeight, gtHapOrder, gtHapCount, tg->track);
}

static int vcfHapClusterTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return height of haplotype graph (2 * #samples * lineHeight);
 * 2 because we're assuming diploid genomes here, no XXY, tetraploid etc. */
{
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
int extraPixel = (getColorMode(tg->tdb) == altOnlyMode) ? 1 : 0;
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



static void vcfTabixLoadItems(struct track *tg)
/* Load items in window from VCF file using its tabix index file. */
{
char *fileOrUrl = NULL;
char *tbiFileOrUrl = trackDbSetting(tg->tdb, "bigDataIndex"); // unrelated to mysql

/* Figure out url or file name. */
if (tg->parallelLoading)
    {
    /* do not use mysql during parallel-fetch load */
    fileOrUrl = trackDbSetting(tg->tdb, "bigDataUrl");
    }
else
    {
    struct sqlConnection *conn = hAllocConnTrack(database, tg->tdb);
    fileOrUrl = bbiNameFromSettingOrTableChrom(tg->tdb, conn, tg->table, chromName);
    hFreeConn(&conn);
    }

if (isEmpty(fileOrUrl))
    return;
int vcfMaxErr = -1;
struct vcfFile *vcff = NULL;
boolean hapClustEnabled = cartOrTdbBoolean(cart, tg->tdb, VCF_HAP_ENABLED_VAR, TRUE);
if (slCount(windows)>1)
    hapClustEnabled = FALSE;  // haplotype sorting display not currently available with multiple windows.
/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    vcff = vcfTabixFileAndIndexMayOpen(fileOrUrl, tbiFileOrUrl, chromName, winStart, winEnd, vcfMaxErr, -1);
    if (vcff != NULL)
	{
	filterRecords(vcff, tg->tdb);
	if (hapClustEnabled && vcff->genotypeCount > 1 && vcff->genotypeCount < 3000 &&
	    (tg->visibility == tvPack || tg->visibility == tvSquish))
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
	filterRecords(vcff, tg->tdb);
	if (hapClustEnabled && vcff->genotypeCount > 1 && vcff->genotypeCount < 3000 &&
	    (tg->visibility == tvPack || tg->visibility == tvSquish))
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
