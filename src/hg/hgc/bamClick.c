/* bamClick - handler for alignments in BAM format (produced by MAQ,
 * BWA and some other short-read alignment tools). */
#ifdef USE_BAM

#include "common.h"
#include "hash.h"
#include "hdb.h"
#include "bamFile.h"
#include "hgc.h"

static char const rcsid[] = "$Id: bamClick.c,v 1.7 2009/08/24 23:47:56 angie Exp $";

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
printf("<B>Flags: </B><tt>0x%02x</tt><BR>\n", core->flag);
printf("<B>Alignment Quality: </B>%d<BR>\n", core->qual);
printf("<B>CIGAR string: </B><tt>%s</tt> (", bamGetCigar(bam));
bamShowCigarEnglish(bam);
printf(")<BR>\n");
printf("<B>Tags:</B>");
bamShowTags(bam);
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
//TODO: show flags properly, maybe display sequence quality scores
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
//TODO: tell them which one they clicked (match itemStart w/core->pos)
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
bamFetch(database, tdb->tableName, posForBam, oneBam, &btd);
if (isPaired && hashNumEntries(pairHash) > 0)
    {
    char *setting = trackDbSettingOrDefault(tdb, "pairSearchRange", "1000");
    int pairSearchRange = atoi(setting);
    struct hashEl *hel;
    struct hashCookie cookie = hashFirst(pairHash);
    while ((hel = hashNext(&cookie)) != NULL)
	{
	struct hash *newPairHash = hashNew(0);
	btd.itemName = hel->name;
	btd.pairHash = newPairHash;
	safef(posForBam, sizeof(posForBam), "%s:%d-%d", seqNameForBam,
	      winStart-pairSearchRange, winEnd+pairSearchRange);
	bamFetch(database, tdb->tableName, posForBam, oneBam, &btd);
	if (hashNumEntries(newPairHash) > 0)
	    {
	    struct hashCookie cookie2 = hashFirst(pairHash);
	    while ((hel = hashNext(&cookie2)) != NULL)
		{
		printf("<B>Note: </B>unable to find paired end "
		       "for this %s within +-%d of viewing window<BR>\n",
		       hel->name, pairSearchRange);
		singleBamDetails((bam1_t *)hel->val);
		}
	    }
	}
    }
}

#endif//def USE_BAM
