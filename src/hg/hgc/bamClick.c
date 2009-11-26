/* bamClick - handler for alignments in BAM format (produced by MAQ,
 * BWA and some other short-read alignment tools). */
#ifdef USE_BAM

#include "common.h"
#include "hash.h"
#include "hdb.h"
#include "bamFile.h"
#include "hgc.h"

static char const rcsid[] = "$Id: bamClick.c,v 1.12 2009/11/26 00:29:11 angie Exp $";

#include "bamFile.h"

struct bamTrackData
    {
    int itemStart;
    char *itemName;
    struct hash *pairHash;
    };

static void singleBamDetails(const bam1_t *bam)
/* Print out the properties of this alignment. */
{
const bam1_core_t *core = &bam->core;
char *itemName = bam1_qname(bam);
int tLength = bamGetTargetLength(bam);
int tStart = core->pos, tEnd = tStart+tLength;
boolean isRc = bamIsRc(bam);
printPosOnChrom(seqName, tStart, tEnd, NULL, FALSE, itemName);
printf("<B>Alignment Quality: </B>%d<BR>\n", core->qual);
printf("<B>CIGAR string: </B><tt>%s</tt> (", bamGetCigar(bam));
bamShowCigarEnglish(bam);
printf(")<BR>\n");
printf("<B>Tags:</B>");
bamShowTags(bam);
puts("<BR>");
printf("<B>Flags: </B><tt>0x%02x:</tt><BR>\n &nbsp;&nbsp;", core->flag);
bamShowFlagsEnglish(bam);
puts("<BR><BR>");
char nibName[HDB_MAX_PATH_STRING];
hNibForChrom(database, seqName, nibName);
struct dnaSeq *genoSeq = hFetchSeq(nibName, seqName, tStart, tEnd);
struct ffAli *ffa = bamToFfAli(bam, genoSeq, tStart);
char *qSeq = ffa->nStart;
printf("<B>Alignment of %s to %s:%d-%d%s:</B><BR>\n", itemName,
       seqName, tStart+1, tEnd, (isRc ? " (reverse complemented)" : ""));
ffShowSideBySide(stdout, ffa, qSeq, 0, genoSeq->dna, tStart, tLength, 0, tLength, 8, isRc,
		 FALSE);
printf("<B>Sequence quality scores:</B><BR>\n<TT><TABLE><TR>\n");
UBYTE *quals = bamGetQueryQuals(bam);
int i;
for (i = 0;  i < core->l_qseq;  i++)
    {
    if (i > 0 && (i % 24) == 0)
	printf("</TR>\n<TR>");
    printf("<TD>%c<BR>%d</TD>", qSeq[i], quals[i]);
    }
printf("</TR></TABLE></TT>\n");
}

static void showOverlap(const bam1_t *leftBam, const bam1_t *rightBam)
/* If the two reads overlap, show how. */
{
const bam1_core_t *leftCore = &(leftBam->core), *rightCore = &(rightBam->core);
int leftStart = leftCore->pos, rightStart = rightCore->pos;
int leftLen = bamGetTargetLength(leftBam), rightLen = bamGetTargetLength(rightBam);
char *leftSeq = bamGetQuerySequence(leftBam), *rightSeq = bamGetQuerySequence(rightBam);
if (bamIsRc(leftBam))
    reverseComplement(leftSeq, strlen(leftSeq));
if (bamIsRc(rightBam))
    reverseComplement(rightSeq, strlen(rightSeq));
if ((rightStart > leftStart && leftStart + leftLen > rightStart) ||
    (leftStart > rightStart && rightStart+rightLen > leftStart))
    {
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
showOverlap(leftBam, rightBam);
printf("<TABLE><TR><TD><H4>Left end read</H4>\n");
singleBamDetails(leftBam);
printf("</TD><TD><H4>Right end read</H4>\n");
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
	    singleBamDetails(bam);
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
// TODO: libify tdb settings table_pairEndsByName, stripPrefix and pairSearchRange

char varName[1024];
safef(varName, sizeof(varName), "%s_pairEndsByName", tdb->tableName);
boolean isPaired = cartUsualBoolean(cart, varName,
				    (trackDbSetting(tdb, "pairEndsByName") != NULL));
char *seqNameForBam = seqName;
char *stripPrefix = trackDbSetting(tdb, "stripPrefix");
if (stripPrefix && startsWith(stripPrefix, seqName))
    seqNameForBam = seqName + strlen(stripPrefix);
char posForBam[512];
safef(posForBam, sizeof(posForBam), "%s:%d-%d", seqNameForBam, winStart, winEnd);

bamIgnoreStrand();
struct hash *pairHash = isPaired ? hashNew(0) : NULL;
struct bamTrackData btd = {start, item, pairHash};
char *fileName;
if (isCustomTrack(tdb->tableName))
    {
    fileName = trackDbSetting(tdb, "bigDataUrl");
    if (fileName == NULL)
	errAbort("doBamDetails: can't find bigDataUrl for custom track %s", tdb->tableName);
    }
else
    fileName = bamFileNameFromTable(database, tdb->tableName, seqNameForBam);
bamFetch(fileName, posForBam, oneBam, &btd);
if (isPaired)
    {
    char *setting = trackDbSettingOrDefault(tdb, "pairSearchRange", "20000");
    int pairSearchRange = atoi(setting);
    if (pairSearchRange > 0 && hashNumEntries(pairHash) > 0)
	{
	// Repeat the search for item in a larger window:
	struct hash *newPairHash = hashNew(0);
	btd.pairHash = newPairHash;
	safef(posForBam, sizeof(posForBam), "%s:%d-%d", seqNameForBam,
	      max(0, winStart-pairSearchRange), winEnd+pairSearchRange);
	bamFetch(fileName, posForBam, oneBam, &btd);
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
	singleBamDetails(bam);
	}
    }
}

#endif//def USE_BAM
