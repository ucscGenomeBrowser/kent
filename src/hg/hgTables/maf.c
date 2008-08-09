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

static char const rcsid[] = "$Id: maf.c,v 1.14.4.2 2008/08/09 04:40:32 markd Exp $";

boolean isMafTable(char *database, struct trackDb *track, char *table)
/* Return TRUE if table is maf. */
{
char setting[128], *p = setting;

if (track == NULL)
    return FALSE;
if (isEmpty(track->type))
    return FALSE;
safecpy(setting, sizeof setting, track->type);
char *type = nextWord(&p);

if (sameString(type, "maf") || sameString(type, "wigMaf"))
    if (sameString(track->tableName, table))
        return TRUE;
return FALSE;
}

void doOutMaf(struct trackDb *track, char *table, struct sqlConnection *conn)
/* Output regions as MAF.  maf tables look bed-like enough for 
 * cookedBedsOnRegions to handle intersections. */
{
struct region *region = NULL, *regionList = getRegions();
struct lm *lm = lmInit(64*1024);
textOpen();

struct sqlConnection *ctConn = NULL;
struct sqlConnection *ctConn2 = NULL;
struct customTrack *ct = NULL;
char *mafFile = NULL;

if (isCustomTrack(table))
    {
    ctConn = hAllocConn(CUSTOM_TRASH);
    ctConn2 = hAllocConn(CUSTOM_TRASH);
    ct = lookupCt(table);
    struct hash *settings = track->settingsHash;
    if ((mafFile = hashFindVal(settings, "mafFile")) == NULL)
	{
	/* this shouldn't happen */
	printf("cannot find custom track file %s\n", mafFile);
	return;
	}
    }

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
	if (ct == NULL)
	    mafList = mafLoadInRegion(conn, table, bed->chrom, 
				      bed->chromStart, bed->chromEnd);
	else
	    mafList = mafLoadInRegion2(ctConn, ctConn2, ct->dbTableName, 
		    bed->chrom, bed->chromStart, bed->chromEnd, mafFile);
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

if (isCustomTrack(table))
    {
    hFreeConn(&ctConn);
    hFreeConn(&ctConn2);
    }
}
