/* Details page for interact type tracks */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "obscure.h"
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

sprintLongWithCommas(startBuf, inter->sourceStart+1);
sprintLongWithCommas(endBuf, inter->sourceEnd);
printf("<b>Source region:</b> %s&nbsp;&nbsp;"
                "<a href='hgTracks?position=%s:%d-%d' target='_blank'>%s:%s-%s</a><br>\n", 
                inter->sourceName, inter->sourceChrom, inter->sourceStart+1, inter->sourceEnd,
                inter->sourceChrom, startBuf, endBuf);
sprintLongWithCommas(startBuf, inter->targetStart+1);
sprintLongWithCommas(endBuf, inter->targetEnd);
printf("<b>Target region:</b> %s&nbsp;&nbsp;"
                "<a href='hgTracks?position=%s:%d-%d' target='_blank'>%s:%s-%s</a><br>\n", 
                inter->targetName, inter->targetChrom, inter->targetStart+1, inter->targetEnd,
                inter->targetChrom, startBuf, endBuf);
#ifdef TODO
/* TODO: get count and score stats of all interactions in window */
double *scores;
AllocArray(scores, count);
#endif
}

