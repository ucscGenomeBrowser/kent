/* vcfTrack -- handlers for Variant Call Format data. */

#include "common.h"
#include "bigWarn.h"
#include "dystring.h"
#include "errCatch.h"
#include "hacTree.h"
#include "hdb.h"
#include "hgTracks.h"
#include "pgSnp.h"
#include "trashDir.h"
#include "vcf.h"
#include "vcfUi.h"
#if (defined USE_TABIX && defined KNETFILE_HOOKS)
#include "knetUdc.h"
#include "udc.h"
#endif//def USE_TABIX && KNETFILE_HOOKS

#ifdef USE_TABIX

static struct pgSnp *vcfFileToPgSnp(struct vcfFile *vcff)
/* Convert vcff's records to pgSnp; don't free vcff until you're done with pgSnp
 * because it contains pointers into vcff's records' chrom. */
{
struct pgSnp *pgsList = NULL;
struct vcfRecord *rec;
int maxLen = 33;
for (rec = vcff->records;  rec != NULL;  rec = rec->next)
    {
    struct pgSnp *pgs = pgSnpFromVcfRecord(rec);
    // Insertion sequences can be quite long; abbreviate here for display.
    int len = strlen(pgs->name);
    if (len > maxLen)
	{
	if (strchr(pgs->name, '/') != NULL)
	    {
	    char *copy = cloneString(pgs->name);
	    char *allele[8];
	    int cnt = chopByChar(copy, '/', allele, pgs->alleleCount);
	    int maxAlLen = maxLen / pgs->alleleCount;
	    pgs->name[0] = '\0';
	    int i;
	    for (i = 0;  i < cnt;  i++)
		{
		if (i > 0)
		    safencat(pgs->name, len+1, "/", 1);
		if (strlen(allele[i]) > maxAlLen-3)
		    strcpy(allele[i]+maxAlLen-3, "...");
		safencat(pgs->name, len+1, allele[i], maxAlLen);
		}
	    }
	else
	    strcpy(pgs->name+maxLen-3, "...");
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
    unsigned short gtHapIx;    // if leaf, (genotype index << 1) + hapIx (0 or 1 for diploid)
};

INLINE boolean isRef(const struct hapCluster *c, int varIx)
// Return TRUE if the leaves of cluster have at least as many reference alleles
// as alternate alleles for variant varIx.
{
unsigned short altCount = c->leafCount - c->refCounts[varIx] - c->unkCounts[varIx];
return (c->refCounts[varIx] >= altCount);
}

INLINE boolean hasUnk(const struct hapCluster *c, int varIx)
// Return TRUE if at least one haplotype in this cluster has an unknown/unphased value at varIx.
{
return (c->unkCounts[varIx] > 0);
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
    else if (hasUnk(kid1, i) != hasUnk(kid2, i))
	distance += weight/2;
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

INLINE void hapClusterToString(const struct hapCluster *c, struct dyString *dy, int len)
/* Write a text representation of hapCluster's alleles into dy.  */
{
dyStringClear(dy);
int i;
for (i=0;  i < len;  i++)
    dyStringAppendC(dy, (isRef(c, i) ? '0': '1'));
}

static int cwaCmp(const struct slList *item1, const struct slList *item2, void *extraData)
/* Convert hapCluster to allele strings for easy comparison by strcmp. */
{
const struct hapCluster *c1 = (const struct hapCluster *)item1;
const struct hapCluster *c2 = (const struct hapCluster *)item2;
struct cwaExtraData *helper = extraData;
static struct dyString *dy1 = NULL, *dy2 = NULL;
if (dy1 == NULL)
    {
    dy1 = dyStringNew(0);
    dy2 = dyStringNew(0);
    }
hapClusterToString(c1, dy1, helper->len);
hapClusterToString(c2, dy2, helper->len);
return strcmp(dy1->string, dy2->string);
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

static unsigned short *clusterChroms(const struct vcfFile *vcff, int centerIx,
				     unsigned short *retGtHapEnd, struct hacTree **retTree)
/* Given a bunch of VCF records with phased genotypes, build up one haplotype string
 * per chromosome that is the sequence of alleles in all variants (simplified to one base
 * per variant).  Each individual/sample will have two haplotype strings (unless haploid
 * like Y or male X).  Independently cluster the haplotype strings using hacTree with the 
 * center-weighted alpha functions above. Return an array of genotype+haplotype indices
 * in the order determined by the hacTree, and set retGtHapEnd to its length/end. */
{
int len = slCount(vcff->records);
// Limit the number of variants that we compare, to keep from timing out:
// (really what we should limit is the number of distinct haplo's passed to hacTree!)
const int maxVariantsPerSide = 20;
int startIx = max(0, centerIx - maxVariantsPerSide);
int endIx = min(len, centerIx+1 + maxVariantsPerSide);
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
	if (gt->isPhased || gt->isHaploid || (gt->hapIxA == gt->hapIxB))
	    {
	    // first chromosome:
	    c1->leafCount = 1;
	    c1->gtHapIx = gtIx << 1;
	    if (gt->hapIxA == 0)
		c1->refCounts[countIx] = 1;
	    if (gt->isHaploid)
		haveHaploid = TRUE;
	    else
		{
		c2->leafCount = 1;
		c2->gtHapIx = (gtIx << 1) | 1;
		if (gt->hapIxB == 0)
		    c2->refCounts[countIx] = 1;
		}
	    }
	else
	    {
	    // Unphased heterozygote, don't use haplotype info for clustering
	    c1->leafCount = c2->leafCount = 1;
	    c1->gtHapIx = gtIx << 1;
	    c2->gtHapIx = (gtIx << 1) | 1;
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
	    c = c->next;
	    }
	}
    }
struct hacTree *ht = hacTreeFromItems((struct slList *)(hapArray[0]), lm,
				      cwaDistance, cwaMerge, cwaCmp, &helper);
unsigned short *gtHapOrder = needMem(vcff->genotypeCount * 2 * sizeof(unsigned short));
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

INLINE char *gtSummaryString(struct vcfRecord *rec)
// Make pgSnp-like mouseover text, but with genotype counts instead of allele counts.
// NOTE: Returned string is statically allocated, don't free it!
{
static struct dyString *dy = NULL;
if (dy == NULL)
    dy = dyStringNew(0);
else
    dyStringClear(dy);
if (rec->alleleCount < 2)
    return "";
const struct vcfFile *vcff = rec->file;
int gtRefRefCount = 0, gtRefAltCount = 0, gtAltAltCount = 0, gtOtherCount = 0;
int i;
for (i=0;  i < vcff->genotypeCount;  i++)
    {
    struct vcfGenotype *gt = &(rec->genotypes[i]);
    if (gt->hapIxA == 0 && gt->hapIxB == 0)
	gtRefRefCount++;
    else if (gt->hapIxA == 1 && gt->hapIxB == 1)
	gtAltAltCount++;
    else if ((gt->hapIxA == 0 && gt->hapIxB == 1) || (gt->hapIxA == 1 && gt->hapIxB == 0))
	gtRefAltCount++;
    else
	gtOtherCount++;
    }
// These are pooled strings! Restore when done.
if (revCmplDisp)
    {
    for (i=0;  i < rec->alleleCount;  i++)
	reverseComplement(rec->alleles[i], strlen(rec->alleles[i]));
    }
dyStringPrintf(dy, "%s/%s:%d %s/%s:%d %s/%s:%d", rec->alleles[0], rec->alleles[0], gtRefRefCount,
	       rec->alleles[0], rec->alleles[1], gtRefAltCount,
	       rec->alleles[1], rec->alleles[1], gtAltAltCount);
if (gtOtherCount > 0)
    dyStringPrintf(dy, " other:%d", gtOtherCount);
// Restore original values of pooled strings.
if (revCmplDisp)
    {
    for (i=0;  i < rec->alleleCount;  i++)
	reverseComplement(rec->alleles[i], strlen(rec->alleles[i]));
    }
return dy->string;
}

// This is initialized when we start drawing:
static Color purple = 0;

static void drawOneRec(struct vcfRecord *rec, unsigned short *gtHapOrder, unsigned short gtHapCount,
		       struct track *tg, struct hvGfx *hvg, int xOff, int yOff, int width,
		       boolean isCenter, boolean colorByRefAlt)
/* Draw a stack of genotype bars for this record */
{
const double scale = scaleForPixels(width);
int x1 = round((double)(rec->chromStart-winStart)*scale) + xOff;
int x2 = round((double)(rec->chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
if (w <= 1)
    {
    x1--;
    w = 3;
    }
double hapsPerPix = (double)gtHapCount / (tg->height-1);
int pixIx;
for (pixIx = 0;  pixIx < tg->height;  pixIx++)
    {
    int gtHapOrderIxStart = round(hapsPerPix * pixIx);
    // Watch out for overflow:
    if (gtHapOrderIxStart >= gtHapCount)
	break;
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
	if (!gt->isPhased && gt->hapIxA != gt->hapIxB)
	    unks++;
	else
	    {
	    int alIx = hapIx ? gt->hapIxB : gt->hapIxA;
	    if (alIx)
		alts++;
	    else
		refs++;
	    }
	}
    const int fudgeFactor = 4;
    Color col = MG_BLACK;
    if (unks > (refs + alts))
	col = shadesOfGray[5];
    else if (alts > fudgeFactor * refs)
	col = colorByRefAlt ? MG_RED : pgSnpColor(rec->alleles[1]);
    else if (refs > fudgeFactor * alts)
	col = colorByRefAlt ? MG_BLUE : pgSnpColor(rec->alleles[0]);
    else
	col = colorByRefAlt ? purple : shadesOfGray[5];
    int y = yOff + pixIx;
    hvGfxLine(hvg, x1, y, x2, y, col);
    }
char *mouseoverText = gtSummaryString(rec);
if (isCenter)
    {
    // Thick black lines to distinguish this variant:
    int yBot = yOff + tg->height - 2;
    hvGfxBox(hvg, x1-3, yOff, 3, tg->height, MG_BLACK);
    hvGfxBox(hvg, x2, yOff, 3, tg->height, MG_BLACK);
    hvGfxLine(hvg, x1-2, yOff, x2+2, yOff, MG_BLACK);
    hvGfxLine(hvg, x1-2, yBot, x2+2, yBot, MG_BLACK);
    // Special mouseover instructions:
    static struct dyString *dy = NULL;
    if (dy == NULL)
	dy = dyStringNew(0);
    dyStringPrintf(dy, "%s   Haplotypes sorted on ", mouseoverText);
    char cartVar[512];
    safef(cartVar, sizeof(cartVar), "%s.centerVariantChrom", tg->tdb->track);
    char *centerChrom = cartOptionalString(cart, cartVar);
    if (centerChrom == NULL || !sameString(chromName, centerChrom))
	dyStringAppend(dy, "middle variant by default. ");
    else
	dyStringAppend(dy, "this variant. ");
    dyStringAppend(dy, "To anchor sorting to a different variant, click on that variant and "
		   "then click on the 'Use this variant' button below the variant name.");
    mouseoverText = dy->string;
    }
mapBoxHgcOrHgGene(hvg, rec->chromStart, rec->chromEnd, x1, yOff, w, tg->height, tg->track,
		  rec->name, mouseoverText, NULL, TRUE, NULL);
}

static int getCenterVariantIx(struct track *tg, int seqStart, int seqEnd,
			      struct vcfRecord *records)
// If the user hasn't specified a local variant/position to use as center,
// just use the median variant in window.
{
int defaultIx = (slCount(records)-1) / 2;
char cartVar[512];
safef(cartVar, sizeof(cartVar), "%s.centerVariantChrom", tg->tdb->track);
char *centerChrom = cartOptionalString(cart, cartVar);
if (centerChrom != NULL && sameString(chromName, centerChrom))
    {
    safef(cartVar, sizeof(cartVar), "%s.centerVariantPos", tg->tdb->track);
    int centerPos = cartInt(cart, cartVar);
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
				yFromNodeFunc *yFromNode, void *extraData)
/* Recursively draw the haplotype clustering tree in the left label area.
 * Returns pixel height for use at non-leaf levels of tree. */
{
const int branchW = 3;
const Color boundaryColor = MG_RED;
const int labelEnd = leftLabelX + leftLabelWidth;
const int rightEdge = labelEnd - 1;
if (yType == yrtStart || yType == yrtEnd)
    {
    // We're just getting vertical span of a leaf cluster, not drawing any lines.
    int yLeft, yRight;
    if (ht->left)
	yLeft = rDrawTreeInLabelArea(ht->left, hvg, yType, x, yFromNode, extraData);
    else
	yLeft = yFromNode(ht->itemOrCluster, extraData, yType);
    if (ht->right)
	yRight = rDrawTreeInLabelArea(ht->right, hvg, yType, x, yFromNode, extraData);
    else
	yRight = yFromNode(ht->itemOrCluster, extraData, yType);
    if (yType == yrtStart)
	return min(yLeft, yRight);
    else
	return max(yLeft, yRight);
    }
// Otherwise yType is yrtMidPoint.  If we have 2 children, we'll be drawing some lines:
if (ht->left != NULL && ht->right != NULL)
    {
    const int nextX = x + branchW;
    int midY;
    if (ht->childDistance == 0 || x+(2*branchW) > labelEnd)
	{
	// Treat this as a leaf cluster.
	// Recursing twice is wasteful. Could be avoided if this, and yFromNode,
	// returned both yStart and yEnd. However, the time to draw a tree of
	// 2188 hap's (1kG phase1 interim) is in the noise, so I consider it
	// not worth the effort of refactoring to save a sub-millisecond here.
	int yStartLeft = rDrawTreeInLabelArea(ht->left, hvg, yrtStart, nextX,
					      yFromNode, extraData);
	int yEndLeft = rDrawTreeInLabelArea(ht->left, hvg, yrtEnd, nextX,
					    yFromNode, extraData);
	int yStartRight = rDrawTreeInLabelArea(ht->right, hvg, yrtStart, nextX,
					       yFromNode, extraData);
	int yEndRight = rDrawTreeInLabelArea(ht->right, hvg, yrtEnd, nextX,
					     yFromNode, extraData);
	int yStart = min(yStartLeft, yStartRight);
	int yEnd = max(yEndLeft, yEndRight);
	midY = (yStart + yEnd) / 2;
	Color branchColor = (ht->childDistance == 0) ? MG_BLACK : shadesOfGray[5];
	hvGfxLine(hvg, x, midY, rightEdge, midY, branchColor);
	hvGfxLine(hvg, rightEdge, yStart, rightEdge, yEnd-1, branchColor);
	hvGfxLine(hvg, max(rightEdge-1, x), yStart, rightEdge, yStart, boundaryColor);
	hvGfxLine(hvg, max(rightEdge-1, x), yEnd-1, rightEdge, yEnd-1, boundaryColor);
	}
    else
	{
	int leftMid = rDrawTreeInLabelArea(ht->left, hvg, yrtMidPoint, nextX,
					   yFromNode, extraData);
	int rightMid = rDrawTreeInLabelArea(ht->right, hvg, yrtMidPoint, nextX,
					    yFromNode, extraData);
	midY = (leftMid + rightMid) / 2;
	hvGfxLine(hvg, nextX-1, leftMid, nextX-1, rightMid, MG_BLACK);
	hvGfxLine(hvg, x, midY, nextX-1, midY, MG_BLACK);
	}
    return midY;
    }
else if (ht->left != NULL)
    return rDrawTreeInLabelArea(ht->left, hvg, yType, x, yFromNode, extraData);
else if (ht->right != NULL)
    return rDrawTreeInLabelArea(ht->right, hvg, yType, x, yFromNode, extraData);
// Leaf node -- return pixel height. Draw a line if yType is midpoint.
int y = yFromNode(ht->itemOrCluster, extraData, yType);
if (yType == yrtMidPoint && x < labelEnd)
    {
    hvGfxLine(hvg, x, y, rightEdge, y, MG_BLACK);
    hvGfxLine(hvg, max(rightEdge-1, x), y, rightEdge, y, boundaryColor);
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
			 unsigned short gtHapCount, unsigned short *gtHapOrder)
/* Build a mapping of genotype and haplotype to pixel y coords. */
{
helper->gtHapCount = gtHapCount;
helper->gtHapIxToPxStart = needMem(gtHapCount * sizeof(unsigned short));
helper->gtHapIxToPxEnd = needMem(gtHapCount * sizeof(unsigned short));
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
if (gtHapIx >= helper->gtHapCount)
    errAbort("vcfTrack.c: gtHapIx %d out of range [0,%d).", gtHapIx, helper->gtHapCount);
int y;
if (yType == yrtStart)
    y = helper->gtHapIxToPxStart[gtHapIx];
else if (yType == yrtEnd)
    y = helper->gtHapIxToPxEnd[gtHapIx];
else
    y = (helper->gtHapIxToPxStart[gtHapIx] + helper->gtHapIxToPxEnd[gtHapIx]) / 2;
return y;
}

static void drawTreeInLabelArea(struct hacTree *ht, struct hvGfx *hvg, int yOff, int height,
				unsigned short gtHapCount, unsigned short *gtHapOrder)
/* Draw the haplotype clustering in the left label area (as much as fits there). */
{
// Figure out which hvg to use, save current clipping, and clip to left label coords:
struct hvGfx *hvgLL = (hvgSide != NULL) ? hvgSide : hvg;
int clipXBak, clipYBak, clipWidthBak, clipHeightBak;
hvGfxGetClip(hvgLL, &clipXBak, &clipYBak, &clipWidthBak, &clipHeightBak);
hvGfxUnclip(hvgLL);
hvGfxSetClip(hvgLL, leftLabelX, yOff, leftLabelWidth, height);
// Draw the tree:
int x = leftLabelX;
struct yFromNodeHelper helper = {0, NULL, NULL};
initYFromNodeHelper(&helper, yOff, height-1, gtHapCount, gtHapOrder);
(void)rDrawTreeInLabelArea(ht, hvgLL, yrtMidPoint, x, yFromHapNode, &helper);
// Restore the prior clipping:
hvGfxUnclip(hvgLL);
hvGfxSetClip(hvgLL, clipXBak, clipYBak, clipWidthBak, clipHeightBak);
}

static void ignoreEm(char *format, va_list args)
/* Ignore warnings from genotype parsing -- when there's one, there
 * are usually hundreds more just like it. */
{
}

static void vcfHapClusterDraw(struct track *tg, int seqStart, int seqEnd,
			      struct hvGfx *hvg, int xOff, int yOff, int width,
			      MgFont *font, Color color, enum trackVisibility vis)
/* Split samples' chromosomes (haplotypes), cluster them by center-weighted
 * alpha similarity, and draw in the order determined by clustering. */
{
const struct vcfFile *vcff = tg->extraUiData;
if (vcff->records == NULL)
    return;
purple = hvGfxFindColorIx(hvg, 0x99, 0x00, 0xcc);
boolean compositeLevel = isNameAtCompositeLevel(tg->tdb, tg->tdb->track);
char *colorBy = cartUsualStringClosestToHome(cart, tg->tdb, compositeLevel,
					     VCF_HAP_COLORBY_VAR, VCF_HAP_COLORBY_REFALT);
boolean colorByRefAlt = sameString(colorBy, VCF_HAP_COLORBY_REFALT);
pushWarnHandler(ignoreEm);
struct vcfRecord *rec;
for (rec = vcff->records;  rec != NULL;  rec = rec->next)
    vcfParseGenotypes(rec);
popWarnHandler();
unsigned short gtHapCount = 0;
int ix, centerIx = getCenterVariantIx(tg, seqStart, seqEnd, vcff->records);
struct hacTree *ht = NULL;
unsigned short *gtHapOrder = clusterChroms(vcff, centerIx, &gtHapCount, &ht);
struct vcfRecord *centerRec = NULL;
for (rec = vcff->records, ix=0;  rec != NULL;  rec = rec->next, ix++)
    {
    if (ix == centerIx)
	centerRec = rec;
    else
	drawOneRec(rec, gtHapOrder, gtHapCount, tg, hvg, xOff, yOff, width, FALSE, colorByRefAlt);
    }
// Draw the center rec on top, outlined with black lines, to make sure it is very visible:
drawOneRec(centerRec, gtHapOrder, gtHapCount, tg, hvg, xOff, yOff, width, TRUE, colorByRefAlt);
// Draw as much of the tree as can fit in the left label area:
drawTreeInLabelArea(ht, hvg, yOff, tg->height, gtHapCount, gtHapOrder);
}

static int vcfHapClusterTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return height of haplotype graph (2 * #samples * lineHeight);
 * 2 because we're assuming diploid genomes here, no XXY, tetraploid etc. */
{
// Should we make it single-height when on chrY?
const struct vcfFile *vcff = tg->extraUiData;
if (vcff->records == NULL)
    return 0;
int ploidy = sameString(chromName, "chrY") ? 1 : 2;
int simpleHeight = ploidy * vcff->genotypeCount * tg->lineHeight;
int defaultHeight = min(simpleHeight, VCF_DEFAULT_HAP_HEIGHT);
int cartHeight = cartOrTdbInt(cart, tg->tdb, VCF_HAP_HEIGHT_VAR, defaultHeight);
tg->height = min(cartHeight+1, maximumTrackHeight(tg));
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

static void vcfTabixLoadItems(struct track *tg)
/* Load items in window from VCF file using its tabix index file. */
{
char *fileOrUrl = NULL;
/* Figure out url or file name. */
if (tg->parallelLoading)
    {
    /* do not use mysql uring parallel-fetch load */
    fileOrUrl = trackDbSetting(tg->tdb, "bigDataUrl");
    }
else
    {
    // TODO: may need to handle per-chrom files like bam, maybe fold bamFileNameFromTable into this:
    struct sqlConnection *conn = hAllocConnTrack(database, tg->tdb);
    fileOrUrl = bbiNameFromSettingOrTable(tg->tdb, conn, tg->table);
    hFreeConn(&conn);
    }
int vcfMaxErr = -1;
struct vcfFile *vcff = NULL;
boolean compositeLevel = isNameAtCompositeLevel(tg->tdb, tg->tdb->track);
boolean hapClustEnabled = cartUsualBooleanClosestToHome(cart, tg->tdb, compositeLevel,
							VCF_HAP_ENABLED_VAR, TRUE);
/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    vcff = vcfTabixFileMayOpen(fileOrUrl, chromName, winStart, winEnd, vcfMaxErr);
    if (vcff != NULL)
	{
	if (hapClustEnabled && vcff->genotypeCount > 1 && vcff->genotypeCount < 3000 &&
	    (tg->visibility == tvPack || tg->visibility == tvSquish))
	    vcfHapClusterOverloadMethods(tg, vcff);
	else
	    {
	    tg->items = vcfFileToPgSnp(vcff);
	    // pgSnp bases coloring/display decision on count of items:
	    tg->customInt = slCount(tg->items);
	    }
	// Don't vcfFileFree here -- we are using its string pointers!
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
pgSnpMethods(track);
track->loadItems = vcfTabixLoadItems;
track->canPack = TRUE;
}

#else // no USE_TABIX:

// If code was not built with USE_TABIX=1, but there are vcfTabix tracks, display a message
// in place of the tracks (instead of annoying "No track handler" warning messages).

static void drawUseVcfTabixWarning(struct track *tg, int seqStart, int seqEnd, struct hvGfx *hvg,
				   int xOff, int yOff, int width, MgFont *font, Color color,
				   enum trackVisibility vis)
/* Draw a message saying that the code needs to be built with USE_TABIX=1. */
{
char message[512];
safef(message, sizeof(message),
      "Get tabix from samtools.sourceforge.net and recompile kent/src with USE_TABIX=1");
Color yellow = hvGfxFindRgb(hvg, &undefinedYellowColor);
hvGfxBox(hvg, xOff, yOff, width, tg->heightPer, yellow);
hvGfxTextCentered(hvg, xOff, yOff, width, tg->heightPer, MG_BLACK, font, message);
}

void vcfTabixMethods(struct track *track)
/* Methods for VCF alignment files, in absence of tabix lib. */
{
#if (defined USE_TABIX && defined KNETFILE_HOOKS)
knetUdcInstall();
#endif//def USE_TABIX && KNETFILE_HOOKS
messageLineMethods(track);
track->drawItems = drawUseVcfTabixWarning;
}

#endif // no USE_TABIX
