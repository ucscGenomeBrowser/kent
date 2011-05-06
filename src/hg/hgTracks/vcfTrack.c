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

#define VCF_MAX_ALLELE_LEN 80

static struct pgSnp *vcfFileToPgSnp(struct vcfFile *vcff)
/* Convert vcff's records to pgSnp; don't free vcff until you're done with pgSnp
 * because it contains pointers into vcff's records' chrom. */
{
struct pgSnp *pgsList = NULL;
struct vcfRecord *rec;
struct dyString *dy = dyStringNew(0);
for (rec = vcff->records;  rec != NULL;  rec = rec->next)
    {
    struct pgSnp *pgs;
    AllocVar(pgs);
    pgs->chrom = rec->chrom;
    pgs->chromStart = rec->chromStart;
    pgs->chromEnd = rec->chromEnd;
    // Build up slash-separated allele string from rec->ref + rec->alt:
    dyStringClear(dy);
    dyStringAppend(dy, rec->ref);
    int altCount, i;
    if (sameString(rec->alt, "."))
	altCount = 0;
    else
	{
	char *words[64]; // we're going to truncate anyway if there are this many alleles!
	char copy[VCF_MAX_ALLELE_LEN+1];
	strncpy(copy, rec->alt, VCF_MAX_ALLELE_LEN);
	copy[VCF_MAX_ALLELE_LEN] = '\0';
	altCount = chopCommas(copy, words);
	for (i = 0;  i < altCount && dy->stringSize < VCF_MAX_ALLELE_LEN;  i++)
	    dyStringPrintf(dy, "/%s", words[i]);
	if (i < altCount)
	    altCount = i;
	}
    pgs->name = cloneStringZ(dy->string, dy->stringSize+1);
    pgs->alleleCount = altCount + 1;
    // Build up comma-sep list of per-allele counts, if available:
    dyStringClear(dy);
    int refAlleleCount = 0;
    boolean gotAltCounts = FALSE;
    for (i = 0;  i < rec->infoCount;  i++)
	if (sameString(rec->infoElements[i].key, "AN"))
	    {
	    refAlleleCount = rec->infoElements[i].values[0].datInt;
	    break;
	    }
    for (i = 0;  i < rec->infoCount;  i++)
	if (sameString(rec->infoElements[i].key, "AC"))
	    {
	    int alCounts[64];
	    int j;
	    gotAltCounts = (rec->infoElements[i].count > 0);
	    for (j = 0;  j < rec->infoElements[i].count;  j++)
		{
		int ac = rec->infoElements[i].values[j].datInt;
		if (j < altCount)
		    alCounts[1+j] = ac;
		refAlleleCount -= ac;
		}
	    if (gotAltCounts)
		{
		while (j++ < altCount)
		    alCounts[1+j] = -1;
		alCounts[0] = refAlleleCount;
		if (refAlleleCount >= 0)
		    dyStringPrintf(dy, "%d", refAlleleCount);
		else
		    dyStringAppend(dy, "-1");
		for (j = 0;  j < altCount;  j++)
		    if (alCounts[1+j] >= 0)
			dyStringPrintf(dy, ",%d", alCounts[1+j]);
		    else
			dyStringAppend(dy, ",-1");
		}
	    break;
	    }
    if (refAlleleCount > 0 && !gotAltCounts)
	dyStringPrintf(dy, "%d", refAlleleCount);
    pgs->alleleFreq = cloneStringZ(dy->string, dy->stringSize+1);
    // Build up comma-sep list... supposed to be per-allele quality scores but I think
    // the VCF spec only gives us one BQ... for the reference position?  should ask.
    dyStringClear(dy);
    for (i = 0;  i < rec->infoCount;  i++)
	if (sameString(rec->infoElements[i].key, "BQ"))
	    {
	    float qual = rec->infoElements[i].values[0].datFloat;
	    dyStringPrintf(dy, "%.1f", qual);
	    int j;
	    for (j = 0;  j < altCount;  j++)
		dyStringPrintf(dy, ",%.1f", qual);
	    break;
	    }
    pgs->alleleScores = cloneStringZ(dy->string, dy->stringSize+1);
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

static unsigned short *clusterChroms(const struct vcfFile *vcff, unsigned short *retGtHapEnd)
/* Given a bunch of VCF records with phased genotypes, build up one haplotype string
 * per chromosome that is the sequence of alleles in all variants (simplified to one base
 * per variant).  Each individual/sample will have two haplotype strings (unless haploid
 * like Y or male X).  Independently cluster the haplotype strings using hacTree with the 
 * center-weighted alpha functions above. Return an array of genotype+haplotype indices
 * in the order determined by the hacTree, and set retGtHapEnd to its length/end. */
{
int len = slCount(vcff->records);
// Use the median variant in the window as the center; would be even nicer to allow
// the user to choose a variant (or position) to use as center:
int center = len / 2;
// Should alpha depend on len?  Should the penalty drop off with distance?  Seems like
// straight-up exponential will cause the signal to drop to nothing pretty quickly...
double alpha = 0.5;
struct lm *lm = lmInit(0);
struct cwaExtraData helper = { center, len, alpha, lm };
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
/* Color allele red for alternate allele, blue for reference allele. */
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

static void vcfHapClusterDraw(struct track *tg, int seqStart, int seqEnd,
			      struct hvGfx *hvg, int xOff, int yOff, int width,
			      MgFont *font, Color color, enum trackVisibility vis)
/* Split samples' chromosomes (haplotypes), cluster them by center-weighted
 * alpha similarity, and draw in the order determined by clustering. */
{
const struct vcfFile *vcff = tg->extraUiData;
unsigned short gtHapEnd = 0;
unsigned short *gtHapOrder = clusterChroms(vcff, &gtHapEnd);
struct dyString *tmp = dyStringNew(0);
struct vcfRecord *rec;
const int lineHeight = tg->lineHeight;
const int itemHeight = tg->heightPer;
const double scale = scaleForPixels(width);
for (rec = vcff->records;  rec != NULL;  rec = rec->next)
    {
    dyStringClear(tmp);
    dyStringAppend(tmp, rec->alt);
    char *altAlleles[256];
    int altCount = chopCommas(tmp->string, altAlleles);
    int x1 = round((double)(rec->chromStart-winStart)*scale) + xOff;
    int x2 = round((double)(rec->chromEnd-winStart)*scale) + xOff;
    int w = x2-x1;
    if (w < 1)
	w = 1;
    int y = yOff;
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
    //#*** TODO: pgSnp-like mouseover text?
    mapBoxHgcOrHgGene(hvg, rec->chromStart, rec->chromEnd, x1, yOff, w, tg->height, tg->track,
		      rec->name, rec->name, FALSE, TRUE, NULL);
    }
// left labels?
}

static int vcfHapClusterTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return height of haplotype graph (2 * #samples * lineHeight);
 * 2 because we're assuming diploid genomes here, no XXY, tetraploid etc. */
{
// Should we make it single-height when on chrY?
const struct vcfFile *vcff = tg->extraUiData;
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
struct sqlConnection *conn = hAllocConnTrack(database, tg->tdb);
// TODO: may need to handle per-chrom files like bam, maybe fold bamFileNameFromTable into this::
char *fileOrUrl = bbiNameFromSettingOrTable(tg->tdb, conn, tg->table);
hFreeConn(&conn);
int vcfMaxErr = 100;
struct vcfFile *vcff = NULL;
/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    vcff = vcfTabixFileMayOpen(fileOrUrl, chromName, winStart, winEnd, vcfMaxErr);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    if (isNotEmpty(errCatch->message->string))
	tg->networkErrMsg = cloneString(errCatch->message->string);
    tg->drawItems = bigDrawWarning;
    tg->totalHeight = bigWarnTotalHeight;
    }
errCatchFree(&errCatch);
if (vcff != NULL)
    {
    if (doHapClusterDisplay && vcff->genotypeCount > 0 && vcff->genotypeCount < 3000 &&
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
