/* Handle details pages for wiggle tracks. */

#include "common.h"
#include "wiggle.h"
#include "cart.h"
#include "hgc.h"
#include "hCommon.h"
#include "hgColors.h"
#include "bigBed.h"
#include "hui.h"

static void bigBedClick(char *fileName, struct trackDb *tdb,
                     char *item, int start, int end, int bedSize)
/* Handle click in generic bigBed track. */
{
boolean showUrl = FALSE;
char *chrom = cartString(cart, "c");

/* Open BigWig file and get interval list. */
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct lm *lm = lmInit(0);
int ivStart = start, ivEnd = end;
if (start == end)
    {
    // item is an insertion; expand the search range from 0 bases to 2 so we catch it:
    ivStart = max(0, start-1);
    ivEnd++;
    }
struct bigBedInterval *bbList = bigBedIntervalQuery(bbi, chrom, ivStart, ivEnd, 0, lm);

/* Get bedSize if it's not already defined. */
if (bedSize == 0)
    {
    bedSize = bbi->definedFieldCount;
    showUrl = TRUE;
    }


char *scoreFilter = cartOrTdbString(cart, tdb, "scoreFilter", NULL);
int minScore = 0;
if (scoreFilter)
    minScore = atoi(scoreFilter);

/* Find particular item in list - matching start, and item if possible. */
boolean found = FALSE;
boolean firstTime = TRUE;
struct bigBedInterval *bb;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    if (!(bb->start == start && bb->end == end))
	continue;
    if (bedSize > 3)
	{
	char *name = cloneFirstWordByTab(bb->rest);
	boolean match = sameString(name, item);
	freez(&name);
	if (!match)
	    continue;
	}

    found = TRUE;
    if (firstTime)
	printf("<BR>\n");
    int seq1Seq2Fields = 0;
    // check for seq1 and seq2 in columns 7+8 (eg, pairedTagAlign)
    boolean seq1Seq2 = sameOk(trackDbSetting(tdb, BASE_COLOR_USE_SEQUENCE), "seq1Seq2");
    if (seq1Seq2 && bedSize == 6)
	seq1Seq2Fields = 2;
    char *fields[bedSize+seq1Seq2Fields];
    char startBuf[16], endBuf[16];
    char *rest = cloneString(bb->rest);
    int bbFieldCount = bigBedIntervalToRow(bb, chrom, startBuf, endBuf, fields,
                                           bedSize+seq1Seq2Fields);
    if (bbFieldCount != bedSize+seq1Seq2Fields)
        {
        errAbort("Disagreement between trackDb field count (%d) and %s fieldCount (%d)",
		bedSize, fileName, bbFieldCount);
	}
    struct bed *bed = bedLoadN(fields, bedSize);
    if (bedSize >= 6 && scoreFilter && bed->score < minScore)
	continue;
    if (showUrl && (bedSize >= 4))
        printCustomUrl(tdb, item, TRUE);
    bedPrintPos(bed, bedSize, tdb);

    // display seq1 and seq2
    if (seq1Seq2 && bedSize+seq1Seq2Fields == 8)
        printf("<table><tr><th>Sequence 1</th><th>Sequence 2</th></tr>"
	       "<tr><td> %s </td><td> %s </td></tr></table>", fields[6], fields[7]);
    else if (isNotEmpty(rest))
	{
	char *restFields[256];
	int restCount = chopTabs(rest, restFields);
	int restBedFields = bedSize - 3;
	if (restCount > restBedFields)
	    {
            if (0 == extraFieldsPrint(tdb,NULL,restFields + restBedFields,restCount - restBedFields))
                {
                int i;
                char label[20];
                safef(label, sizeof(label), "nonBedFieldsLabel");
                printf("<B>%s&nbsp;</B>",
                       trackDbSettingOrDefault(tdb, label, "Non-BED fields:"));
                for (i = restBedFields;  i < restCount;  i++)
                    printf("%s%s", (i > 0 ? "\t" : ""), restFields[i]);
                printf("<BR>\n");
                }
	    }
	}
    if (isCustomTrack(tdb->track))
	{
	time_t timep = bbiUpdateTime(bbi);
	printBbiUpdateTime(&timep);
	}

    }

if (!found)
    {
    printf("No item %s starting at %d\n", emptyForNull(item), start);
    }

lmCleanup(&lm);
bbiFileClose(&bbi);
}

void genericBigBedClick(struct sqlConnection *conn, struct trackDb *tdb,
                     char *item, int start, int end, int bedSize)
/* Handle click in generic bigBed track. */
{
char *fileName = bbiNameFromSettingOrTable(tdb, conn, tdb->table);
bigBedClick(fileName, tdb, item, start, end, bedSize);
}

void bigBedCustomClick(struct trackDb *tdb)
/* Display details for BigWig custom tracks. */
{
char *fileName = trackDbSetting(tdb, "bigDataUrl");
char *item = cartOptionalString(cart, "i");
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
bigBedClick(fileName, tdb, item, start, end, 0);
}
