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
#include "bamFile.h"

static char const rcsid[] = "$Id: bamTrack.c,v 1.10 2009/10/08 06:38:23 angie Exp $";

struct bamTrackData
    {
    struct track *tg;
    struct hash *pairHash;
    int minAliQual;
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
	else
	    // not properly paired: make it a lighter shade.
	    lf->grayIx -= 2;
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
char cartVarName[512];
safef(cartVarName, sizeof(cartVarName), "%s_minAliQual", tg->tdb->tableName);
int minAliQual = cartUsualInt(cart, cartVarName, 0);
struct bamTrackData btd = {tg, pairHash, minAliQual};
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
    slSort(&(tg->items), isPaired ? linkedFeaturesSeriesCmp : linkedFeaturesCmpStart);
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

void bamMethods(struct track *track)
/* Methods for BAM alignment files. */
{
track->canPack = TRUE;
char varName[1024];
safef(varName, sizeof(varName), "%s_pairEndsByName", track->mapName);
boolean isPaired = cartUsualBoolean(cart, varName,
				    (trackDbSetting(track->tdb, "pairEndsByName") != NULL));
if (isPaired)
    {
    linkedFeaturesSeriesMethods(track);
    track->loadItems = bamPairedLoadItems;
    }
else
    {
    linkedFeaturesMethods(track);
    track->loadItems = bamLoadItems;
    }
track->labelNextItemButtonable = track->nextItemButtonable = FALSE;
track->labelNextPrevItem = NULL;
track->nextPrevItem = NULL;
track->colorShades = shadesOfGray;
}

#endif /* USE_BAM */
