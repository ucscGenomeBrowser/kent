/* bedList - get list of beds in region that pass filtering. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "jksql.h"
#include "trackDb.h"
#include "customTrack.h"
#include "hdb.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: bedList.c,v 1.1 2004/07/20 05:58:19 kent Exp $";

struct bed *dbGetFilteredBeds(struct sqlConnection *conn, 
	struct trackDb *track)
/* getBed - get list of beds from database in region that pass filtering. */
{
struct region *region;
struct bed *bedList = NULL;
char *table = track->tableName;
char *filter = filterClause(database, table);

for (region = getRegions(); region != NULL; region = region->next)
    {
    int end = region->end;
    if (end == 0)
         end = hChromSize(region->chrom);
    struct bed *bedListRegion = hGetBedRangeDb(database, table, 
    	region->chrom, region->start, end, filter);
    bedList = slCat(bedList, bedListRegion);
    }
freez(&filter);
return bedList;
}

struct bed *getFilteredBedsInRegion(struct sqlConnection *conn, 
	struct trackDb *track)
/* getBed - get list of beds in region that pass filtering. */
{
if (isCustomTrack(track->tableName))
    return customTrackGetFilteredBeds(track->tableName, NULL, NULL);
else
    return dbGetFilteredBeds(conn, track);
}

void doOutBed(struct trackDb *track, struct sqlConnection *conn)
/* Output selected regions as bed. */
{
struct bed *bedList = getFilteredBedsInRegion(conn, track);
textOpen();
uglyf("Got %d beds, size unknown\n", slCount(bedList));
}
