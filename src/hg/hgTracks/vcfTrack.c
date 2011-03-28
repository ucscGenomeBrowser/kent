/* vcfTrack -- handlers for Variant Call Format data. */

#include "common.h"
#include "bigWarn.h"
#include "dystring.h"
#include "errCatch.h"
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
static boolean boringBed = FALSE;
static boolean doHapGraphDisplay = TRUE;
static boolean sortByGenotype = TRUE;

static struct bed4 *vcfFileToBed4(struct vcfFile *vcff)
/* Convert vcff's records to bed4; don't free vcff until you're done with bed4
 * because bed4 contains pointers into vcff's records' chrom and name. */
{
struct bed4 *bedList = NULL;
struct vcfRecord *rec;
for (rec = vcff->records;  rec != NULL;  rec = rec->next)
    {
    struct bed4 *bed;
    AllocVar(bed);
    bed->chrom = rec->chrom;
    bed->chromStart = rec->chromStart;
    bed->chromEnd = rec->chromEnd;
    bed->name = rec->name;
    slAddHead(&bedList, bed);
    }
slReverse(&bedList);
return bedList;
}

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

INLINE char *hapIxToAllele(int hapIx, char *refAllele, char *altAlleles[])
/* Look up allele by index into reference allele and alternate allele(s). */
{
return (hapIx == 0) ? refAllele : altAlleles[hapIx-1];
}

INLINE Color colorFromGt(struct vcfGenotype *gt, int ploidIx, char *refAllele,
			 char *altAlleles[], int altCount)
/* Color allele by base. */
{
int hapIx = ploidIx ? gt->hapIxB : gt->hapIxA;
char *allele = hapIxToAllele(hapIx, refAllele, altAlleles);
if (gt->isHaploid && hapIx > 0)
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

INLINE int drawOneGenotype(struct vcfGenotype *gt, char *ref, char *altAlleles[], int altCount,
			    struct hvGfx *hvg, int x1, int y, int w,
			    int itemHeight, int lineHeight)
/* Draw a base-colored box for each haplotype in genotype, with a separating line
 * (black for phased genotypes, gray for unphased). Return the new y offset. */
{
int ploidIx, ploidy = 2; // Assuming diploid genomes here, no XXY, tetraploid etc.
for (ploidIx = 0;  ploidIx < ploidy;  ploidIx++)
    {
    if (ploidIx > 0 && gt->isHaploid)
	break;
    Color color = colorFromGt(gt, ploidIx, ref, altAlleles, altCount);
    hvGfxBox(hvg, x1, y, w, itemHeight, color);
    if (ploidIx < ploidy-1)
	hvGfxLine(hvg, x1, y+itemHeight, x1+w-1, y+itemHeight,
		  (gt->isPhased ? MG_BLACK : MG_GRAY));
    y += lineHeight;
    }
return y;
}

static void vcfHapGraphDrawGtOrdered(struct track *tg, int gtOrder[],
				     struct hvGfx *hvg, int xOff, int yOff, int width,
				     MgFont *font, Color color, enum trackVisibility vis)
/* Draw per-individual haplotypes in gtOrder (maybe phased, maybe not) from a VCF
 * file that contains genotypes. */
{
const int lineHeight = tg->lineHeight;
const int itemHeight = tg->heightPer;
const double scale = scaleForPixels(width);
struct dyString *tmp = dyStringNew(0);
const struct vcfFile *vcff = tg->extraUiData;
struct vcfRecord *rec;
for (rec = vcff->records;  rec != NULL;  rec = rec->next)
    {
    vcfParseGenotypes(rec);
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
    int gtIx;
    for (gtIx = 0;  gtIx < vcff->genotypeCount;  gtIx++)
	{
	struct vcfGenotype *gt = &(rec->genotypes[gtOrder[gtIx]]);
	y = drawOneGenotype(gt, rec->ref, altAlleles, altCount,
			    hvg, x1, y, w, itemHeight, lineHeight);
	}
    //#*** TODO: pgSnp-like mouseover text?
    mapBoxHgcOrHgGene(hvg, rec->chromStart, rec->chromEnd, x1, yOff, w, tg->height, tg->track,
		      rec->name, rec->name, FALSE, TRUE, NULL);
    }
if (vis == tvPack && withLeftLabels)
    {
    int y = yOff, ploidy = 2, gtIx;
    for (gtIx = 0;  gtIx < vcff->genotypeCount;  gtIx++)
	{
	hvGfxTextRight(hvg, leftLabelX, y, leftLabelWidth-1, itemHeight*ploidy,
		       color, font, vcff->genotypeIds[gtOrder[gtIx]]);
	y += lineHeight * ploidy;
	}
    }
}

/* For qsorting genotypes-in-window: */
struct gtIxAndStr
    {
    int ix;			// index into vcfRecord's array of vcfGenotypes
    struct dyString *dy;	// Concatenation of all genotypes in window
    };

static void gtIxAndStrInit(struct gtIxAndStr *gt, int ix)
/* Initialize the already-allocated gt. */
{
gt->ix = ix;
gt->dy = dyStringNew(0);
}

static int gtIxAndStrCmp(const void *el1, const void *el2)
/* Comparator for qsort */
{
const struct gtIxAndStr *gt1 = el1;
const struct gtIxAndStr *gt2 = el2;
int diff = strcmp(gt1->dy->string, gt2->dy->string);
if (diff == 0)
    diff = gt1->ix - gt2->ix;
return diff;
}

static void vcfHapGraphDraw(struct track *tg, int seqStart, int seqEnd,
			    struct hvGfx *hvg, int xOff, int yOff, int width,
			    MgFont *font, Color color, enum trackVisibility vis)
/* Draw per-individual haplotypes (maybe phased, maybe not) from a VCF
 * file that contains genotypes.  Sort samples by genotypes-in-view if specified. */
{
const struct vcfFile *vcff = tg->extraUiData;
const int gtCount = vcff->genotypeCount;
int *gtOrder, i;
AllocArray(gtOrder, gtCount);
if (sortByGenotype)
    {
    // Step through records to build up per-sample arrays of genotypes-in-window:
    struct dyString *tmp = dyStringNew(0);
    struct gtIxAndStr *gtIxStrs;
    AllocArray(gtIxStrs, gtCount);
    for (i = 0;  i < gtCount;  i++)
	gtIxAndStrInit(&(gtIxStrs[i]), i);
    struct vcfRecord *rec;
    for (rec = vcff->records;  rec != NULL;  rec = rec->next)
	{
	vcfParseGenotypes(rec);
	dyStringClear(tmp);
	dyStringAppend(tmp, rec->alt);
	char *altAlleles[256];
	(void)chopCommas(tmp->string, altAlleles);
	for (i = 0;  i < gtCount;  i++)
	    {
	    struct vcfGenotype *gt = &(rec->genotypes[i]);
	    dyStringPrintf(gtIxStrs[i].dy, "%s/%s,",
			   hapIxToAllele(gt->hapIxA, rec->ref, altAlleles),
			   hapIxToAllele(gt->hapIxB, rec->ref, altAlleles));
	    }
	}
    // Sort samples by genotypes-in-window --> ordering of genotype Ids
    qsort(gtIxStrs, gtCount, sizeof(gtIxStrs[0]), gtIxAndStrCmp);
    for (i = 0;  i < gtCount;  i++)
	gtOrder[i] = gtIxStrs[i].ix;
    }
else
    {
    // Draw genotypes in order of appearance in VCF file:
    for (i = 0;  i < gtCount;  i++)
	gtOrder[i] = i;
    }
vcfHapGraphDrawGtOrdered(tg, gtOrder, hvg, xOff, yOff, width, font, color, vis);
}

static int vcfHapGraphTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return height of haplotype graph (2 * #samples * lineHeight);
 * 2 because we're assuming diploid genomes here, no XXY, tetraploid etc. */
{
// Should we make it single-height when on chrY?
const struct vcfFile *vcff = tg->extraUiData;
int ploidy = 2;
tg->height = ploidy * vcff->genotypeCount * tg->lineHeight;
return tg->height;
}

static char *vcfHapGraphTrackName(struct track *tg, void *item)
/* If someone asks for itemName/mapItemName, just send name of track like wiggle. */
{
return tg->track;
}

static void vcfHapGraphOverloadMethods(struct track *tg, struct vcfFile *vcff)
/* If we confirm at load time that we can draw a haplotype graph, use
 * this to overwrite the methods for the rest of execution: */
{
tg->heightPer = (tg->visibility == tvSquish) ? (tl.fontHeight/4) : (tl.fontHeight / 2);
tg->lineHeight = tg->heightPer + 1;
tg->drawItems = vcfHapGraphDraw;
tg->totalHeight = vcfHapGraphTotalHeight;
tg->itemHeight = tgFixedItemHeight;
tg->itemName = vcfHapGraphTrackName;
tg->mapItemName = vcfHapGraphTrackName;
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
    if (boringBed)
	tg->items = vcfFileToBed4(vcff);
    else if (doHapGraphDisplay && vcff->genotypeCount > 0 &&
	     (tg->visibility == tvPack || tg->visibility == tvSquish))
	vcfHapGraphOverloadMethods(tg, vcff);
    else
	{
	tg->items = vcfFileToPgSnp(vcff);
	/* base coloring/display decision on count of items */
	tg->customInt = slCount(tg->items);
	}
    // Don't vcfFileFree here -- we are using its string pointers!
    }
}

void vcfTabixMethods(struct track *track)
/* Methods for VCF + tabix files. */
{
if (boringBed == TRUE)
    bedMethods(track);
else
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
if (udcCacheTimeout() < 300)
    udcSetCacheTimeout(300);
#endif//def USE_TABIX && KNETFILE_HOOKS
messageLineMethods(track);
track->drawItems = drawUseVcfTabixWarning;
}

#endif // no USE_TABIX
