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
/* Retrieve this item or items from track table */
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

struct interact *inters = NULL, *inter = NULL;
char **row;
int offset;
struct sqlResult *sr = hRangeQuery(conn, table, chrom, start, end, NULL, &offset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    inter = interactLoad(row+offset);
    if (inter->chromStart != start || inter->chromEnd != end)
        continue;
    if (isNotEmpty(item) && differentString(inter->name, item))
        continue;
    slAddHead(&inters, inter);
    }
slSort(&inters, interactDistanceCmp);
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
struct interact *inters = NULL, *inter = NULL;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    char startBuf[16], endBuf[16];
    char *row[32];
    bigBedIntervalToRow(bb, chrom, startBuf, endBuf, row, ArraySize(row));
    inter = interactLoad(row);
    if (inter == NULL)
        continue;
    if (inter->chromStart != start || inter->chromEnd != end)
        continue;
    if (isNotEmpty(item) && differentString(inter->name, item))
        continue;
    slAddHead(&inters, inter);
    }
slSort(&inters, interactDistanceCmp);
return inters;
}

static struct interact *getInteractions(struct trackDb *tdb, char *item, 
                                        char *chrom, int start, int end)
/* Retrieve interact BED item from track */
{
struct interact *inters = NULL;
char *file = trackDbSetting(tdb, "bigDataUrl");
if (file != NULL)
    inters = getInteractFromFile(file, item, chrom, start, end);
else
    inters = getInteractFromTable(tdb, item, chrom, start, end);
return inters;
}

void doInteractItemDetails(struct trackDb *tdb, struct interact *inter, char *item)
/* Details of interaction item */
{
char startBuf[1024], endBuf[1024], sizeBuf[1024];
if (!isEmptyTextField(inter->name))
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
if (!isEmptyTextField(inter->exp))
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
if (isEmptyTextField(inter->sourceName))
    region1Name = "";

char *region2Label = "Target";
char *region2Chrom = inter->targetChrom;
int region2Start = inter->targetStart;
int region2End = inter->targetEnd;
char *region2Name = inter->targetName;
if (isEmptyTextField(inter->targetName))
    region2Name = "";

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

// format and print
sprintLongWithCommas(startBuf, region1Start + 1);
sprintLongWithCommas(endBuf, region1End);
sprintLongWithCommas(sizeBuf, region1End - region1Start);
printf("<b>%s region:</b> %s&nbsp;&nbsp;"
                "<a href='hgTracks?position=%s:%d-%d' target='_blank'>%s:%s-%s</a> %s",
                region1Label, region1Name, region1Chrom, region1Start+1, region1End,
                region1Chrom, startBuf, endBuf, inter->sourceStrand[0] == '.' ? "" : inter->sourceStrand);
printf("&nbsp;&nbsp;%s bp<br>\n", sizeBuf);

sprintLongWithCommas(startBuf, region2Start+1);
sprintLongWithCommas(endBuf, region2End);
sprintLongWithCommas(sizeBuf, region2End - region2Start);
printf("<b>%s region:</b> %s&nbsp;&nbsp;"
                "<a href='hgTracks?position=%s:%d-%d' target='_blank'>%s:%s-%s</a> %s",
                region2Label, region2Name, region2Chrom, region2Start+1, region2End,
                region2Chrom, startBuf, endBuf, inter->targetStrand[0] == '.' ? "" : inter->targetStrand);
printf("&nbsp;&nbsp;%s bp<br>\n", sizeBuf);
int distance = interactRegionDistance(inter);
if (distance > 0)
    {
    // same chrom
    sprintLongWithCommas(sizeBuf, distance);
    printf("<b>Distance between midpoints:</b> %s bp<br>\n", sizeBuf); 
    }

#ifdef TODO /* TODO: get count and score stats of all interactions in window ?*/
double *scores;
AllocArray(scores, count);
#endif
}

void doInteractDetails(struct trackDb *tdb, char *item)
/* Details of interaction items */
{
char *chrom = cartString(cart, "c");
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct interact *inter = NULL;
struct interact *inters = getInteractions(tdb, item, chrom, start, end);
if (inters == NULL)
    errAbort("Can't find interaction %s", item ? item : "");
int count = slCount(inters);
if (count > 1)
    printf("<b>Interactions:</b> %d<hr>", count);
genericHeader(tdb, item);
for (inter = inters; inter; inter = inter->next)
    {
    doInteractItemDetails(tdb, inter, item);
    printf("<hr>\n");
    if (count > 1 && !isEmptyTextField(inter->name) && sameString(inter->name, item))
        printf("<hr>\n");
    }
}


