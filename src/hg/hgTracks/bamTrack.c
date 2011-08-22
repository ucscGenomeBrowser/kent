/* bamTrack -- handlers for alignments in BAM format (produced by MAQ,
 * BWA and some other short-read alignment tools). */

#ifdef USE_BAM

#include "common.h"
#include "hCommon.h"
#include "hash.h"
#include "linefile.h"
#include "htmshell.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "cds.h"
#include "hgBam.h"
#include "wigCommon.h"
#if (defined USE_BAM && defined KNETFILE_HOOKS)
#include "knetUdc.h"
#include "udc.h"
#endif//def USE_BAM && KNETFILE_HOOKS
#include "bigWarn.h"
#include "errCatch.h"

static char const rcsid[] = "$Id: bamTrack.c,v 1.32 2010/05/27 21:13:24 angie Exp $";

struct bamTrackData
    {
    struct track *tg;
    struct hash *pairHash;
    int minAliQual;
    char *colorMode;
    char *grayMode;
    char *userTag;
    int aliQualShadeMin;
    int aliQualShadeMax;
    int baseQualShadeMin;
    int baseQualShadeMax;
    };


struct psl *pslFromBam(const bam1_t *bam)
/* Translate BAM's numeric CIGAR encoding into PSL sufficient for cds.c (just coords,
 * no scoring info) */
{
const bam1_core_t *core = &bam->core;
struct psl *psl;
AllocVar(psl);
boolean isRc = (core->flag & BAM_FREVERSE);
psl->strand[0] = isRc ? '-' : '+';
psl->qName = cloneString(bam1_qname(bam));
psl->tName = cloneString(chromName);
unsigned blockCount = 0;
unsigned *blockSizes, *qStarts, *tStarts;
AllocArray(blockSizes, core->n_cigar);
AllocArray(qStarts, core->n_cigar);
AllocArray(tStarts, core->n_cigar);
int tPos = core->pos, qPos = 0, qLength = 0;
unsigned int *cigar = bam1_cigar(bam);
int i;
for (i = 0;  i < core->n_cigar;  i++)
    {
    char op;
    int n = bamUnpackCigarElement(cigar[i], &op);
    switch (op)
	{
	case 'M': // match or mismatch (gapless aligned block)
	    blockSizes[blockCount] = n;
	    qStarts[blockCount] = qPos;
	    tStarts[blockCount] = tPos;
	    blockCount++;
	    tPos += n;
	    qPos += n;
	    qLength += n;
	    break;
	case 'I': // inserted in query
	    qPos += n;
	    qLength += n;
	    break;
	case 'D': // deleted from query
	case 'N': // long deletion from query (intron as opposed to small del)
	    tPos += n;
	    break;
	case 'S': // skipped query bases at beginning or end ("soft clipping")
	    qPos += n;
	    qLength += n;
	    break;
	case 'H': // skipped query bases not stored in record's query sequence ("hard clipping")
	case 'P': // P="silent deletion from padded reference sequence" -- ignore these.
	    break;
	default:
	    errAbort("pslFromBam: unrecognized CIGAR op %c -- update me", op);
	}
    }
psl->tSize = hChromSize(database, chromName);
psl->tStart = tStarts[0];
psl->tEnd = tStarts[blockCount-1] + blockSizes[blockCount-1];
psl->qSize = qLength;
psl->qStart = qStarts[0];
psl->qEnd = qStarts[blockCount-1] + blockSizes[blockCount-1];
if (isRc)
    reverseIntRange(&psl->qStart, &psl->qEnd, psl->qSize);
psl->blockCount = blockCount;
psl->blockSizes = blockSizes;
psl->qStarts = qStarts;
psl->tStarts = tStarts;
return psl;
}

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
	    qPos += n;
	    break;
	case 'D': // deleted from query
	case 'N': // long deletion from query (intron as opposed to small del)
	    tPos += n;
	    tLength += n;
	    break;
	case 'S': // skipped query bases at beginning or end ("soft clipping")
	case 'H': // skipped query bases not stored in record's query sequence ("hard clipping")
	case 'P': // P="silent deletion from padded reference sequence" -- ignore these.
	    break;
	default:
	    errAbort("sfFromNumericCigar: unrecognized CIGAR op %c -- update me", op);
	}
    }
if (retLength != NULL)
    *retLength = tLength;
slReverse(&sfList);
return sfList;
}

INLINE int shadeTransform(int min, int max, int score)
/* Linearly transform score's place between min and max into a visible shade. */
{
int minShade = 2;
int ix = minShade + (maxShade - minShade) * ((double)(score - min) / (double)(max-min));
if (ix > maxShade)
    ix = maxShade;
if (ix < minShade)
    ix = minShade;
return ix;
}

static struct simpleFeature *expandSfQuals(struct simpleFeature *blocksIn, UBYTE *quals,
					   int orientation, int qLen, int minQual, int maxQual)
/* Chop up blocksIn into one sf per query base, with sf->grayIx set according to
 * base quality score. */
{
struct simpleFeature *sf, *blocksOut = NULL, *tail = NULL;
for (sf = blocksIn;  sf != NULL;  sf = sf->next)
    {
    struct simpleFeature *newSf;
    int i;
    for (i = sf->qStart;  i < sf->qEnd;  i++)
	{
	AllocVar(newSf);
	newSf->start = sf->start + (i - sf->qStart);
	newSf->end = newSf->start + 1;
	newSf->qStart = i;
	newSf->qEnd = i + 1;
	int offset = (orientation < 0) ? (qLen - i - 1) : i;
	newSf->grayIx = shadeTransform(minQual, maxQual, quals[offset]);
	if (blocksOut == NULL)
	    blocksOut = tail = newSf;
	else
	    {
	    tail->next = newSf;
	    tail = tail->next;
	    }
	}
    }
return blocksOut;
}

// similar but not identical to customFactory.c's parseRgb:
static boolean parseRgb(char *s, unsigned char *retR, unsigned char *retG, unsigned char *retB)
/* Turn comma separated list to RGB vals; return FALSE if input is not as expected. */
{
char *row[4];
int wordCount = chopString(s, ",", row, ArraySize(row));
if ((wordCount != 3) || (!isdigit(row[0][0]) || !isdigit(row[1][0]) || !isdigit(row[2][0])))
    return FALSE;
*retR = atoi(row[0]);
*retG = atoi(row[1]);
*retB = atoi(row[2]);
return TRUE;
}

struct linkedFeatures *bamToLf(const bam1_t *bam, void *data)
/* Translate a BAM record into a linkedFeatures item. */
{
struct bamTrackData *btd = (struct bamTrackData *)data;
const bam1_core_t *core = &bam->core;
struct linkedFeatures *lf;
AllocVar(lf);
lf->score = core->qual;
lf->name = cloneString(bam1_qname(bam));
lf->orientation = (core->flag & BAM_FREVERSE) ? -1 : 1;
int length;
lf->components = sfFromNumericCigar(bam, &length);
lf->start = lf->tallStart = core->pos;
lf->end = lf->tallEnd = core->pos + length;
lf->extra = bamGetQuerySequence(bam, FALSE); // cds.c reverses if psl != NULL
lf->original = pslFromBam(bam);
int clippedQLen;
bamGetSoftClipping(bam, NULL, NULL, &clippedQLen);
if (sameString(btd->colorMode, BAM_COLOR_MODE_GRAY) &&
    sameString(btd->grayMode, BAM_GRAY_MODE_ALI_QUAL))
    {
    lf->grayIx = shadeTransform(btd->aliQualShadeMin, btd->aliQualShadeMax, core->qual);
    }
else if (sameString(btd->colorMode, BAM_COLOR_MODE_GRAY) &&
	 sameString(btd->grayMode, BAM_GRAY_MODE_BASE_QUAL))
    {
    UBYTE *quals = bamGetQueryQuals(bam, TRUE);
    lf->components = expandSfQuals(lf->components, quals, lf->orientation, clippedQLen,
				   btd->baseQualShadeMin, btd->baseQualShadeMax);
    lf->grayIx = maxShade - 3;
    }
else if (sameString(btd->colorMode, BAM_COLOR_MODE_TAG) && isNotEmpty(btd->userTag))
    {
    char buf[16];
    char *rgb = bamGetTagString(bam, btd->userTag, buf, sizeof(buf));
    if (rgb != NULL)
	{
	// We don't have access to hvg at loadtime, so can't allocate color here.
	// Instead, pack RGB values into lf->filterColor which fortunately is an int.
	unsigned char r, g, b;
	if (parseRgb(rgb, &r, &g, &b))
	    lf->filterColor = ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff);
	else
	    {
	    static boolean already = FALSE;
	    if (! already)
		{
		warn("%s: At least one BAM tag value for %s (%s) is not in the expected "
		     "RGB format: N,N,N where each N is from 0 to 255.",
		     btd->tg->tdb->shortLabel, btd->userTag, htmlEncode(rgb));
		already = TRUE;
		btd->userTag = NULL;
		}
	    }
	}
    else
	lf->grayIx = maxShade;
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
lf->name = cloneString("stub");
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
lfs->end = lf->next ? max(lf->next->end, lf->end) : lf->end;
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
if (!(core->flag & BAM_FPAIRED) || (core->flag & BAM_FMUNMAP))
    {
    if (lf->start < winEnd && lf->end > winStart)
	slAddHead(&(tg->items), lfsFromLf(lf));
    if ((core->flag & BAM_FMUNMAP) && sameString(btd->colorMode, BAM_COLOR_MODE_GRAY) &&
	sameString(btd->grayMode, BAM_GRAY_MODE_UNPAIRED))
	// not properly paired: make it a lighter shade.
	lf->grayIx -= 4;
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
		int offscreen;
		if (lf->orientation > 0)
		    offscreen = max(winEnd, lf->end) + 10;
		else
		    offscreen = min(winStart, lf->start) - 10;
		if (offscreen < 0) offscreen = 0;
		stub = lfStub(offscreen, -lf->orientation);
		}
	    else
		{
		stub = lfStub(core->mpos, -lf->orientation);
		}
	    lf->next = stub;
	    }
	else if (sameString(btd->colorMode, BAM_COLOR_MODE_GRAY) &&
		 sameString(btd->grayMode, BAM_GRAY_MODE_UNPAIRED))
	    // not properly paired: make it a lighter shade.
	    lf->grayIx -= 4;
	hashAdd(btd->pairHash, lf->name, lf);
	}
    else
	{
	lfMate->next = lf;
	if (min(lfMate->start, lf->start) < winEnd && max(lfMate->end, lf->end) > winStart)
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

static int linkedFeaturesCmpScore(const void *va, const void *vb)
/* Help sort linkedFeatures by score (descending), then by starting pos. */
{
const struct linkedFeatures *a = *((struct linkedFeatures **)va);
const struct linkedFeatures *b = *((struct linkedFeatures **)vb);
int ret = b->score - a->score;
if (ret == 0)
    ret = a->start - b->start;
return ret;
}

static void parseIntRangeSetting(struct trackDb *tdb, char *settingName,
				    int *retMin, int *retMax)
/* If setting is an integer range, store the values into retMin and retMax */
// This is highly similar to lib/wiggleCart.c's static void parseColonRange,
// though that expects doubles.  For quality scores we do want unsigned.
{
char *range = trackDbSetting(tdb, settingName);
if (range != NULL)
    {
    char rangeCopy[16]; // if it's longer than this, there's a problem.
    safecpy(rangeCopy, sizeof(rangeCopy), range);
    char *words[3];
    int wordCount = chopByChar(rangeCopy, ':', words, ArraySize(words));
    if (wordCount == 2)
	{
	if (retMin != NULL)
	    *retMin = sqlUnsigned(words[0]);
	if (retMax != NULL)
	    *retMax = sqlUnsigned(words[1]);
	return;
	}
    else
	warn("track %s table %s: setting %s should be integer range min:max but is %s",
	     tdb->track, tdb->table, settingName, range);
    }
}

void bamLoadItemsCore(struct track *tg, boolean isPaired)
/* Load BAM data into tg->items item list, unless zoomed out so far
 * that the data would just end up in dense mode and be super-slow. */
{
/* protect against temporary network error */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    struct hash *pairHash = isPaired ? hashNew(18) : NULL;
    int minAliQual = atoi(cartOrTdbString(cart, tg->tdb, BAM_MIN_ALI_QUAL, BAM_MIN_ALI_QUAL_DEFAULT));
    char *colorMode = cartOrTdbString(cart, tg->tdb, BAM_COLOR_MODE, BAM_COLOR_MODE_DEFAULT);
    char *grayMode = cartOrTdbString(cart, tg->tdb, BAM_GRAY_MODE, BAM_GRAY_MODE_DEFAULT);
    char *userTag = cartOrTdbString(cart, tg->tdb, BAM_COLOR_TAG, BAM_COLOR_TAG_DEFAULT);
    int aliQualShadeMin = 0, aliQualShadeMax = 99, baseQualShadeMin = 0, baseQualShadeMax = 40;
    parseIntRangeSetting(tg->tdb, "aliQualRange", &aliQualShadeMin, &aliQualShadeMax);
    parseIntRangeSetting(tg->tdb, "baseQualRange", &baseQualShadeMin, &baseQualShadeMax);
    struct bamTrackData btd = {tg, pairHash, minAliQual, colorMode, grayMode, userTag,
			       aliQualShadeMin, aliQualShadeMax, baseQualShadeMin, baseQualShadeMax};
    char *fileName = trackDbSetting(tg->tdb, "bigDataUrl");
    if (fileName == NULL)
	{
	if (tg->customPt)
	    {
	    errAbort("bamLoadItemsCore: can't find bigDataUrl for custom track %s", tg->track);
	    }
	else
	    {
	    struct sqlConnection *conn = hAllocConnTrack(database, tg->tdb);
	    fileName = bamFileNameFromTable(conn, tg->table, chromName);
	    hFreeConn(&conn);
	    }
	}

    char posForBam[512];
    safef(posForBam, sizeof(posForBam), "%s:%d-%d", chromName, winStart, winEnd);
    if (!isPaired)
	bamFetch(fileName, posForBam, addBam, &btd, NULL);
    else
	{
	char *setting = trackDbSettingClosestToHomeOrDefault(tg->tdb, "pairSearchRange", "20000");
	int pairSearchRange = atoi(setting);
	if (pairSearchRange > 0)
	    safef(posForBam, sizeof(posForBam), "%s:%d-%d", chromName,
		  max(0, winStart-pairSearchRange), winEnd+pairSearchRange);
	bamFetch(fileName, posForBam, addBamPaired, &btd, NULL);
	struct hashEl *hel;
	struct hashCookie cookie = hashFirst(btd.pairHash);
	while ((hel = hashNext(&cookie)) != NULL)
	    {
	    struct linkedFeatures *lf = hel->val;
	    if (lf->start < winEnd && lf->end > winStart)
		slAddHead(&(tg->items), lfsFromLf(lf));
	    }
	}
    if (tg->visibility != tvDense)
	{
	slReverse(&(tg->items));
	if (isPaired)
	    slSort(&(tg->items), linkedFeaturesSeriesCmp);
	else if (sameString(colorMode, BAM_COLOR_MODE_STRAND))
	    slSort(&(tg->items), linkedFeaturesCmpOri);
	else if (sameString(colorMode, BAM_COLOR_MODE_GRAY) &&
		 sameString(grayMode, BAM_GRAY_MODE_ALI_QUAL))
	    slSort(&(tg->items), linkedFeaturesCmpScore);
	else
	    slSort(&(tg->items), linkedFeaturesCmpStart);
	if (slCount(tg->items) > MAX_ITEMS_FOR_MAPBOX)
	    {
	    // flag drawItems to make a mapBox for the whole track
	    tg->customInt = 1;
	    tg->mapItem = dontMapItem;
	    }
	}
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    tg->networkErrMsg = cloneString(errCatch->message->string);
    tg->drawItems = bigDrawWarning;
    tg->totalHeight = bigWarnTotalHeight;
    }
errCatchFree(&errCatch);
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
char *exonArrowsDense = trackDbSettingClosestToHome(tg->tdb, "exonArrowsDense");
boolean exonArrowsEvenWhenDense = (exonArrowsDense != NULL &&
				   !sameWord(exonArrowsDense, "off"));
boolean exonArrows = (tg->exonArrows &&
		      (vis != tvDense || exonArrowsEvenWhenDense));
struct dnaSeq *mrnaSeq = NULL;
enum baseColorDrawOpt drawOpt = baseColorDrawOff;
boolean indelShowDoubleInsert, indelShowQueryInsert, indelShowPolyA;
struct psl *psl = (struct psl *)(lf->original);
char *colorMode = cartOrTdbString(cart, tg->tdb, BAM_COLOR_MODE, BAM_COLOR_MODE_DEFAULT);
char *grayMode = cartOrTdbString(cart, tg->tdb, BAM_GRAY_MODE, BAM_GRAY_MODE_DEFAULT);
bool baseQualMode = (sameString(colorMode, BAM_COLOR_MODE_GRAY) &&
		     sameString(grayMode, BAM_GRAY_MODE_BASE_QUAL));
char *qSeq = lf->extra;
if (vis != tvDense && isNotEmpty(qSeq) && !sameString(qSeq, "*"))
    {
    drawOpt = baseColorDrawSetup(hvg, tg, lf, &mrnaSeq, &psl);
    if (drawOpt > baseColorDrawOff)
	exonArrows = FALSE;
    }

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
    {
    // In bamTrack, lf->filterColor is a packed int.  Unpack:
    int r, g, b;
    r = (lf->filterColor >> 16) & 0xff;
    g = (lf->filterColor >> 8) & 0xff;
    b = lf->filterColor & 0xff;
    color = hvGfxFindColorIx(hvg, r, g, b);
    }
else if (tg->colorShades)
    color = tg->colorShades[lf->grayIx];
else
    color = tg->ixColor;

indelEnabled(cart, tg->tdb, basesPerPixel, &indelShowDoubleInsert, &indelShowQueryInsert,
	     &indelShowPolyA);
if (!indelShowDoubleInsert)
    innerLine(hvg, x1, midY, w, color);
for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    int s = sf->start,  e = sf->end;
    if (e <= s || e < winStart || s > winEnd)
	continue;
    if (baseQualMode)
	color = tg->colorShades[sf->grayIx];
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
if (indelShowDoubleInsert && psl)
    {
    int intronGap = 0;
    if (vis != tvDense)
	intronGap = atoi(trackDbSettingClosestToHomeOrDefault(tg->tdb, "intronGap", "0"));
    lfDrawSpecialGaps(lf, intronGap, TRUE, 0, tg, hvg, xOff, y, scale, color, color, vis);
    }
if (vis != tvDense)
    {
    /* If highlighting differences between aligned sequence and genome when
     * zoomed way out, this must be done in a separate pass after exons are
     * drawn so that exons sharing the pixel don't overdraw differences. */
    if ((indelShowQueryInsert || indelShowPolyA) && psl)
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

static void maybeDrawLeftLabels(struct track *tg, int seqStart, int seqEnd, struct hvGfx *hvg,
			       int xOff, int yOff, int width, int height, boolean withCenterLabels,
			       MgFont *font, Color color, enum trackVisibility vis)
/* If this is invoked, we are hiding item labels.  In full mode, that means no left labels.
 * In dense mode, we do need the left label to show shortLabel. */
{
if (tg->limitedVis == tvDense)
    {
    int fontHeight = mgFontLineHeight(font);
    if (isCenterLabelIncluded(tg))
	yOff += fontHeight;
    hvGfxTextRight(hvg, xOff, yOff, width, tg->lineHeight, color, font, tg->shortLabel);
    }
return;
}

static void doMapBoxPerRow(struct track *tg,
	int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis)
{
int fontHeight = mgFontLineHeight(font);
int numRows = tg->height / fontHeight;
struct customTrack *ct = tg->customPt;
char itemBuffer[1024];

if (ct)
    // this should have the trash file name instead of spacer
    // but the click handler doesn't use it anyway
    safef(itemBuffer, sizeof itemBuffer, "%s %s","spacer","zoom in");
else
    safef(itemBuffer, sizeof itemBuffer, "zoom in");

while(numRows--)
    {
    char buffer[1024];

    safef(buffer, sizeof buffer, 
	"Too many items in display.  Zoom in to click on items. (%d)",numRows);
    mapBoxHc(hvg, seqStart, seqEnd, xOff, yOff, width, fontHeight,
	tg->track, itemBuffer,
	buffer);
    yOff += fontHeight;
    }

// just do this once
tg->customInt = 0;
}

void bamLinkedFeaturesSeriesDraw(struct track *tg,
	int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw bam linked features items. */
{
linkedFeaturesSeriesDraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
        font, color, vis);

if(tg->customInt)
    doMapBoxPerRow(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
            font, color, vis);
}

void bamLinkedFeaturesDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width,
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw linked features items. */
{
linkedFeaturesDraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
        font, color, vis);

if(tg->customInt)
    doMapBoxPerRow(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
            font, color, vis);
}

void bamMethods(struct track *track)
/* Methods for BAM alignment files. */
{
#if (defined USE_BAM && defined KNETFILE_HOOKS)
knetUdcInstall();
#endif//def USE_BAM && KNETFILE_HOOKS

track->canPack = TRUE;
boolean compositeLevel = isNameAtCompositeLevel(track->tdb, BAM_PAIR_ENDS_BY_NAME);
boolean isPaired = cartUsualBooleanClosestToHome(cart, track->tdb, compositeLevel,
			 BAM_PAIR_ENDS_BY_NAME,
			 (trackDbSettingClosestToHome(track->tdb, BAM_PAIR_ENDS_BY_NAME) != NULL));
char *tdbShowNames = trackDbSetting(track->tdb, BAM_SHOW_NAMES);
boolean showNames = cartUsualBooleanClosestToHome(cart, track->tdb, compositeLevel,
						  BAM_SHOW_NAMES, !sameOk(tdbShowNames, "off"));
char *colorMode = cartOrTdbString(cart, track->tdb, BAM_COLOR_MODE, BAM_COLOR_MODE_DEFAULT);
char *userTag = cartOrTdbString(cart, track->tdb, BAM_COLOR_TAG, BAM_COLOR_TAG_DEFAULT);
if (sameString(colorMode, BAM_COLOR_MODE_TAG) && userTag != NULL)
    {
    if (! (isalpha(userTag[0]) && isalnum(userTag[1]) && userTag[2] == '\0'))
	{
	warn("%s: BAM tag '%s' is not valid -- must be a letter followed by a letter or number.",
	     track->tdb->shortLabel, htmlEncode(userTag));
	compositeLevel = isNameAtCompositeLevel(track->tdb, BAM_COLOR_TAG);
	cartRemoveVariableClosestToHome(cart, track->tdb, compositeLevel, BAM_COLOR_TAG);
	}
    }

if (isPaired)
    {
    linkedFeaturesSeriesMethods(track);
    track->loadItems = bamPairedLoadItems;
    track->drawItems = bamLinkedFeaturesSeriesDraw;
    track->drawItemAt = bamPairedDrawAt;
    }
else
    {
    linkedFeaturesMethods(track);
    track->loadItems = bamLoadItems; 
    track->drawItems = bamLinkedFeaturesDraw;
    track->drawItemAt = bamDrawAt;
    }
if (!showNames)
    {
    track->drawName = TRUE; // ironic, but this is how to suppress item labels in pack mode.
    track->drawLeftLabels = maybeDrawLeftLabels;
    }

track->nextItemButtonable = track->nextExonButtonable = FALSE;
track->nextPrevItem = NULL;
track->nextPrevExon = NULL;
if (differentString(colorMode, "off"))
    track->colorShades = shadesOfGray;
}

struct bamWigTrackData
{
struct preDrawElement *preDraw;
double scale;
int width;
int preDrawZero;
};

static int countBam(const bam1_t *bam, void *data)
/* bam_fetch() calls this on each bam alignment retrieved.  */
{
struct bamWigTrackData *btd = (struct bamWigTrackData *)data;
const bam1_core_t *core = &bam->core;

int tLength=0, tPos = core->pos, qPos = 0;
unsigned int *cigar = bam1_cigar(bam);
int i;
double scale = btd->scale;
for (i = 0;  i < core->n_cigar;  i++)
    {
    char op;
    int n = bamUnpackCigarElement(cigar[i], &op);
    switch (op)
	{
	case 'M': // match or mismatch (gapless aligned block)
	    {
	    int start = (int)(scale * (tPos - winStart));
	    int end = (int)(scale * ((tPos + n) - winStart));
	    for(i=start; i < end; i++)
		btd->preDraw[i + btd->preDrawZero].count++;
	    tPos =  tPos + n;
	    qPos =  qPos + n;
	    tLength += n;
	    break;
	    }
	case 'I': // inserted in query
	    qPos += n;
	    break;
	case 'D': // deleted from query
	case 'N': // long deletion from query (intron as opposed to small del)
	    tPos += n;
	    tLength += n;
	    break;
	case 'S': // skipped query bases at beginning or end ("soft clipping")
	case 'H': // skipped query bases not stored in record's query sequence ("hard clipping")
	case 'P': // P="silent deletion from padded reference sequence" -- ignore these.
	    break;
	default:
	    errAbort("countBam: unrecognized CIGAR op %c -- update me", op);
	}

    }
return 0;
}

static void bamWigLoadItems(struct track *tg)
{
char *fileName = NULL;
/* Figure out bigWig file name. */
if (tg->parallelLoading)
    {
    /* do not use mysql during parallel-fetch load */
    fileName = trackDbSetting(tg->tdb, "bigDataUrl");
    }
else
    {
    struct sqlConnection *conn = hAllocConnTrack(database, tg->tdb);
    /* this should call bamFileNameFromTable with logic from bamLoadItemsCore to 
     * check the bigDataUrl setting.  Fix this if bamWigs end up being
     * a supported type.   It may be that this code gets rolled into
     * normal BAM display... since that's the plan ;-).
     */
    fileName = bbiNameFromSettingOrTable(tg->tdb, conn, tg->table);
    hFreeConn(&conn);
    }

/* Allocate predraw area. */
struct bamWigTrackData *bwData;
double scale = (double)insideWidth/(winEnd - winStart);

struct preDrawContainer *pre = tg->preDrawContainer = initPreDrawContainer(insideWidth);
AllocVar(bwData);
bwData->preDraw = pre->preDraw;
bwData->scale = scale;
bwData->width = insideWidth;
bwData->preDrawZero = pre->preDrawZero;

char posForBam[512];
safef(posForBam, sizeof(posForBam), "%s:%d-%d", chromName, winStart, winEnd);

bamFetch(fileName, posForBam, countBam, bwData, NULL);

/* fill in rest of predraw */
int preDrawZero = pre->preDrawZero;
int i;
for (i=0; i<insideWidth; ++i)
    {
    struct preDrawElement *pe = &bwData->preDraw[i + preDrawZero];
    pe->min = pe->count;
    pe->max = pe->count;
    pe->sumData = pe->count / scale;
    pe->sumSquares = (pe->count * pe->count)/scale;
    }

}

static void bamWigDrawItems(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width,
	MgFont *font, Color color, enum trackVisibility vis)
{
/* Call actual graphing routine. */
wigDrawPredraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis,
	       tg->preDrawContainer, tg->preDrawContainer->preDrawZero, tg->preDrawContainer->preDrawSize, &tg->graphUpperLimit, &tg->graphLowerLimit);

}

void bamWigMethods(struct track *track, struct trackDb *tdb, 
	int wordCount, char *words[])
/* Set up bamWig methods. */
{
bedGraphMethods(track, tdb, wordCount, words);
track->loadItems = bamWigLoadItems;
track->drawItems = bamWigDrawItems;
}
#else /* no USE_BAM */

#include "common.h"
#include "hgTracks.h"
// If code was not built with USE_BAM=1, but there are bam tracks, display a message
// in place of the tracks (instead of annoying "No track handler" warning messages).

void drawUseBamWarning(struct track *tg, int seqStart, int seqEnd, struct hvGfx *hvg,
                 int xOff, int yOff, int width, MgFont *font, Color color,
                 enum trackVisibility vis)
/* Draw a message saying that the code needs to be built with USE_BAM=1. */
{
char message[512];
safef(message, sizeof(message),
      "Get samtools(.sourceforge.net) and recompile kent/src with USE_BAM=1");
Color yellow = hvGfxFindRgb(hvg, &undefinedYellowColor);
hvGfxBox(hvg, xOff, yOff, width, tg->heightPer, yellow);
hvGfxTextCentered(hvg, xOff, yOff, width, tg->heightPer, MG_BLACK, font, message);
}

void bamMethods(struct track *track)
/* Methods for BAM alignment files, in absence of USE_BAM (samtools lib). */
{
messageLineMethods(track);
track->drawItems = drawUseBamWarning;
}

void bamWigMethods(struct track *track, struct trackDb *tdb, 
	int wordCount, char *words[])
/* Same stub when compiled without USE_BAM. */
{
bamMethods(track);
}

#endif /* no USE_BAM */
