/* Stuff to handle microarray tables */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "trackDb.h"
#include "customTrack.h"
#include "microarray.h"
#include "hgTables.h"

boolean isMicroarray(struct trackDb *curTrack, char *table)
/* Return TRUE if table is specified as a microarray in the current database's 
 * trackDb. */
{
if (curTrack && sameString(curTrack->tableName, table))
    return (startsWith("expRatio", curTrack->type) || startsWith("array", curTrack->type));
else
    {
    struct trackDb *tdb = hTrackDbForTrack(database, table);
    return (tdb && (startsWith("expRatio", tdb->type) || startsWith("array", tdb->type)));
    }
return FALSE;
}

void doOutMicroarrayNames(struct trackDb *tdb)
/* Show the microarray names from .ra file */
{
struct microarrayGroups *allGroups = maGetTrackGroupings(database, tdb);
if (allGroups)
    {
    struct maGrouping *allArrays = allGroups->allArrays;
    int i;
    if (allArrays)
	{
	textOpen();
	printf("#expId\tname\n");
	for (i = 0; i < allArrays->size; i++)
	    {
	    printf("%d\t%s\n", allArrays->expIds[i], allArrays->names[i]);
	    }
	}
    }
}
