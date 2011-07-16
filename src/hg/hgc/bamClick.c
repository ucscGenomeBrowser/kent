/* bamClick - handler for alignments in BAM format (produced by MAQ,
 * BWA and some other short-read alignment tools). */
#ifdef USE_BAM

#include "common.h"
#include "hash.h"
#include "hdb.h"
#include "hgBam.h"
#include "hgc.h"
#if (defined USE_BAM && defined KNETFILE_HOOKS)
#include "knetUdc.h"
#include "udc.h"
#endif//def USE_BAM && KNETFILE_HOOKS

static char const rcsid[] = "$Id: bamClick.c,v 1.21 2010/05/11 01:43:28 kent Exp $";

#include "hgBam.h"

struct bamTrackData
    {
    int itemStart;
    char *itemName;
    struct hash *pairHash;
    };

/* Maybe make this an option someday -- for now, I find it too confusing to deal with
 * CIGAR that is anchored to positive strand while showing rc'd sequence.  I think
 * to do it right, we would need to reverse the CIGAR string for display. */
static boolean useStrand = FALSE;
static boolean skipQualityScore = FALSE;
static void singleBamDetails(const bam1_t *bam)
/* Print out the properties of this alignment. */
{
const bam1_core_t *core = &bam->core;
char *itemName = bam1_qname(bam);
int tLength = bamGetTargetLength(bam);
int tStart = core->pos, tEnd = tStart+tLength;
boolean isRc = useStrand && bamIsRc(bam);
printPosOnChrom(seqName, tStart, tEnd, NULL, FALSE, itemName);
if (!skipQualityScore)
    printf("<B>Alignment Quality: </B>%d<BR>\n", core->qual);
printf("<B>CIGAR string: </B><tt>%s</tt> (", bamGetCigar(bam));
bamShowCigarEnglish(bam);
printf(")<BR>\n");
printf("<B>Tags:</B>");
bamShowTags(bam);
puts("<BR>");
printf("<B>Flags: </B><tt>0x%02x:</tt><BR>\n &nbsp;&nbsp;", core->flag);
bamShowFlagsEnglish(bam);
puts("<BR>");
if (bamIsRc(bam))
    printf("<em>Note: although the read was mapped to the reverse strand of the genome, "
	   "the sequence and CIGAR in BAM are relative to the forward strand.</em><BR>\n");
puts("<BR>");
char nibName[HDB_MAX_PATH_STRING];
hNibForChrom(database, seqName, nibName);
struct dnaSeq *genoSeq = hFetchSeq(nibName, seqName, tStart, tEnd);
char *qSeq = bamGetQuerySequence(bam, FALSE);
if (isNotEmpty(qSeq) && !sameString(qSeq, "*"))
    {
    char *qSeq = NULL;
    struct ffAli *ffa = bamToFfAli(bam, genoSeq, tStart, useStrand, &qSeq);
    printf("<B>Alignment of %s to %s:%d-%d%s:</B><BR>\n", itemName,
	   seqName, tStart+1, tEnd, (isRc ? " (reverse complemented)" : ""));
    ffShowSideBySide(stdout, ffa, qSeq, 0, genoSeq->dna, tStart, tLength, 0, tLength, 8, isRc,
		     FALSE);
    }
if (!skipQualityScore && core->l_qseq > 0)
    {
    printf("<B>Sequence quality scores:</B><BR>\n<TT><TABLE><TR>\n");
    UBYTE *quals = bamGetQueryQuals(bam, useStrand);
    int i;
    for (i = 0;  i < core->l_qseq;  i++)
        {
        if (i > 0 && (i % 24) == 0)
	    printf("</TR>\n<TR>");
        printf("<TD>%c<BR>%d</TD>", qSeq[i], quals[i]);
        }
    printf("</TR></TABLE></TT>\n");
    }
}

static void showOverlap(const bam1_t *leftBam, const bam1_t *rightBam)
/* If the two reads overlap, show how. */
{
const bam1_core_t *leftCore = &(leftBam->core), *rightCore = &(rightBam->core);
int leftStart = leftCore->pos, rightStart = rightCore->pos;
int leftLen = bamGetTargetLength(leftBam), rightLen = bamGetTargetLength(rightBam);
char *leftSeq = bamGetQuerySequence(leftBam, useStrand);
char *rightSeq = bamGetQuerySequence(rightBam, useStrand);
if (useStrand && bamIsRc(leftBam))
    reverseComplement(leftSeq, strlen(leftSeq));
if (useStrand && bamIsRc(rightBam))
    reverseComplement(rightSeq, strlen(rightSeq));
if ((rightStart > leftStart && leftStart + leftLen > rightStart) ||
    (leftStart > rightStart && rightStart+rightLen > leftStart))
    {
    int leftClipLow, rightClipLow;
    bamGetSoftClipping(leftBam, &leftClipLow, NULL, NULL);
    bamGetSoftClipping(rightBam, &rightClipLow, NULL, NULL);
    leftStart -= leftClipLow;
    rightStart -= rightClipLow;
    printf("<B>Note: End read alignments overlap:</B><BR>\n<PRE><TT>");
    int i = leftStart - rightStart;
    while (i-- > 0)
	putc(' ', stdout);
    puts(leftSeq);
    i = rightStart - leftStart;
    while (i-- > 0)
	putc(' ', stdout);
    puts(rightSeq);
    puts("</TT></PRE>");
    }
}

static void bamPairDetails(const bam1_t *leftBam, const bam1_t *rightBam)
/* Print out details for paired-end reads. */
{
if (leftBam && rightBam)
    {
    const bam1_core_t *leftCore = &leftBam->core, *rightCore = &rightBam->core;
    int leftLength = bamGetTargetLength(leftBam), rightLength = bamGetTargetLength(rightBam);
    int start = min(leftCore->pos, rightCore->pos);
    int end = max(leftCore->pos+leftLength, rightCore->pos+rightLength);
    char *itemName = bam1_qname(leftBam);
    printf("<B>Paired read name:</B> %s<BR>\n", itemName);
    printPosOnChrom(seqName, start, end, NULL, FALSE, itemName);
    puts("<P>");
    }
showOverlap(leftBam, rightBam);
printf("<TABLE><TR><TD valign=top><H4>Left end read</H4>\n");
singleBamDetails(leftBam);
printf("</TD><TD valign=top><H4>Right end read</H4>\n");
singleBamDetails(rightBam);
printf("</TD></TR></TABLE>\n");
}

static int oneBam(const bam1_t *bam, void *data)
/* This is called on each record retrieved from a .bam file. */
{
const bam1_core_t *core = &bam->core;
if (core->flag & BAM_FUNMAP)
    return 0;
struct bamTrackData *btd = (struct bamTrackData *)data;
if (sameString(bam1_qname(bam), btd->itemName))
    {
    if (btd->pairHash == NULL || (core->flag & BAM_FPAIRED) == 0)
	{
	if (core->pos == btd->itemStart)
	    {
	    printf("<B>Read name:</B> %s<BR>\n", btd->itemName);
	    singleBamDetails(bam);
	    }
	}
    else
	{
	bam1_t *firstBam = (bam1_t *)hashFindVal(btd->pairHash, btd->itemName);
	if (firstBam == NULL)
	    hashAdd(btd->pairHash, btd->itemName, bamClone(bam));
	else
	    {
	    bamPairDetails(firstBam, bam);
	    hashRemove(btd->pairHash, btd->itemName);
	    }
	}
    }
return 0;
}

void doBamDetails(struct trackDb *tdb, char *item)
/* Show details of an alignment from a BAM file. */
{
if (item == NULL)
    errAbort("doBamDetails: NULL item name");
int start = cartInt(cart, "o");
if (!tdb || !trackDbSetting(tdb, "bamSkipPrintQualScore"))
   skipQualityScore = FALSE;
else
   skipQualityScore = TRUE;
// TODO: libify tdb settings table_pairEndsByName, stripPrefix and pairSearchRange

#if (defined USE_BAM && defined KNETFILE_HOOKS)
knetUdcInstall();
if (udcCacheTimeout() < 300)
    udcSetCacheTimeout(300);
#endif//def USE_BAM && KNETFILE_HOOKS

if (sameString(item, "zoom in"))
    printf("Zoom in to a region with fewer items to enable 'detail page' links for individual items.<BR>");

char varName[1024];
safef(varName, sizeof(varName), "%s_pairEndsByName", tdb->track);
boolean isPaired = cartUsualBoolean(cart, varName,
				    (trackDbSetting(tdb, "pairEndsByName") != NULL));
char position[512];
safef(position, sizeof(position), "%s:%d-%d", seqName, winStart, winEnd);
struct hash *pairHash = isPaired ? hashNew(0) : NULL;
struct bamTrackData btd = {start, item, pairHash};
char *fileName = trackDbSetting(tdb, "bigDataUrl");
if (fileName == NULL)
    {
    if (isCustomTrack(tdb->table))
	{
	errAbort("bamLoadItemsCore: can't find bigDataUrl for custom track %s", tdb->track);
	}
    else
	{
	struct sqlConnection *conn = hAllocConnTrack(database, tdb);
	fileName = bamFileNameFromTable(conn, tdb->table, seqName);
	hFreeConn(&conn);
	}
    }

bamFetch(fileName, position, oneBam, &btd, NULL);
if (isPaired)
    {
    char *setting = trackDbSettingOrDefault(tdb, "pairSearchRange", "20000");
    int pairSearchRange = atoi(setting);
    if (pairSearchRange > 0 && hashNumEntries(pairHash) > 0)
	{
	// Repeat the search for item in a larger window:
	struct hash *newPairHash = hashNew(0);
	btd.pairHash = newPairHash;
	safef(position, sizeof(position), "%s:%d-%d", seqName,
	      max(0, winStart-pairSearchRange), winEnd+pairSearchRange);
	bamFetch(fileName, position, oneBam, &btd, NULL);
	}
    struct hashEl *hel;
    struct hashCookie cookie = hashFirst(btd.pairHash);
    while ((hel = hashNext(&cookie)) != NULL)
	{
	bam1_t *bam = hel->val;
	const bam1_core_t *core = &bam->core;
	if (! (core->flag & BAM_FMUNMAP))
	    printf("<B>Note: </B>unable to find paired end for %s "
		   "within +-%d bases of viewing window %s<BR>\n",
		   item, pairSearchRange, addCommasToPos(database, cartString(cart, "position")));
	else
	    printf("<B>Paired read name:</B> %s<BR>\n", item);
	singleBamDetails(bam);
	}
    }
}

#endif//def USE_BAM
