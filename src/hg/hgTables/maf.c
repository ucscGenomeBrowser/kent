/* maf - stuff to process maf tracks.  */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
#include "hubConnect.h"


boolean isMafTable(char *database, struct trackDb *track, char *table)
/* Return TRUE if table is maf. */
{
if (track == NULL)
    return FALSE;
if (isEmpty(track->type))
    return FALSE;

if (sameString(track->table, table))
    {
    if (startsWithWord("maf",track->type) || startsWithWord("wigMaf",track->type) || startsWithWord("bigMaf",track->type))
        return TRUE;
    }
else
    {
    struct slRef *tdbRefList = trackDbListGetRefsToDescendantLeaves(track->subtracks);
    struct slRef *tdbRef;
    for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
        {
	struct trackDb *childTdb = tdbRef->val;
        if(sameString(childTdb->table, table))
            {
            if (startsWithWord("maf",childTdb->type) || startsWithWord("wigMaf",childTdb->type))
                return TRUE;
            break;
            }
        }
    slFreeList(&tdbRefList);
    }
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
struct hash *settings = track->settingsHash;
char *mafFile = hashFindVal(settings, "mafFile");

if (isCustomTrack(table))
    {
    ctConn = hAllocConn(CUSTOM_TRASH);
    ctConn2 = hAllocConn(CUSTOM_TRASH);
    ct = ctLookupName(table);
    if (mafFile == NULL)
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
	safef(dbChrom, sizeof(dbChrom), "%s.%s", hubConnectSkipHubPrefix(database), bed->chrom);
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
	    {
            if (isBigBed(database, table, curTrack, ctLookupName))
                {
                char *fileName = trackDbSetting(track, "bigDataUrl");
                struct bbiFile *bbi = bigBedFileOpen(fileName);
                mafList = bigMafLoadInRegion(bbi, bed->chrom, bed->chromStart, bed->chromEnd);
                }
	    else if (mafFile != NULL)
		mafList = mafLoadInRegion2(conn, conn, table,
			bed->chrom, bed->chromStart, bed->chromEnd, mafFile);
	    else
		mafList = mafLoadInRegion(conn, table, bed->chrom,
					  bed->chromStart, bed->chromEnd);
	    }
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
