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
#if (defined USE_TABIX && defined KNETFILE_HOOKS)
#include "knetUdc.h"
#include "udc.h"
#endif//def USE_TABIX && KNETFILE_HOOKS

#ifdef USE_TABIX

//#*** TODO: use trackDb/cart setting or something
static boolean doHapClusterDisplay = TRUE;
static boolean colorHapByRefAlt = TRUE;

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
				     unsigned short *retGtHapEnd)
/* Given a bunch of VCF records with phased genotypes, build up one haplotype string
 * per chromosome that is the sequence of alleles in all variants (simplified to one base
 * per variant).  Each individual/sample will have two haplotype strings (unless haploid
 * like Y or male X).  Independently cluster the haplotype strings using hacTree with the 
 * center-weighted alpha functions above. Return an array of genotype+haplotype indices
 * in the order determined by the hacTree, and set retGtHapEnd to its length/end. */
{
int len = slCount(vcff->records);
// Should alpha depend on len?  Should the penalty drop off with distance?  Seems like
// straight-up exponential will cause the signal to drop to nothing pretty quickly...
double alpha = 0.5;
struct lm *lm = lmInit(0);
struct cwaExtraData helper = { centerIx, len, alpha, lm };
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
for (varIx = 0, rec = vcff->records;  rec != NULL;  varIx++, rec = rec->next)
    {
    vcfParseGenotypes(rec);
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
		c1->refCounts[varIx] = 1;
	    if (gt->isHaploid)
		haveHaploid = TRUE;
	    else
		{
		c2->leafCount = 1;
		c2->gtHapIx = (gtIx << 1) | 1;
		if (gt->hapIxB == 0)
		    c2->refCounts[varIx] = 1;
		}
	    }
	else
	    {
	    // Unphased heterozygote, don't use haplotype info for clustering
	    c1->leafCount = c2->leafCount = 1;
	    c1->gtHapIx = gtIx << 1;
	    c2->gtHapIx = (gtIx << 1) | 1;
	    c1->unkCounts[varIx] = c2->unkCounts[varIx] = 1;
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
return gtHapOrder;
}

INLINE char *hapIxToAllele(int hapIx, char *refAllele, char *altAlleles[])
/* Look up allele by index into reference allele and alternate allele(s). */
{
return (hapIx == 0) ? refAllele : altAlleles[hapIx-1];
}

INLINE Color colorFromGt(struct vcfGenotype *gt, int ploidIx, char *refAllele,
			 char *altAlleles[], int altCount, boolean grayUnphasedHet)
/* Color allele by base. */
{
int hapIx = ploidIx ? gt->hapIxB : gt->hapIxA;
char *allele = hapIxToAllele(hapIx, refAllele, altAlleles);
if (gt->isHaploid && hapIx > 0)
    return shadesOfGray[5];
if (grayUnphasedHet && !gt->isPhased && gt->hapIxA != gt->hapIxB)
    return shadesOfGray[5];
// Copying pgSnp color scheme here, using first base of allele which is not ideal for multibase
// but allows us to simplify it to 5 colors:
else if (allele[0] == 'A')
    return MG_RED;
else if (allele[0] == 'C')
    return MG_BLUE;
else if (allele[0] == 'G')
    return darkGreenColor;
else if (allele[0] == 'T')
    return MG_MAGENTA;
else
    return shadesOfGray[5];
}

INLINE Color colorFromRefAlt(struct vcfGenotype *gt, int hapIx, boolean grayUnphasedHet)
/* Color allele red for alternate allele, blue for reference allele -- 
 * except for special center variant, make it yellow/green for contrast. */
{
if (grayUnphasedHet && !gt->isPhased && gt->hapIxA != gt->hapIxB)
    return shadesOfGray[5];
int alIx = hapIx ? gt->hapIxB : gt->hapIxA;
return alIx ? MG_RED : MG_BLUE;
}


INLINE int drawOneHap(struct vcfGenotype *gt, int hapIx,
		      char *ref, char *altAlleles[], int altCount,
		      struct hvGfx *hvg, int x1, int y, int w, int itemHeight, int lineHeight)
/* Draw a base-colored box for genotype[hapIx].  Return the new y offset. */
{
Color color = colorHapByRefAlt ? colorFromRefAlt(gt, hapIx, TRUE) :
				 colorFromGt(gt, hapIx, ref, altAlleles, altCount, TRUE);
hvGfxBox(hvg, x1, y, w, itemHeight+1, color);
y += itemHeight+1;
return y;
}

INLINE char *gtSummaryString(struct vcfRecord *rec, char **altAlleles, int altCount)
// Make pgSnp-like mouseover text, but with genotype counts instead of allele counts.
// NOTE 1: Returned string is statically allocated, don't free it!
// NOTE 2: if revCmplDisp is set, this reverse-complements rec->ref and altAlleles!
{
static struct dyString *dy = NULL;
if (dy == NULL)
    dy = dyStringNew(0);
dyStringClear(dy);
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
    reverseComplement(rec->ref, strlen(rec->ref));
    for (i=0;  i < altCount;  i++)
	reverseComplement(altAlleles[i], strlen(altAlleles[i]));
    }

dyStringPrintf(dy, "%s/%s:%d %s/%s:%d %s/%s:%d", rec->ref, rec->ref, gtRefRefCount,
	       rec->ref, altAlleles[0], gtRefAltCount,
	       altAlleles[0], altAlleles[0], gtAltAltCount);
if (gtOtherCount > 0)
    dyStringPrintf(dy, " other:%d", gtOtherCount);
// Restore original values of pooled strings.
if (revCmplDisp)
    {
    reverseComplement(rec->ref, strlen(rec->ref));
    for (i=0;  i < altCount;  i++)
	reverseComplement(altAlleles[i], strlen(altAlleles[i]));
    }
return dy->string;
}

static void drawOneRec(struct vcfRecord *rec, unsigned short *gtHapOrder, int gtHapEnd,
		       struct track *tg, struct hvGfx *hvg, int xOff, int yOff, int width,
		       boolean isCenter)
/* Draw a stack of genotype bars for this record */
{
static struct dyString *tmp = NULL;
if (tmp == NULL)
    tmp = dyStringNew(0);
char *altAlleles[256];
int altCount;
const int lineHeight = tg->lineHeight;
const int itemHeight = tg->heightPer;
const double scale = scaleForPixels(width);
int x1 = round((double)(rec->chromStart-winStart)*scale) + xOff;
int x2 = round((double)(rec->chromEnd-winStart)*scale) + xOff;
int w = x2-x1;
if (w <= 1)
    {
    x1--;
    w = 3;
    }
int y = yOff;
dyStringClear(tmp);
dyStringAppend(tmp, rec->alt);
altCount = chopCommas(tmp->string, altAlleles);
int gtHapOrderIx;
for (gtHapOrderIx = 0;  gtHapOrderIx < gtHapEnd;  gtHapOrderIx++)
    {
    int gtHapIx = gtHapOrder[gtHapOrderIx];
    int hapIx = gtHapIx & 1;
    int gtIx = gtHapIx >>1;
    struct vcfGenotype *gt = &(rec->genotypes[gtIx]);
    y = drawOneHap(gt, hapIx, rec->ref, altAlleles, altCount,
		   hvg, x1, y, w, itemHeight, lineHeight);
    }
char *mouseoverText = gtSummaryString(rec, altAlleles, altCount);
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
		   "then click on the link below the variant name.");
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

static void vcfHapClusterDraw(struct track *tg, int seqStart, int seqEnd,
			      struct hvGfx *hvg, int xOff, int yOff, int width,
			      MgFont *font, Color color, enum trackVisibility vis)
/* Split samples' chromosomes (haplotypes), cluster them by center-weighted
 * alpha similarity, and draw in the order determined by clustering. */
{
const struct vcfFile *vcff = tg->extraUiData;
if (vcff->records == NULL)
    return;
unsigned short gtHapEnd = 0;
int ix, centerIx = getCenterVariantIx(tg, seqStart, seqEnd, vcff->records);
unsigned short *gtHapOrder = clusterChroms(vcff, centerIx, &gtHapEnd);
struct vcfRecord *rec, *centerRec = NULL;
for (rec = vcff->records, ix=0;  rec != NULL;  rec = rec->next, ix++)
    {
    if (ix == centerIx)
	centerRec = rec;
    else
	drawOneRec(rec, gtHapOrder, gtHapEnd, tg, hvg, xOff, yOff, width, FALSE);
    }
// Draw the center rec on top, outlined with black lines, to make sure it is very visible:
drawOneRec(centerRec, gtHapOrder, gtHapEnd, tg, hvg, xOff, yOff, width, TRUE);
}

static int vcfHapClusterTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return height of haplotype graph (2 * #samples * lineHeight);
 * 2 because we're assuming diploid genomes here, no XXY, tetraploid etc. */
{
// Should we make it single-height when on chrY?
const struct vcfFile *vcff = tg->extraUiData;
if (vcff->records == NULL)
    return 0;
int ploidy = 2;
tg->height = ploidy * vcff->genotypeCount * tg->lineHeight;
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
int vcfMaxErr = 100;
struct vcfFile *vcff = NULL;
/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    vcff = vcfTabixFileMayOpen(fileOrUrl, chromName, winStart, winEnd, vcfMaxErr);
    if (vcff != NULL)
	{
	if (doHapClusterDisplay && vcff->genotypeCount > 1 && vcff->genotypeCount < 3000 &&
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
