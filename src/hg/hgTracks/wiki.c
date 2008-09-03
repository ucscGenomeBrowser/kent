#include "common.h"
#include "jksql.h"
#include "hgTracks.h"
#include "rnaPLFoldTrack.h"

/* ------ BEGIN wiki ------ */

/*******************************************************************/

#define MAX_WIKI_NAME 80

void loadWiki(struct track *tg);


void loadWiki(struct track *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct bed *bed, *list = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, 6);
    freeMem(bed->name);
    bed->name = malloc(MAX_WIKI_NAME);
    snprintf(bed->name, MAX_WIKI_NAME, "%d-%d:%s", bed->chromStart, bed->chromEnd, bed->strand);
    slAddHead(&list, bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
//slReverse(&list);

// add dummy links
AllocVar(bed);
bed->chrom = chromName;
bed->chromStart = winStart;
bed->chromEnd = winEnd;
bed->name = "Make new entry:+";
bed->strand[0] = '+';
bed->score = 100;
slAddHead(&list, bed);

AllocVar(bed);
bed->chrom = chromName;
bed->chromStart = winStart;
bed->chromEnd = winEnd;
bed->name = "Make new entry:-";
bed->strand[0] = '-';
bed->score = 100;
slAddHead(&list, bed);

tg->items = list;
}


void wikiMethods(struct track *tg)
{
  //linkedFeaturesMethods(tg);
tg->loadItems = loadWiki;
//tg->colorShades = shadesOfGray;
//tg->drawItemAt = tigrOperonDrawAt;
}

/**** End of Lowe lab additions ****/
