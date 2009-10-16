/* bamTrack -- handlers for alignments in BAM format (produced by MAQ,
 * BWA and some other short-read alignment tools). */

#ifdef USE_BAM

#include "common.h"
#include "hCommon.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "cds.h"
#include "bamFile.h"

static char const rcsid[] = "$Id: bamTrack.c,v 1.11 2009/10/16 00:34:31 angie Exp $";

struct bamTrackData
    {
    struct track *tg;
    struct hash *pairHash;
    int minAliQual;
    char *colorMode;
    char *grayMode;
    boolean grayUnpaired;
    char *userTag;
    };

struct simpleFeature *sfFromNumericCigar(const bam1_t *bam, int *retLength)
/* Translate BAM's numeric CIGAR encoding into a list of simpleFeatures, 
 * and tally up length on reference sequence while we're at it. */
{
const bam1_core_t *core = &bam->core;
struct simpleFeature *sf, *sfList = NULL;
int tLength=0, tPos = core->pos, qPos = 0;
unsigned int *cigar = bam1_cigar(bam);
int i;
for (i = 0;  i < core->n_cigar;  i++)
    {
    char op;
    int n = bamUnpackCigarElement(cigar[i], &op);
    switch (op)
	{
	case 'M': // match or mismatch (gapless aligned block)
	    AllocVar(sf);
	    sf->start = tPos;
	    sf->qStart = qPos;
	    tPos = sf->end = tPos + n;
	    qPos = sf->qEnd = qPos + n;
	    slAddHead(&sfList, sf);
	    tLength += n;
	    break;
	case 'I': // inserted in query
	case 'S': // skipped query bases at beginning or end ("soft clipping")
	    qPos += n;
	    break;
	case 'D': // deleted from query
	case 'N': // long deletion from query (intron as opposed to small del)
	    tPos += n;
	    tLength += n;
	    break;
	case 'H': // skipped query bases not stored in record's query sequence ("hard clipping")
	case 'P': // P="silent deletion from padded reference sequence" -- ignore these.
	    break;
	default:
	    errAbort("sfFromNumericCigar: unrecognized CIGAR op %c -- update me", op);
	}
    }
if (retLength != NULL)
    *retLength = tLength;
return sfList;
}

struct linkedFeatures *bamToLf(const bam1_t *bam, void *data)
/* Translate a BAM record into a linkedFeatures item. */
{
struct bamTrackData *btd = (struct bamTrackData *)data;
const bam1_core_t *core = &bam->core;
struct linkedFeatures *lf;
AllocVar(lf);
lf->score = core->qual;
safef(lf->name, sizeof(lf->name), bam1_qname(bam));
lf->orientation = (core->flag & BAM_FREVERSE) ? -1 : 1;
int length;
lf->components = sfFromNumericCigar(bam, &length);
lf->start = lf->tallStart = core->pos;
lf->end = lf->tallEnd = core->pos + length;
lf->extra = bamGetQuerySequence(bam);
if (sameString(btd->colorMode, BAM_COLOR_MODE_GRAY) &&
    sameString(btd->grayMode, BAM_GRAY_MODE_ALI_QUAL))
    {
    // hardcode transform coefficients for now; if user demand, make into tdb/cart vars.
    int min = 0, max = 99;
    int minShade = 2;
    int ix = minShade + (maxShade - minShade) * ((double)(core->qual - min) / (double)(max-min));
    if (ix > maxShade)
	ix = maxShade;
    if (ix < minShade)
	ix = minShade;
    lf->grayIx = ix;
    }
else
    lf->grayIx = maxShade;
return lf;
}

boolean passesFilters(const bam1_t *bam, struct bamTrackData *btd)
/* Return TRUE if bam passes hgTrackUi-set filters. */
{
if (bam == NULL)
    return FALSE;
const bam1_core_t *core = &bam->core;
// Always reject unmapped items -- nowhere to draw them.
if (core->flag & BAM_FUNMAP)
    return FALSE;
if (core->qual < btd->minAliQual)
    return FALSE;
return TRUE;
}

int addBam(const bam1_t *bam, void *data)
/* bam_fetch() calls this on each bam alignment retrieved.  Translate each bam 
 * into a linkedFeatures item, and add it to tg->items. */
{
struct bamTrackData *btd = (struct bamTrackData *)data;
if (!passesFilters(bam, btd))
    return 0;
struct linkedFeatures *lf = bamToLf(bam, data);
struct track *tg = btd->tg;
slAddHead(&(tg->items), lf);
return 0;
}

static struct linkedFeatures *lfStub(int startEnd, int orientation)
/* Make a linkedFeatures for a zero-length item (so that an arrow will be drawn
 * toward the given coord. */
{
struct linkedFeatures *lf;
AllocVar(lf);
safef(lf->name, sizeof(lf->name), "stub");
lf->orientation = orientation;
struct simpleFeature *sf;
AllocVar(sf);
sf->start = sf->end = lf->start = lf->end = lf->tallStart = lf->tallEnd = startEnd;
lf->components = sf;
lf->extra = cloneString("");
lf->grayIx = maxShade;
return lf;
}

static struct linkedFeaturesSeries *lfsFromLf(struct linkedFeatures *lf)
/* Make a linkedFeaturesSeries from one or two linkedFeatures elements. */
{
struct linkedFeaturesSeries *lfs;
AllocVar(lfs);
lfs->name = cloneString(lf->name);
lfs->grayIx = lf->grayIx;
if (lf->next != NULL)
    slSort(&lf, linkedFeaturesCmpStart);
lfs->orientation = 0;
lfs->start = lf->start;
lfs->end = lf->next ? lf->next->end : lf->end;
lfs->features = lf;
return lfs;
}

int addBamPaired(const bam1_t *bam, void *data)
/* bam_fetch() calls this on each bam alignment retrieved.  Translate each bam 
 * into a linkedFeaturesSeries item, and either store it until we find its mate
 * or add it to tg->items. */
{
const bam1_core_t *core = &bam->core;
struct bamTrackData *btd = (struct bamTrackData *)data;
if (! passesFilters(bam, btd))
    return 0;
struct linkedFeatures *lf = bamToLf(bam, data);
struct track *tg = btd->tg;
if (! (core->flag & BAM_FPAIRED))
    {
    slAddHead(&(tg->items), lfsFromLf(lf));
    }
else
    {
    struct linkedFeatures *lfMate = (struct linkedFeatures *)hashFindVal(btd->pairHash, lf->name);
    if (lfMate == NULL)
	{
	if (core->flag & BAM_FPROPER_PAIR)
	    {
	    // If we know that this is properly paired, but don't have the mate,
	    // make a bogus item off the edge of the window so that if we don't
	    // encounter its mate later, we can at least draw an arrow off the
	    // edge of the window.
	    struct linkedFeatures *stub;
	    if (core->mpos < 0)
		{
		int offscreen = (lf->orientation > 0) ? winEnd + 10 : winStart - 10;
		if (offscreen < 0) offscreen = 0;
		stub = lfStub(offscreen, -lf->orientation);
		}
	    else
		{
		stub = lfStub(core->mpos, -lf->orientation);
		}
	    lf->next = stub;
	    }
	else if (btd->grayUnpaired)
	    // not properly paired: make it a lighter shade.
	    lf->grayIx -= 3;
	hashAdd(btd->pairHash, lf->name, lf);
	}
    else
	{
	lfMate->next = lf;
	slAddHead(&(tg->items), lfsFromLf(lfMate));
	hashRemove(btd->pairHash, lf->name);
	}
    }
return 0;
}

#define MAX_ITEMS_FOR_MAPBOX 1500
static void dontMapItem(struct track *tg, struct hvGfx *hvg, void *item,
			char *itemName, char *mapItemName, int start, int end,
			int x, int y, int width, int height)
/* When there are many many items, drawing hgc links can really slow us down. */
{
}

static int linkedFeaturesCmpOri(const void *va, const void *vb)
/* Help sort linkedFeatures by strand, then by starting pos. */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
int ret = a->orientation - b->orientation;
if (ret == 0)
    ret = a->start - b->start;
return ret;
}

void bamLoadItemsCore(struct track *tg, boolean isPaired)
/* Load BAM data into tg->items item list, unless zoomed out so far
 * that the data would just end up in dense mode and be super-slow. */
{
char *seqNameForBam = chromName;
char *stripPrefix = trackDbSetting(tg->tdb, "stripPrefix");
if (stripPrefix && startsWith(stripPrefix, chromName))
    seqNameForBam = chromName + strlen(stripPrefix);
char posForBam[512];
safef(posForBam, sizeof(posForBam), "%s:%d-%d", seqNameForBam, winStart, winEnd);

struct hash *pairHash = isPaired ? hashNew(18) : NULL;
char cartVarName[1024];
safef(cartVarName, sizeof(cartVarName), "%s_" BAM_MIN_ALI_QUAL, tg->tdb->tableName);
int minAliQual = cartUsualInt(cart, cartVarName,
	       atoi(trackDbSettingOrDefault(tg->tdb, BAM_MIN_ALI_QUAL, BAM_MIN_ALI_QUAL_DEFAULT)));
safef(cartVarName, sizeof(cartVarName), "%s_" BAM_COLOR_MODE, tg->tdb->tableName);
char *colorMode = cartUsualString(cart, cartVarName,
		         trackDbSettingOrDefault(tg->tdb, BAM_COLOR_MODE, BAM_COLOR_MODE_DEFAULT));
safef(cartVarName, sizeof(cartVarName), "%s_" BAM_GRAY_MODE, tg->tdb->tableName);
char *grayMode = cartUsualString(cart, cartVarName,
		         trackDbSettingOrDefault(tg->tdb, BAM_GRAY_MODE, BAM_GRAY_MODE_DEFAULT));
boolean grayUnpaired = (sameString(colorMode, BAM_COLOR_MODE_GRAY) &&
			sameString(grayMode, BAM_GRAY_MODE_UNPAIRED));
safef(cartVarName, sizeof(cartVarName), "%s_" BAM_COLOR_TAG, tg->tdb->tableName);
char *userTag = cartUsualString(cart, cartVarName,
		         trackDbSettingOrDefault(tg->tdb, BAM_COLOR_TAG, BAM_COLOR_TAG_DEFAULT));
struct bamTrackData btd = {tg, pairHash, minAliQual, colorMode, grayMode, grayUnpaired, userTag};
char *fileName;
if (tg->customPt)
    {
    fileName = trackDbSetting(tg->tdb, "bigDataUrl");
    if (fileName == NULL)
	errAbort("bamLoadItemsCore: can't find bigDataUrl for custom track %s", tg->mapName);
    }
else
    fileName = bamFileNameFromTable(database, tg->mapName, seqNameForBam);
bamFetch(fileName, posForBam, (isPaired ? addBamPaired : addBam), &btd);
if (isPaired)
    {
    struct hashEl *hel;
    struct hashCookie cookie = hashFirst(pairHash);
    int count = 0;
    while ((hel = hashNext(&cookie)) != NULL)
	{
	struct linkedFeatures *lf = hel->val;
	slAddHead(&(tg->items), lfsFromLf(lf));
	count++;
	}
    }
if (tg->visibility != tvDense)
    {
    slReverse(&(tg->items));
    if (isPaired)
	slSort(&(tg->items), linkedFeaturesSeriesCmp);
    else if (sameString(colorMode, BAM_COLOR_MODE_STRAND))
	slSort(&(tg->items), linkedFeaturesCmpOri);
    else
	slSort(&(tg->items), linkedFeaturesCmpStart);
    if (slCount(tg->items) > MAX_ITEMS_FOR_MAPBOX)
	tg->mapItem = dontMapItem;
    }
}

void bamLoadItems(struct track *tg)
/* Load single-ended-only BAM data into tg->items item list, unless zoomed out so far
 * that the data would just end up in dense mode and be super-slow. */
{
bamLoadItemsCore(tg, FALSE);
}

void bamPairedLoadItems(struct track *tg)
/* Load possibly paired BAM data into tg->items item list, unless zoomed out so far
 * that the data would just end up in dense mode and be super-slow. */
{
bamLoadItemsCore(tg, TRUE);
}

void bamDrawAt(struct track *tg, void *item,
	struct hvGfx *hvg, int xOff, int y, double scale,
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single bam linkedFeatures item.  Borrows a lot from linkedFeaturesDrawAt,
 * but cuts a lot of unneeded features (like coding region) and adds a couple
 * additional sources of color. */
{
struct linkedFeatures *lf = item;
struct simpleFeature *sf;
int heightPer = tg->heightPer;
int x1 = round((double)((int)lf->start-winStart)*scale) + xOff;
int x2 = round((double)((int)lf->end-winStart)*scale) + xOff;
int w = x2-x1;
int midY = y + (heightPer>>1);
char *exonArrowsDense = trackDbSetting(tg->tdb, "exonArrowsDense");
boolean exonArrowsEvenWhenDense = (exonArrowsDense != NULL &&
				   !sameWord(exonArrowsDense, "off"));
boolean exonArrows = (tg->exonArrows &&
		      (vis != tvDense || exonArrowsEvenWhenDense));
struct dnaSeq *mrnaSeq = NULL;
enum baseColorDrawOpt drawOpt = baseColorDrawOff;
boolean indelShowDoubleInsert, indelShowQueryInsert, indelShowPolyA;
struct psl *psl = NULL;
if (vis != tvDense)
    {
    drawOpt = baseColorDrawSetup(hvg, tg, lf, &mrnaSeq, &psl);
    if (drawOpt > baseColorDrawOff)
	exonArrows = FALSE;
    }

char cartVarName[1024];
safef(cartVarName, sizeof(cartVarName), "%s_" BAM_COLOR_MODE, tg->tdb->tableName);
char *colorMode = cartUsualString(cart, cartVarName,
		         trackDbSettingOrDefault(tg->tdb, BAM_COLOR_MODE, BAM_COLOR_MODE_DEFAULT));
static Color darkBlueColor = 0;
static Color darkRedColor = 0;
if (darkRedColor == 0)
    {
    darkRedColor = hvGfxFindColorIx(hvg, 100,0,0);
    darkBlueColor = hvGfxFindColorIx(hvg, 0,0,100);
    }
if (sameString(colorMode, BAM_COLOR_MODE_STRAND))
    color = (lf->orientation < 0) ? darkRedColor : darkBlueColor;
else if (lf->filterColor > 0)
    color = lf->filterColor;
else if (tg->colorShades)
    color = tg->colorShades[lf->grayIx];


indelEnabled(cart, tg->tdb, basesPerPixel, &indelShowDoubleInsert, &indelShowQueryInsert,
	     &indelShowPolyA);
if (!indelShowDoubleInsert)
    innerLine(hvg, x1, midY, w, color);
for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    int s = sf->start,  e = sf->end;
    baseColorDrawItem(tg, lf, sf->grayIx, hvg, xOff, y, scale, font, s, e, heightPer,
		      zoomedToCodonLevel, mrnaSeq, sf, psl, drawOpt, MAXPIXELS, winStart, color);
    if (tg->exonArrowsAlways ||
	(exonArrows &&
	 (sf->start <= winStart || sf->start == lf->start) &&
	 (sf->end >= winEnd || sf->end == lf->end)))
	{
	Color barbColor = hvGfxContrastingColor(hvg, color);
	x1 = round((double)((int)s-winStart)*scale) + xOff;
	x2 = round((double)((int)e-winStart)*scale) + xOff;
	w = x2-x1;
	clippedBarbs(hvg, x1+1, midY, x2-x1-2, tl.barbHeight, tl.barbSpacing, lf->orientation,
		     barbColor, TRUE);
	}
    }
if (indelShowDoubleInsert)
    {
    int intronGap = 0;
    if (vis != tvDense)
	intronGap = atoi(trackDbSettingOrDefault(tg->tdb, "intronGap", "0"));
    lfDrawSpecialGaps(lf, intronGap, TRUE, 0, tg, hvg, xOff, y, scale, color, color, vis);
    }
if (vis != tvDense)
    {
    /* If highlighting differences between aligned sequence and genome when
     * zoomed way out, this must be done in a separate pass after exons are
     * drawn so that exons sharing the pixel don't overdraw differences. */
    if (indelShowQueryInsert || indelShowPolyA)
	baseColorOverdrawQInsert(tg, lf, hvg, xOff, y, scale, heightPer, mrnaSeq, psl, winStart,
				 drawOpt, indelShowQueryInsert, indelShowPolyA);
    baseColorOverdrawDiff(tg, lf, hvg, xOff, y, scale, heightPer, mrnaSeq, psl, winStart, drawOpt);
    baseColorDrawCleanup(lf, &mrnaSeq, &psl);
    }
}

void bamPairedDrawAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y,
		     double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a bam linked features series item at position. (like linkedFeaturesSeriesDrawAt,
 * but calls bamDrawAt instead of linkedFeaturesDrawAt) */
{
struct linkedFeaturesSeries *lfs = item;
struct linkedFeatures *lf;
int midY = y + (tg->heightPer>>1);
int prevEnd = lfs->start;

if ((lf = lfs->features) == NULL)
    return;
for (lf = lfs->features; lf != NULL; lf = lf->next)
    {

    int x1 = round((double)((int)prevEnd-winStart)*scale) + xOff;
    int x2 = round((double)((int)lf->start-winStart)*scale) + xOff;
    int w = x2-x1;
    if (w > 0)
	hvGfxLine(hvg, x1, midY, x2, midY, color);
    bamDrawAt(tg, lf, hvg, xOff, y, scale, font, color, vis);
    prevEnd = lf->end;
    }
}


void bamMethods(struct track *track)
/* Methods for BAM alignment files. */
{
track->canPack = TRUE;
char varName[1024];
safef(varName, sizeof(varName), "%s_" BAM_PAIR_ENDS_BY_NAME, track->mapName);
boolean isPaired = cartUsualBoolean(cart, varName,
				    (trackDbSetting(track->tdb, BAM_PAIR_ENDS_BY_NAME) != NULL));
if (isPaired)
    {
    linkedFeaturesSeriesMethods(track);
    track->loadItems = bamPairedLoadItems;
    track->drawItemAt = bamPairedDrawAt;
    }
else
    {
    linkedFeaturesMethods(track);
    track->loadItems = bamLoadItems;
    track->drawItemAt = bamDrawAt;
    }
track->labelNextItemButtonable = track->nextItemButtonable = FALSE;
track->labelNextPrevItem = NULL;
track->nextPrevItem = NULL;
track->colorShades = shadesOfGray;
}

#endif /* USE_BAM */
