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
// bam.h is incomplete without _IOLIB set to 1, 2 or 3.  2 is used by Makefile.generic:
#define _IOLIB 2
#include "bam.h"
#include "sam.h"

static char const rcsid[] = "$Id: bamTrack.c,v 1.1 2009/06/24 20:33:03 angie Exp $";

#define BAM_MAX_ZOOM 200000

struct bamTrackData
    {
    struct track *tg;
// TODO: save state here when we add pairing into linkedFeaturesSeries
//    struct hash *looseEnds;
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
    char op = "MIDNSHP"[cigar[i]&BAM_CIGAR_MASK];
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
	    errAbort("Unrecognized CIGAR op character '%c' (%03o)", op, op);
	    ;
	}
    }
if (retLength != NULL)
    *retLength = tLength;
return sfList;
}

int bamToLf(const bam1_t *bam, void *data)
/* bam_fetch() calls this on each bam alignment retrieved.  Translate each bam 
 * into a linkedFeatures item, and add it to tg->items. */
{
struct bamTrackData *btd = (struct bamTrackData *)data;
struct track *tg = btd->tg;
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
lf->extra = qSeq;
slAddHead(&(tg->items), lf);
return 0;
}

void bamLoadItems(struct track *tg)
/* Load BAM data into tg->items item list, unless zoomed out so far
 * that the data would just end up in dense mode and be super-slow. */
{
if (winEnd-winStart > BAM_MAX_ZOOM)
    return;
struct sqlConnection *conn = hAllocConn(database);
char *bamFileName = bbiNameFromTable(conn, tg->mapName);
hFreeConn(&conn);

// TODO: if bamFile is URL, convert URL to path a la UDC, but under udcFuse mountpoint.
// new hg.conf setting for udcFuse mountpoint/support.

bam_index_t *idx = bam_index_load(bamFileName);
samfile_t *fh = samopen(bamFileName, "rb", NULL);
int chromId, s, e;

// TODO: watch out for presence/absence of initial "chr" -- need trackDb setting
char *posToParse = position;
if (startsWith("chr", position))
    posToParse += 3;

bam_parse_region(fh->header, posToParse, &chromId, &s, &e);
struct bamTrackData btd = {tg};
int ret = bam_fetch(fh->x.bam, idx, chromId, winStart, winEnd, &btd, bamToLf);
if (ret != 0)
    errAbort("bam_fetch(%s, %s=%d:%d-%d failed (%d)", bamFileName, chromName,
	     chromId, winStart+1, winEnd, ret);
samclose(fh);
if (tg->visibility != tvDense)
    {
    slReverse(&(tg->items));
    slSort(&(tg->items), linkedFeaturesCmpStart);
    }
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

void bamMethods(struct track *track)
/* Methods for BAM alignment files. */
{
track->canPack = TRUE;
linkedFeaturesMethods(track);
track->loadItems = bamLoadItems;
track->drawItems = bamDrawItems;
}

#endif /* USE_BAM */
