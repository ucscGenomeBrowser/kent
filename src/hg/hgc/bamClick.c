/* bamClick - handler for alignments in BAM format (produced by MAQ,
 * BWA and some other short-read alignment tools). */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "hdb.h"
#include "hgBam.h"
#include "hgc.h"
#include "knetUdc.h"
#include "udc.h"
#include "chromAlias.h"


#include "hgBam.h"
#include "hgConfig.h"

struct bamTrackData
    {
    int itemStart;
    char *itemName;
    struct hash *pairHash;
    boolean foundIt;
    };

#include <htslib/sam.h>

static void getClippedBases(const bam1_t *bam, int clippedBases[4]) 
/* Function to get the number of soft and hard clipped bases at the start and end of a read,
 the array will contain [startHardCnt, startSoftCnt, endSoftCnt, endHardCnt] */
{
uint32_t *cigar = bam_get_cigar(bam);
int nCigar = bam->core.n_cigar;
for (int i = 0; i < 4; i++)
    clippedBases[i] = 0;

int iCigar = 0;

// start
if ((iCigar < nCigar) && (bam_cigar_op(cigar[iCigar]) == BAM_CHARD_CLIP))
    {
    clippedBases[0] += bam_cigar_oplen(cigar[iCigar]);
    iCigar++;
    }
if ((iCigar < nCigar) && (bam_cigar_op(cigar[iCigar]) == BAM_CSOFT_CLIP))
    {
    clippedBases[1] += bam_cigar_oplen(cigar[iCigar]);
    }

// end
iCigar = nCigar - 1;
if ((iCigar >= 0) && (bam_cigar_op(cigar[iCigar]) == BAM_CHARD_CLIP))
    {
    clippedBases[3] += bam_cigar_oplen(cigar[iCigar]);
    iCigar--;
    }
if ((iCigar >= 0) && (bam_cigar_op(cigar[iCigar]) == BAM_CSOFT_CLIP))
    {
    clippedBases[2] += bam_cigar_oplen(cigar[iCigar]);
    }
}

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

// alignment type
char *alnType = NULL;
if (bam->core.flag & BAM_FSECONDARY)
    alnType = "Secondary";
else if (bam->core.flag & BAM_FSUPPLEMENTARY)
    alnType = "Supplementary";
else
    alnType = "Primary";
printf("<B>Alignment type:</B> %s</BR>\n", alnType);

// clipping
int clippedBases[4];
getClippedBases(bam, clippedBases);
printf("<B>Start clipping:</B> hard: %d  soft: %d</BR>\n", clippedBases[0], clippedBases[1]);
printf("<B>End clipping:</B> hard: %d  soft: %d</BR> \n", clippedBases[3], clippedBases[2]);

if (core->n_cigar > 50)
    printf("<B>CIGAR string: </B> Cannot show long CIGAR string, more than 50 operations. Contact us if you need to see the full CIGAR string here.<BR>\n");
else
    {
    printf("<B>CIGAR string: </B><tt>%s</tt>", bamGetCigar(bam));
    printf("<BR>\n");
    //bamShowCigarEnglish(bam);
    printf("<p><B>CIGAR Legend:</B><BR>"
            "<b>M</b> : alignment match (seq. match or mismatch), <b>I</b> : insert from genome, <b>D</b> : deletion from genome, "
            "<b>N</b> : skipped from genome, <BR>"
            "<b>S</b> : soft clipping, <b>H</b> : hard clipping, <b>P</b> : padding, "
            "<b>=</b> : sequence match, <b>X</b> : sequence mismatch\n");
    printf("</p>\n");
    }

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
struct dnaSeq *genoSeq = hChromSeq(database, seqName, tStart, tEnd);
char *qSeq = bamGetQuerySequence(bam, FALSE);
if (core->l_qseq > 5000)
    printf("<B>Alignment not shown, query sequence is %d bp long &gt; 5000bp</B><BR>\n", core->l_qseq);
else
    {
    if (isNotEmpty(qSeq) && !sameString(qSeq, "*"))
        {
        char *qSeq = NULL;
        struct ffAli *ffa = bamToFfAli(bam, genoSeq, tStart, useStrand, &qSeq);
        printf("<B>Alignment of %s to %s:%d-%d%s:</B><BR>\n", itemName,
               seqName, tStart+1, tEnd, (isRc ? " (reverse complemented)" : ""));
        ffShowSideBySide(stdout, ffa, qSeq, 0, genoSeq->dna, tStart, tLength, 0, tLength, 8, isRc,
                         FALSE);
        }
    }

if (!skipQualityScore && core->l_qseq > 0)
    {
    if (core->l_qseq > 5000)
        {
        printf("<B>Sequence quality not shown, query sequence %d bp long &gt; 5000bp</B><BR>\n", core->l_qseq);
        } 
    else
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

static int oneBam(const bam1_t *bam, void *data, bam_hdr_t *header)
/* This is called on each record retrieved from a .bam file. */
{
const bam1_core_t *core = &bam->core;
if (core->flag & BAM_FUNMAP)
    return 0;
struct bamTrackData *btd = (struct bamTrackData *)data;
if (sameString(bam1_qname(bam), btd->itemName))
    {
    btd->foundIt = TRUE;
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

knetUdcInstall();
if (udcCacheTimeout() < 300)
    udcSetCacheTimeout(300);

if (sameString(item, "zoom in"))
    printf("Zoom in to a region with fewer items to enable 'detail page' links for individual items.<BR>");

char varName[1024];
safef(varName, sizeof(varName), "%s_pairEndsByName", tdb->track);
boolean isPaired = cartUsualBoolean(cart, varName,
				    (trackDbSetting(tdb, "pairEndsByName") != NULL));
char position[512];
struct hash *pairHash = isPaired ? hashNew(0) : NULL;
struct bamTrackData btd = {start, item, pairHash, FALSE};
char *fileName = hReplaceGbdb(trackDbSetting(tdb, "bigDataUrl"));
if (fileName == NULL)
    {
    if (isCustomTrack(tdb->table))
	{
	errAbort("bamLoadItemsCore: can't find bigDataUrl for custom track %s", tdb->track);
	}
    else
	{
	struct sqlConnection *conn = hAllocConnTrack(database, tdb);
	fileName = hReplaceGbdb(bamFileNameFromTable(conn, tdb->table, seqName));
	hFreeConn(&conn);
	}
    }

char *indexName = hReplaceGbdb(trackDbSetting(tdb, "bigDataIndex"));
char *cacheDir =  cfgOption("cramRef");
char *refUrl = trackDbSetting(tdb, "refUrl");

struct slName *aliasList = chromAliasFindAliases(seqName);
struct slName *nativeName = newSlName(seqName);
slAddHead(&aliasList, nativeName);

char *chromName = NULL;
for (; aliasList; aliasList = aliasList->next)
    {
    chromName = aliasList->name;
    safef(position, sizeof(position), "%s:%d-%d", chromName, winStart, winEnd);

    bamAndIndexFetchPlus(fileName, indexName, position, oneBam, &btd, NULL, refUrl, cacheDir);
    if (btd.foundIt)
        break;
    }

if (isPaired)
    {
    char *setting = trackDbSettingOrDefault(tdb, "pairSearchRange", "20000");
    int pairSearchRange = atoi(setting);
    if (pairSearchRange > 0 && hashNumEntries(pairHash) > 0)
	{
	// Repeat the search for item in a larger window:
	struct hash *newPairHash = hashNew(0);
	btd.pairHash = newPairHash;
	safef(position, sizeof(position), "%s:%d-%d", chromName,
	      max(0, winStart-pairSearchRange), winEnd+pairSearchRange);
        bamAndIndexFetchPlus(fileName, indexName, position, oneBam, &btd, NULL, refUrl, cacheDir);
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

