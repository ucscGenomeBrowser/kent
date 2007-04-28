/* maf - stuff to process maf tracks.  */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "portable.h"
#include "obscure.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "cart.h"
#include "web.h"
#include "trackDb.h"
#include "maf.h"
#include "hgMaf.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: maf.c,v 1.9 2007/04/28 23:59:41 kate Exp $";

boolean isMafTable(char *database, struct trackDb *track, char *table)
/* Return TRUE if table is maf. */
{
if (track == NULL)
    return FALSE;
else
    {
    char *typeDupe = cloneString(track->type);
    char *s = typeDupe;
    char *type = nextWord(&s);
    char *summary = trackDbSetting(track, "summary");
    struct consWiggle *wig, *wiggles = wigMafWiggles(track);
    boolean retVal = FALSE;
    if (type != NULL)
	retVal = (sameString(type, "maf") || sameString(type, "wigMaf"));
    if (summary && sameString(summary, table))
        retVal = FALSE;
    for (wig = wiggles; wig != NULL; wig = wig->next)
	if (sameString(wig->table, table))
            retVal = FALSE;
    freeMem(typeDupe);
    return retVal;
    }
}

void doOutMaf(struct trackDb *track, char *table, struct sqlConnection *conn)
/* Output regions as MAF.  maf tables look bed-like enough for 
 * cookedBedsOnRegions to handle intersections. */
{
struct region *region = NULL, *regionList = getRegions();
struct lm *lm = lmInit(64*1024);
textOpen();
mafWriteStart(stdout, NULL);
for (region = regionList; region != NULL; region = region->next)
    {
    struct bed *bedList = cookedBedList(conn, table, region, lm, NULL);
    struct bed *bed = NULL;
    for (bed = bedList;  bed != NULL;  bed = bed->next)
	{
	struct mafAli *mafList = NULL, *maf = NULL;
	char dbChrom[64];
	safef(dbChrom, sizeof(dbChrom), "%s.%s", database, bed->chrom);
	/* For MAF, we clip to viewing window (region) instead of showing 
	 * entire items that happen to overlap the window/region, which is 
	 * what we get from cookedBedList. */
	if (bed->chromStart < region->start)
	    bed->chromStart = region->start;
	if (bed->chromEnd > region->end)
	    bed->chromEnd = region->end;
	if (bed->chromStart >= bed->chromEnd)
	    continue;
	mafList = mafLoadInRegion(conn, table, bed->chrom, 
				  bed->chromStart, bed->chromEnd);
	for (maf = mafList; maf != NULL; maf = maf->next)
	    {
	    struct mafAli *subset = mafSubset(maf, dbChrom, 
					      bed->chromStart, bed->chromEnd);
	    if (subset != NULL)
		{
		subset->score = mafScoreMultiz(subset);
		mafWrite(stdout, subset);
		mafAliFree(&subset);
		}
	    }
	mafAliFreeList(&mafList);
	}
    }
mafWriteEnd(stdout);
lmCleanup(&lm);
}
