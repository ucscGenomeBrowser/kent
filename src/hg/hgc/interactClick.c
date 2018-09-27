/* Details page for interact type tracks */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "obscure.h"
#include "hdb.h"
#include "jksql.h"
#include "hgc.h"

#include "interact.h"
#include "interactUi.h"

struct interactPlusRow
    {
    /* Keep field values in string format, for url processing */
    struct interactPlusRow *next;
    struct interact *interact;
    char **row;
    };

static struct interactPlusRow *getInteractsFromTable(struct trackDb *tdb, char *chrom, 
                                    int start, int end, char *name, char *mergeMode, char *foot)
/* Retrieve interact items at this position from track table */
// TODO: Support for mergeMode
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

struct interactPlusRow *iprs = NULL;
char **row;
int offset;
struct sqlResult *sr = hRangeQuery(conn, table, chrom, start, end, NULL, &offset);
int fieldCount = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct interact *inter = interactLoadAndValidate(row+offset);
    if (!name)
        {
        if (inter->chromStart != start || inter->chromEnd != end)
            continue;
        }
    else
        {
        if (differentString(inter->name, name))
            continue;
        }
    // got one, save object and row representation
    struct interactPlusRow *ipr;
    AllocVar(ipr);
    ipr->interact = inter;
    char **fieldVals;
    if (fieldCount == 0)
        fieldCount = sqlCountColumns(sr);
    AllocArray(fieldVals, fieldCount);
    int i;
    for (i = 0; i < fieldCount; i++)
        fieldVals[i] = cloneString(row[i]);
    ipr->row = fieldVals;
    slAddHead(&iprs, ipr);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return iprs;
}

static struct interactPlusRow *getInteractsFromFile(char *file, char *chrom, int start, int end, 
                                                        char *name, char *mergeMode, char *foot)
/* Retrieve interact items at this position or name from big file */
{
struct bbiFile *bbi = bigBedFileOpen(file);
struct lm *lm = lmInit(0);
struct bigBedInterval *bb, *bbList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
struct interactPlusRow *iprs = NULL;

for (bb = bbList; bb != NULL; bb = bb->next)
    {
    char startBuf[16], endBuf[16];
    int maxFields = 32;
    char *row[maxFields];      // big enough ?
    int fieldCount = bigBedIntervalToRow(bb, chrom, startBuf, endBuf, row, maxFields);
    struct interact *inter = interactLoadAndValidate(row);
    if (inter == NULL)
        continue;
    if (!name)
        {
        if (inter->chromStart != start || inter->chromEnd != end)
            continue;
        }
    else
        {
        char *match = inter->name;
        if (mergeMode)
            match = sameString(mergeMode, INTERACT_MERGE_SOURCE) ?
                                                inter->sourceName : inter->targetName;
        if (differentString(name, match))
            continue;
        }

    // got one, save object and row representation
    struct interactPlusRow *ipr;
    AllocVar(ipr);
    ipr->interact = inter;
    char **fieldVals;
    AllocArray(fieldVals, fieldCount);
    int i;
    for (i = 0; i < fieldCount; i++)
        fieldVals[i] = cloneString(row[i]);
    ipr->row = fieldVals;
    slAddHead(&iprs, ipr);
    }
return iprs;
}

static struct interactPlusRow *getInteractions(struct trackDb *tdb, char *chrom, 
                                                int start, int end, char *name, char *foot)
/* Retrieve interact items at this position or name. 
 * Also any others with the same endpoint, if endpoint clicked on*/
{
struct interactPlusRow *iprs = NULL;
char *file = trackDbSetting(tdb, "bigDataUrl");
char *mergeMode = interactUiMergeMode(cart, tdb->track, tdb);
if (file != NULL)
    iprs = getInteractsFromFile(file, chrom, start, end, name, mergeMode, foot);
else
    iprs = getInteractsFromTable(tdb, chrom, start, end, name, mergeMode, foot);

// TODO: add sort on score, or position for merged items ?
//slSort(&inters, bedCmpScore);
//slReverse(&inters);
return iprs;
}

void doInteractRegionDetails(struct trackDb *tdb, struct interact *inter)
{
/* print info for both regions */
/* Use different labels:
        1) directional (source/target)
        2) non-directional same chrom (lower/upper)
        3) non-directional other chrom (this/other)
*/
char startBuf[1024], endBuf[1024], sizeBuf[1024];
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
printf("<br>");
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
                region1Chrom, startBuf, endBuf, 
                inter->sourceStrand[0] == '.' ? "" : inter->sourceStrand);
printf("&nbsp;&nbsp;%s bp<br>\n", sizeBuf);

sprintLongWithCommas(startBuf, region2Start+1);
sprintLongWithCommas(endBuf, region2End);
sprintLongWithCommas(sizeBuf, region2End - region2Start);
printf("<b>%s region:</b> %s&nbsp;&nbsp;"
                "<a href='hgTracks?position=%s:%d-%d' target='_blank'>%s:%s-%s</a> %s",
                region2Label, region2Name, region2Chrom, region2Start+1, region2End,
                region2Chrom, startBuf, endBuf, 
                inter->targetStrand[0] == '.' ? "" : inter->targetStrand);
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

void doInteractItemDetails(struct trackDb *tdb, struct interactPlusRow *ipr, char *item, 
                                boolean isMultiple)
/* Details of interaction item */
{
struct interact *inter = ipr->interact;
struct slPair *fields = getFields(tdb, ipr->row);
printCustomUrlWithFields(tdb, item, item, TRUE, fields);
if (!isEmptyTextField(inter->name))
    printf("<b>Interaction:</b> %s<br>\n", inter->name);
printf("<b>Score:</b> %d<br>\n", inter->score);
printf("<b>Value:</b> %0.3f<br>\n", inter->value);
if (!isEmptyTextField(inter->exp))
    printf("<b>Experiment:</b> %s<br>\n", inter->exp);
puts("<p>");
if (!isMultiple)
    doInteractRegionDetails(tdb, inter);
}

void doInteractDetails(struct trackDb *tdb, char *item)
/* Details of interaction items */
{
char *chrom = cartString(cart, "c");
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
char *foot = cartOptionalString(cart, "foot");
struct interactPlusRow *iprs = getInteractions(tdb, chrom, start, end, item, foot);
if (iprs == NULL)
    errAbort("Can't find interaction %s", item ? item : "");
int count = slCount(iprs);
char *mergeMode = interactUiMergeMode(cart, tdb->track, tdb);
if (count > 1)
    {
    printf("<b>Interactions:</b> %d<p>", count);
    if (mergeMode)
        {
        char startBuf[1024], endBuf[1024], sizeBuf[1024];
        sprintLongWithCommas(startBuf, start + 1);
        sprintLongWithCommas(endBuf, end);
        sprintLongWithCommas(sizeBuf, end - start);
        printf("<b>%s interactions region:</b> &nbsp;&nbsp;"
                        "<a href='hgTracks?position=%s:%d-%d' target='_blank'>%s:%s-%s</a> ",
                        item, chrom, start+1, end, chrom, startBuf, endBuf);
        printf("&nbsp;&nbsp;%s bp<br>\n", sizeBuf);
        }
    else
        doInteractRegionDetails(tdb, iprs->interact);
    printf("</p>");
    }
genericHeader(tdb, item);
static struct interactPlusRow *ipr = NULL;
for (ipr = iprs; ipr != NULL; ipr = ipr->next)
    {
    if (count > 1)
        printf("<hr>\n");
    if (mergeMode)
        doInteractRegionDetails(tdb, ipr->interact);
    else
        doInteractItemDetails(tdb, ipr, item, count > 1);
    if (count > 1 && !isEmptyTextField(ipr->interact->name) && sameString(ipr->interact->name, item))
        printf("<hr>\n");
    }
if (count > 1 && mergeMode)
    printf("<hr>\n");
}


