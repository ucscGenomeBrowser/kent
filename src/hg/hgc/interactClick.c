/* Details page for interact type tracks */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "obscure.h"
#include "hdb.h"
#include "hgc.h"

#include "interact.h"
#include "interactUi.h"

static struct interact *getInteractFromTable(struct trackDb *tdb, char *item, 
                                                char *chrom, int start, int end)
/* Retrieve this item from track table */
{
struct sqlConnection *conn = NULL;
struct customTrack *ct = lookupCt(tdb->track);
char *table;
if (ct != NULL)
    {
    conn = hAllocConn(CUSTOM_TRASH);
    table = ct->dbTableName;
    }
else
    {
    conn = hAllocConnTrack(database, tdb);
    table = tdb->table;
    }
if (conn == NULL)
    return NULL;

struct interact *inters = NULL, *inter;
char **row;
int offset;
char where[512];
if (isNotEmpty(item))
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

static struct interact *getInteractFromFile(char *file, char *item, char *chrom, int start, int end)
/* Retrieve interact BED item from big file */
{
struct bbiFile *bbi = bigBedFileOpen(file);
struct lm *lm = lmInit(0);
struct bigBedInterval *bb, *bbList =  bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    char startBuf[16], endBuf[16];
    char *row[32];
    bigBedIntervalToRow(bb, chrom, startBuf, endBuf, row, ArraySize(row));
    struct interact *interact = interactLoad(row);
    if (interact == NULL)
        continue;
    if (sameString(interact->name, item))
        return interact;
    }
return NULL;
}

static struct interact *getInteract(struct trackDb *tdb, char *item, 
                                        char *chrom, int start, int end)
/* Retrieve interact BED item from track */
{
struct interact *interact = NULL;
char *file = trackDbSetting(tdb, "bigDataUrl");
if (file != NULL)
    interact = getInteractFromFile(file, item, chrom, start, end);
else
    interact = getInteractFromTable(tdb, item, chrom, start, end);
return interact;
}


void doInteractDetails(struct trackDb *tdb, char *item)
/* Details of interaction item */
{
char *chrom = cartString(cart, "c");
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct interact *inter = getInteract(tdb, item, chrom, start, end);
if (inter == NULL)
    errAbort("Can't find interaction '%s'", item);
genericHeader(tdb, item);

char startBuf[1024], endBuf[1024], sizeBuf[1024];
if (isNotEmpty(inter->name))
    printf("<b>Interaction name:</b> %s<br>\n", inter->name);
printf("<b>Interaction region:</b> ");
if (interactOtherChrom(inter))
    printf("across chromosomes<br>");
else
    {
    sprintLongWithCommas(startBuf, inter->chromStart+1);
    sprintLongWithCommas(endBuf, inter->chromEnd);
    sprintLongWithCommas(sizeBuf, inter->chromEnd - inter->chromStart);
    printf("<a href='hgTracks?position=%s:%d-%d' target='_blank'>%s:%s-%s</a>", 
                inter->chrom, inter->chromStart, inter->chromEnd,
                inter->chrom, startBuf, endBuf);
    printf("&nbsp;&nbsp;%s bp<br>\n", sizeBuf);
    }
printf("<b>Score:</b> %d<br>\n", inter->score);
printf("<b>Value:</b> %0.3f<br>\n", inter->value);
if (isNotEmpty(inter->exp))
    printf("<b>Experiment:</b> %s<br>\n", inter->exp);
puts("<p>");

/* print info for both regions */
/* Use different labels:
        1) directional (source/target)
        2) non-directional same chrom (lower/upper)
        3) non-directional other chrom (this/other)
*/
char *region1Label = "Source";
char *region1Chrom = inter->sourceChrom;
int region1Start = inter->sourceStart;
int region1End = inter->sourceEnd;
char *region1Name = inter->sourceName;

char *region2Label = "Target";
char *region2Chrom = inter->targetChrom;
int region2Start = inter->targetStart;
int region2End = inter->targetEnd;
char *region2Name = inter->targetName;

if (!interactUiDirectional(tdb))
    {
    if (interactOtherChrom(inter))
        {
        region1Label = "This";
        region2Label = "Other";
        }
    else
        {
        region1Label = "Lower";
        region2Label = "Upper";
        }
    }

sprintLongWithCommas(startBuf, region1Start + 1);
sprintLongWithCommas(endBuf, region1End);
sprintLongWithCommas(sizeBuf, region1End - region1Start);
printf("<b>%s region:</b> %s&nbsp;&nbsp;"
                "<a href='hgTracks?position=%s:%d-%d' target='_blank'>%s:%s-%s</a>",
                region1Label, region1Name, region1Chrom, region1Start+1, region1End,
                region1Chrom, startBuf, endBuf);
printf("&nbsp;&nbsp;%s bp<br>\n", sizeBuf);

sprintLongWithCommas(startBuf, region2Start+1);
sprintLongWithCommas(endBuf, region2End);
sprintLongWithCommas(sizeBuf, region2End - region2Start);
printf("<b>%s region:</b> %s&nbsp;&nbsp;"
                "<a href='hgTracks?position=%s:%d-%d' target='_blank'>%s:%s-%s</a>",
                region2Label, region2Name, region2Chrom, region2Start+1, region2End,
                region2Chrom, startBuf, endBuf);
printf("&nbsp;&nbsp;%s bp<br>\n", sizeBuf);

#ifdef TODO /* TODO: get count and score stats of all interactions in window ?*/
double *scores;
AllocArray(scores, count);
#endif
}

