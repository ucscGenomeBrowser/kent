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

static char const rcsid[] = "$Id: bamTrack.c,v 1.4 2009/07/27 21:52:09 angie Exp $";

#define BAM_MAX_ZOOM 200000

struct bamTrackData
    {
    struct track *tg;
    struct hash *pairHash;
    };

struct simpleFeature *sfFromNumericCigar(const bam1_t *bam, int *retLength)
/* Translate BAM's numeric CIGAR encoding into a list of simpleFeatures  */
{
const bam1_core_t *core = &bam->core;
struct simpleFeature *sf, *sfList = NULL;
int tLength=0, tPos = core->pos, qPos = 0;
unsigned int *cigar = bam1_cigar(bam);
int i;
for (i = 0;  i < core->n_cigar;  i++)
    {
    // decoding lifted from bam.c bam_format1(), long may it remain stable:
    int n = cigar[i]>>BAM_CIGAR_SHIFT;
    int opcode = cigar[i] & BAM_CIGAR_MASK;
    char op = "MIDNSHP"[opcode];
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
	    errAbort("Unrecognized CIGAR op index %d -- has bam.c bam_format1() changed?", opcode);
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
uint8_t *s = bam1_seq(bam);
struct linkedFeatures *lf;
AllocVar(lf);
lf->score = core->qual;
safef(lf->name, sizeof(lf->name), bam1_qname(bam));
lf->orientation = (core->flag & BAM_FREVERSE) ? -1 : 1;
int length;
lf->components = sfFromNumericCigar(bam, &length);
lf->start = lf->tallStart = core->pos;
lf->end = lf->tallEnd = core->pos + length;
char *qSeq = needMem(core->l_qseq + 1);
int i;
for (i = 0; i < core->l_qseq; i++)
    qSeq[i] = bam_nt16_rev_table[bam1_seqi(s, i)];
if (lf->orientation == -1)
    reverseComplement(qSeq, core->l_qseq);
lf->extra = qSeq;
return lf;
}

int addBam(const bam1_t *bam, void *data)
/* bam_fetch() calls this on each bam alignment retrieved.  Translate each bam 
 * into a linkedFeatures item, and add it to tg->items. */
{
const bam1_core_t *core = &bam->core;
if (core->flag & BAM_FUNMAP)
    return 0;
struct linkedFeatures *lf = bamToLf(bam, data);
struct bamTrackData *btd = (struct bamTrackData *)data;
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
return lf;
}

static struct linkedFeaturesSeries *lfsFromLf(struct linkedFeatures *lf)
/* Make a linkedFeaturesSeries from one or two linkedFeatures elements. */
{
struct linkedFeaturesSeries *lfs;
AllocVar(lfs);
lfs->name = cloneString(lf->name);
lfs->grayIx = lf->grayIx;
if (!lf->next || lf->next->orientation == -lf->orientation)
    lfs->orientation = lf->orientation;
if (lf->next != NULL)
    slSort(&lf, linkedFeaturesCmpStart);
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
if (core->flag & BAM_FUNMAP)
    return 0;
struct linkedFeatures *lf = bamToLf(bam, data);
struct bamTrackData *btd = (struct bamTrackData *)data;
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
	if (core->flag & BAM_FPROPER_PAIR && core->mpos < 0)
	    {
	    // If we know that this is properly paired, but don't know where its mate
	    // is, make a bogus item off the edge of the window so that if we don't
	    // encounter its mate later, we can at least draw an arrow off the
	    // edge of the window.
	    int offscreen = (lf->orientation > 0) ? winEnd + 10 : winStart - 10;
	    if (offscreen < 0) offscreen = 0;
	    struct linkedFeatures *stub = lfStub(offscreen, -lf->orientation);
	    lf->next = stub;
	    }
	else if (! (core->flag & BAM_FPROPER_PAIR))
	    {
	    // TODO: find a way to make this a lighter shade.
	    // doesn't work: lf->grayIx += 2;
	    }
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
if (winEnd-winStart > BAM_MAX_ZOOM)
    return;
char *seqNameForBam = chromName;
char *stripPrefix = trackDbSetting(tg->tdb, "stripPrefix");
if (stripPrefix && startsWith(stripPrefix, chromName))
    seqNameForBam = chromName + strlen(stripPrefix);
char posForBam[512];
safef(posForBam, sizeof(posForBam), "%s:%d-%d", seqNameForBam, winStart, winEnd);

struct hash *pairHash = isPaired ? hashNew(18) : NULL;
struct bamTrackData btd = {tg, pairHash};
bamFetch(database, tg->mapName, posForBam, (isPaired ? addBamPaired : addBam), &btd);
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

void bamDrawItems(struct track *tg, int seqStart, int seqEnd, struct hvGfx *hvg,
		  int xOff, int yOff, int width, MgFont *font, Color color,
		  enum trackVisibility vis)
/* Draw BAM alignments unless zoomed out too far. */
{
if (winEnd-winStart > BAM_MAX_ZOOM)
    return;
linkedFeaturesDraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
}

void bamPairedDrawItems(struct track *tg, int seqStart, int seqEnd, struct hvGfx *hvg,
			int xOff, int yOff, int width, MgFont *font, Color color,
			enum trackVisibility vis)
/* Draw paired-end BAM alignments unless zoomed out too far. */
{
if (winEnd-winStart > BAM_MAX_ZOOM)
    return;
linkedFeaturesSeriesDraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
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
    track->drawItems = bamPairedDrawItems;
    }
else
    {
    linkedFeaturesMethods(track);
    track->loadItems = bamLoadItems;
    track->drawItems = bamDrawItems;
    }
track->labelNextItemButtonable = track->nextItemButtonable = FALSE;
track->labelNextPrevItem = NULL;
track->nextPrevItem = NULL;
}

#endif /* USE_BAM */
