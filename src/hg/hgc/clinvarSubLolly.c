#include "common.h"
#include "wiggle.h"
#include "cart.h"
#include "hgc.h"
#include "hCommon.h"
#include "hgColors.h"
#include "bigBed.h"
#include "hui.h"
#include "subText.h"   
#include "trackHub.h"   
#include "clinvarSubLolly.h"   


char *statusByScore[] =
{
"Other",
"Benign",
"Likely Benign",
"Uncertain significance",
"Likely pathogenic",
"Pathogenic",
};

void printSubmissions(struct trackDb *tdb, char *chrom, int start, int end, unsigned wantScore, int numSubs, boolean not)
{
char *xrefTrack = trackDbSetting(tdb, "xrefTrack");
struct trackDb *mainTdb = hashFindVal(trackHash, xrefTrack);
char *mainBigBedFile = trackDbSetting(mainTdb, "bigDataUrl");
struct bbiFile *bbi = bigBedFileOpen(mainBigBedFile);
struct lm *lm = lmInit(0);
struct bigBedInterval *bbList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);

int count;
char *extra = "";
if (not)
    {
    count = slCount(bbList) - numSubs;
    extra = "other than ";
    }
else 
    count = numSubs;

if (count == 0)
    return;

printf("<B>There are %d submissions at this position with status %s '%s'</B><BR>\n", count, extra, statusByScore[wantScore]);

struct bigBedInterval *bb;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    char *fields[bbi->fieldCount];
    int restCount = chopTabs(cloneString(bb->rest), fields);
    int score = atoi(fields[1]);
    if (not ^ (score != wantScore))
        continue;

    int restBedFields = 6;
    char **extraFields = (fields + restBedFields);
    int extraFieldCount = restCount - restBedFields;
    extraFieldsPrint(mainTdb,NULL,extraFields, extraFieldCount);
    }
printf("<BR>");
}

void doClinvarSubLolly(struct trackDb *tdb, char *item)
/* Put up page for clinvarSubLolly track. */
{
genericHeader(tdb, NULL);
struct sqlConnection *conn = NULL;

if (!trackHubDatabase(database))
    conn = hAllocConnTrack(database, tdb);

int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
char *chrom = cartString(cart, "c");
char *fileName = bbiNameFromSettingOrTable(tdb, conn, tdb->table);
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct lm *lm = lmInit(0);
struct bigBedInterval *bbList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
//int  bedSize = bbi->definedFieldCount;

struct bigBedInterval *bb;
char *fields[bbi->fieldCount];
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    if (!(bb->start == start && bb->end == end))
	continue;
    char *name = cloneFirstWordByDelimiterNoSkip(bb->rest, '\t');
    boolean match = (isEmpty(name) && isEmpty(item)) || sameOk(name, item);
    if (!match)
        continue;
    char startBuf[16], endBuf[16];
    bigBedIntervalToRow(bb, chrom, startBuf, endBuf, fields, bbi->fieldCount);
    int numSubs = chopString(fields[12], ",", NULL, 0);

    printSubmissions(tdb,  chrom, start, end, atoi(fields[4]), numSubs, FALSE);
    printSubmissions(tdb,  chrom, start, end, atoi(fields[4]), numSubs, TRUE);
    break;
    }
}
