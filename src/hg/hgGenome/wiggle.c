/* wiggle - stuff to process wiggle tracks. 
 * Much of this is lifted from hgText/hgWigText.c */

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
#include "bed.h"
#include "hdb.h"
#include "hui.h"
#include "hgColors.h"
#include "trackDb.h"
#include "customTrack.h"
#include "wiggle.h"
#include "correlate.h"

#include "hgGenome.h"

static char const rcsid[] = "$Id: wiggle.c,v 1.1.58.1 2008/07/31 02:24:05 markd Exp $";


boolean isWiggle(char *db, char *table)
/* Return TRUE if db.table is a wiggle. */
{
boolean typeWiggle = FALSE;

if (db != NULL && table != NULL)
    {
    if (isCustomTrack(table))
	{
	struct customTrack *ct = lookupCt(table);
	if (ct->wiggle)
	    typeWiggle = TRUE;
	}
    else
	{
	struct hTableInfo *hti = maybeGetHti(db, table);
	typeWiggle = (hti != NULL && HTI_IS_WIGGLE);
	}
    }
return(typeWiggle);
}


struct wiggleDataStream *wigChromRawStats(char *chrom)
/* Fetch stats for wig data in chrom.  
 * Returns a wiggleDataStream, free it with wiggleDataStreamFree() */
{
char splitTableOrFileName[256];
struct customTrack *ct = NULL;
boolean isCustom = FALSE;
struct wiggleDataStream *wds = NULL;
int operations = wigFetchRawStats;
char *table = curTable;

/* ct, isCustom, wds are set here */
if (isCustomTrack(table)) 
    { 
    ct = lookupCt(table); 
    isCustom = TRUE; 
    if (! ct->wiggle) 
	errAbort("called to work on a custom track '%s' that isn't wiggle data ?", table); 
 
    if (ct->dbTrack) 
	safef(splitTableOrFileName,ArraySize(splitTableOrFileName), "%s", 
		ct->dbTableName); 
    else 
	safef(splitTableOrFileName,ArraySize(splitTableOrFileName), "%s", 
		ct->wigFile); 
    } 

wds = wiggleDataStreamNew(); 

wds->setChromConstraint(wds, chrom);

if (isCustom)
    {
    if (ct->dbTrack)
	wds->getData(wds, CUSTOM_TRASH, splitTableOrFileName, operations);
    else
	wds->getData(wds, NULL, splitTableOrFileName, operations);
    }
else
    {
    boolean hasBin = FALSE;
    if (hFindSplitTable(database, chrom, table, splitTableOrFileName, &hasBin))
	{
	wds->getData(wds, database, splitTableOrFileName, operations);
	}
    }
return wds;
}



