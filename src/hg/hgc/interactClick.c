/* Details page for interact type tracks */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hdb.h"
#include "hgc.h"

#include "interact.h"
#include "interactUi.h"

static struct interact *getInteract(char *item, char *chrom, int start, int end, char *table)
/* Retrieve this item from track table
 * TODO: Hubbify */
{
struct sqlConnection *conn = hAllocConn(database);
struct interact *inters = NULL, *inter;
char **row;
int offset;
char where[512];
sqlSafefFrag(where, sizeof(where), "name='%s'", item);
struct sqlResult *sr = hRangeQuery(conn, table, chrom, start, end, where, &offset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    inter = interactLoad(row+offset);
    slAddHead(&inters, inter);
    }
slReverse(&inters);
sqlFreeResult(&sr);
hFreeConn(&conn);
return inters;
}

void doInteractDetails(struct trackDb *tdb, char *item)
/* Details of interaction item */
{
char *chrom = cartString(cart, "c");
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
// TODO: hubbify
struct interact *inter = getInteract(item, chrom, start, end, tdb->table);
if (inter == NULL)
    errAbort("Can't find interaction '%s'", item);
genericHeader(tdb, item);
printf("<b>Source: </b> %s &nbsp;&nbsp;<a>%s:%d-%d</a><br>\n", 
                inter->sourceName, inter->sourceChrom, inter->sourceStart, inter->sourceEnd);
printf("<b>Target: </b> %s &nbsp;&nbsp;<a>%s:%d-%d</a><br>\n", 
                inter->targetName, inter->targetChrom, inter->targetStart, inter->targetEnd);
}

#ifdef FOO
static void doLongTabix(struct trackDb *tdb, char *item)
/* Handle a click on a long range interaction */
{
char *bigDataUrl = hashFindVal(tdb->settingsHash, "bigDataUrl");
struct bedTabixFile *btf = bedTabixFileMayOpen(bigDataUrl, NULL, 0, 0);
char *chromName = cartString(cart, "c");
struct bed *list = bedTabixReadBeds(btf, chromName, winStart, winEnd, bedLoad5);
bedTabixFileClose(&btf);
unsigned maxWidth;
struct longRange *longRangeList = parseLongTabix(list, &maxWidth, 0);
struct longRange *longRange, *ourLongRange = NULL;
unsigned itemNum = sqlUnsigned(item);
unsigned count = slCount(longRangeList);
double *doubleArray;

AllocArray(doubleArray, count);

int ii = 0;
for(longRange = longRangeList; longRange; longRange = longRange->next, ii++)
    {
    if (longRange->id == itemNum)
        {
        ourLongRange = longRange;
        }
    doubleArray[ii] = longRange->score;
    }

if (ourLongRange == NULL)
    errAbort("cannot find long range item with id %d\n", itemNum);

printf("Item you clicked on:<BR>\n");
printf("&nbsp;&nbsp;&nbsp;&nbsp;<B>ID:</B> %u<BR>\n", ourLongRange->id);
if (!ourLongRange->hasColor)
    // if there's color, then there's no score in this format
    printf("<B>Score:</B> %g<BR>\n", ourLongRange->score);

unsigned padding =  (ourLongRange->e - ourLongRange->s) / 10;
int s = ourLongRange->s - padding; 
int e = ourLongRange->e + padding; 
if (s < 0 ) 
    s = 0;
int chromSize = hChromSize(database, seqName);
if (e > chromSize)
    e = chromSize;

char sStartPosBuf[1024], sEndPosBuf[1024];
char eStartPosBuf[1024], eEndPosBuf[1024];
// FIXME:  longRange should store region starts, not centers
sprintLongWithCommas(sStartPosBuf, ourLongRange->s - ourLongRange->sw/2 + 1);
sprintLongWithCommas(sEndPosBuf, ourLongRange->s + ourLongRange->sw/2 + 1);
sprintLongWithCommas(eStartPosBuf, ourLongRange->e - ourLongRange->ew/2 + 1);
sprintLongWithCommas(eEndPosBuf, ourLongRange->e + ourLongRange->ew/2 + 1);
char sWidthBuf[1024], eWidthBuf[1024];
char regionWidthBuf[1024];
sprintLongWithCommas(sWidthBuf, ourLongRange->sw);
sprintLongWithCommas(eWidthBuf, ourLongRange->ew);
sprintLongWithCommas(regionWidthBuf, ourLongRange->ew + ourLongRange->e - ourLongRange->s);

if (differentString(ourLongRange->sChrom, ourLongRange->eChrom))
    {
    printf("<B>Current region: </B>");
    printf("<A HREF=\"hgTracks?position=%s:%s-%s \" TARGET=_BLANK>%s:%s-%s (%s bp)</A><BR>\n",  
            ourLongRange->sChrom, sStartPosBuf, sEndPosBuf,
            ourLongRange->sChrom, sStartPosBuf,sEndPosBuf, sWidthBuf);
    printf("<B>Paired region: </B>");
    printf("<A HREF=\"hgTracks?position=%s:%s-%s \" TARGET=_BLANK>%s:%s-%s (%s bp)<BR></A><BR>\n",  
            ourLongRange->eChrom, eStartPosBuf, eEndPosBuf, 
            ourLongRange->eChrom, eStartPosBuf, eEndPosBuf, eWidthBuf);
    }
else
    {
    printf("<B>Lower region: </B>");
    printf("<A HREF=\"hgTracks?position=%s:%s-%s \" TARGET=_BLANK>%s:%s-%s (%s bp)</A><BR>\n",  
            ourLongRange->sChrom, sStartPosBuf,sEndPosBuf, 
            ourLongRange->sChrom, sStartPosBuf,sEndPosBuf, sWidthBuf);
    printf("<B>Upper region: </B>");
    printf("<A HREF=\"hgTracks?position=%s:%s-%s \" TARGET=_BLANK>%s:%s-%s (%s bp)<BR></A><BR>\n",  
            ourLongRange->eChrom, eStartPosBuf, eEndPosBuf, 
            ourLongRange->eChrom, eStartPosBuf, eEndPosBuf, eWidthBuf);
    printf("<B>Interaction region: </B>");
    printf("<A HREF=\"hgTracks?position=%s:%s-%s \" TARGET=_BLANK>%s:%s-%s (%s bp)<BR></A><BR>\n",  
            ourLongRange->eChrom, sStartPosBuf, eEndPosBuf, 
            ourLongRange->eChrom, sStartPosBuf, eEndPosBuf, regionWidthBuf);
    }
if (ourLongRange->hasColor)
    return;

struct aveStats *as = aveStatsCalc(doubleArray, count);
printf("<BR>Statistics on the scores of all items in window (go to track controls to set minimum score to display):\n");

printf("<TABLE BORDER=1>\n");
printf("<TR><TD><B>Q1</B></TD><TD>%f</TD></TR>\n", as->q1);
printf("<TR><TD><B>median</B></TD><TD>%f</TD></TR>\n", as->median);
printf("<TR><TD><B>Q3</B></TD><TD>%f</TD></TR>\n", as->q3);
printf("<TR><TD><B>average</B></TD><TD>%f</TD></TR>\n", as->average);
printf("<TR><TD><B>min</B></TD><TD>%f</TD></TR>\n", as->minVal);
printf("<TR><TD><B>max</B></TD><TD>%f</TD></TR>\n", as->maxVal);
printf("<TR><TD><B>count</B></TD><TD>%d</TD></TR>\n", as->count);
printf("<TR><TD><B>total</B></TD><TD>%f</TD></TR>\n", as->total);
printf("<TR><TD><B>standard deviation</B></TD><TD>%f</TD></TR>\n", as->stdDev);
printf("</TABLE>\n");
}
#endif
