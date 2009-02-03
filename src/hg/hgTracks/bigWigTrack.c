/* bigWigTrack - stuff to handle loading and display of bigWig type tracks in browser.   */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
// #include "wiggle.h"
// #include "scoredRef.h"
// #include "customTrack.h"
#include "localmem.h"
#include "wigCommon.h"
#include "bbiFile.h"
#include "bigWig.h"

static char const rcsid[] = "$Id: bigWigTrack.c,v 1.1 2009/02/03 02:14:57 kent Exp $";

static void bigWigLoadItems(struct track *tg)
/* Fill up tg->items with bedGraphItems derived from a bigWig file */
{
/* Figure out bigWig file name. */
struct sqlConnection *conn = hAllocConn(database);
char query[256];
safef(query, sizeof(query), "select fileName from %s", tg->mapName);
char *wigFileName = sqlQuickString(conn, query);
if (wigFileName == NULL)
    errAbort("Missing fileName in %s table", tg->mapName);
hFreeConn(&conn);

/* Get interval list from bigWig. */
struct lm *lm = lmInit(0);
struct bbiFile *bwf = bigWigFileOpen(wigFileName);
struct bbiInterval *bi, *biList = bigWigIntervalQuery(bwf, chromName, winStart, winEnd, lm);

/* Convert from bigWig's interval structure to bedGraphItem. */
struct bedGraphItem *bgList = NULL;
for (bi = biList; bi != NULL; bi = bi->next)
    {
    struct bedGraphItem *bg;
    AllocVar(bg);
    bg->start = bi->start;
    bg->end = bi->end;
    bg->dataValue = bi->val;
    slAddHead(&bgList, bg);
    }
slReverse(&bgList);

/* Clean up and go home. */
bbiFileClose(&bwf);
lmCleanup(&lm);
tg->items = bgList;
}

void bigWigMethods(struct track *track, struct trackDb *tdb, 
	int wordCount, char *words[])
/* Set up bigWig methods. */
{
bedGraphMethods(track, tdb, wordCount, words);
track->loadItems = bigWigLoadItems;
}
