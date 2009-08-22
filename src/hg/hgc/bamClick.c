/* bamClick - handler for alignments in BAM format (produced by MAQ,
 * BWA and some other short-read alignment tools). */
#ifdef USE_BAM

#include "common.h"
#include "hash.h"
#include "hdb.h"
#include "bamFile.h"
#include "hgc.h"

static char const rcsid[] = "$Id: bamClick.c,v 1.6 2009/08/22 02:18:35 angie Exp $";

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
boolean isRc = ((core->flag & BAM_FREVERSE) != 0);
printPosOnChrom(seqName, tStart, tEnd, (isRc ? "-" : "+"), FALSE, itemName);
printf("<B>Flags: </B><tt>0x%02x</tt><BR>\n", core->flag);
printf("<B>Alignment Quality: </B>%d<BR>\n", core->qual);
printf("<B>CIGAR string: </B><tt>%s</tt><BR>\n", bamGetCigar(bam));
char nibName[HDB_MAX_PATH_STRING];
hNibForChrom(database, seqName, nibName);
struct dnaSeq *genoSeq = hFetchSeq(nibName, seqName, tStart, tEnd);
struct ffAli *ffa = bamToFfAli(bam, genoSeq, tStart);
char *qSeq = ffa->nStart;
printf("<BR><B>Alignment of %s to %s:%d-%d%s:</B><BR>\n", itemName,
       seqName, tStart+1, tEnd, (isRc ? " (reverse complemented)" : ""));
ffShowSideBySide(stdout, ffa, qSeq, 0, genoSeq->dna, tStart, tLength, 0, tLength, 8, isRc,
		 FALSE);
printf("<B>Tags:</B><BR>\n");

// ugly: lifted from bam.c bam_format1:
uint8_t *s = bam1_aux(bam);
while (s < bam->data + bam->data_len)
    {
    uint8_t type, key[2];
    key[0] = s[0]; key[1] = s[1];
    s += 2; type = *s; ++s;
    printf("\t%c%c:", key[0], key[1]);
    if (type == 'A') { printf("A:%c", *s); ++s; }
    else if (type == 'C') { printf("i:%u", *s); ++s; }
    else if (type == 'c') { printf("i:%d", *s); ++s; }
    else if (type == 'S') { printf("i:%u", *(uint16_t*)s); s += 2; }
    else if (type == 's') { printf("i:%d", *(int16_t*)s); s += 2; }
    else if (type == 'I') { printf("i:%u", *(uint32_t*)s); s += 4; }
    else if (type == 'i') { printf("i:%d", *(int32_t*)s); s += 4; }
    else if (type == 'f') { printf("f:%g", *(float*)s); s += 4; }
    else if (type == 'd') { printf("d:%lg", *(double*)s); s += 8; }
    else if (type == 'Z' || type == 'H')
	{
	printf("%c:", (char)type); 
	while (*s) putc(*s++, stdout);
	++s;
	}
    }
//TODO: show flags properly, maybe display sequence quality scores
}

static void bamPairDetails(const bam1_t *leftBam, const bam1_t *rightBam)
/* Print out details for paired-end reads. */
{
//TODO: tell them which one they clicked (match itemStart w/core->pos)
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
