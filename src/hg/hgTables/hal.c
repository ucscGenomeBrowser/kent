/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "cart.h"
#include "jksql.h"
#include "grp.h"
#include "hubConnect.h"
#include "hdb.h"
#include "maf.h"
#include "hgTables.h"
#include "trackHub.h"
#ifdef USE_HAL
#include "halBlockViz.h"
#endif // USE_HAL

extern struct trackDb *curTrack;	/* Currently selected track. */
extern char *database;
extern struct hash *fullTableToTdbHash;        /* All tracks and subtracks keyed by tdb->table field. */

boolean isHalTable(char *table)
/* Return TRUE if table corresponds to a HAL file. */
{
if (isHubTrack(table))
    {
    struct trackDb *tdb = hashFindVal(fullTableToTdbHash, table);
    return startsWithWord("halSnake", tdb->type);
    }
else
    return trackIsType(database, table, curTrack, "halSnake", ctLookupName);
}

struct trackDb *findTrackDb(struct trackDb *trackDb, char *table)
// find a child of a composite that corresponds to this table
{
if (sameString(trackDb->table, table))
    return trackDb;

struct trackDb *subTrack = trackDb->subtracks;
struct trackDb *childTrackDb;
for(; subTrack; subTrack = subTrack->next)
    {
    if ((childTrackDb = findTrackDb(subTrack, table)) != NULL)
	return childTrackDb;
    }
return NULL;
}

void doHalMaf(struct trackDb *parentTrack, char *table, struct sqlConnection *conn)
/* Output regions as MAF.  maf tables look bed-like enough for
 * cookedBedsOnRegions to handle intersections. */
{
#ifdef USE_HAL
struct region *region = NULL, *regionList = getRegions();
struct trackDb *tdb;

if ((tdb = findTrackDb(parentTrack, table)) == NULL)
    errAbort("cannot find track named %s under %s\n", table, parentTrack->table);
char *fileName = trackDbSetting(tdb, "bigDataUrl");
char *otherSpecies = trackDbSetting(tdb, "otherSpecies");
char *errString;
int handle = halOpenLOD(fileName, &errString);
if (handle < 0)
    errAbort("HAL open error: %s\n", errString);

struct hal_species_t *speciesList = halGetSpecies(handle, &errString);

if (speciesList == NULL)
    errAbort("HAL get species error: %s\n", errString);

for(; speciesList; speciesList = speciesList->next)
    {
    if  (sameString(speciesList->name, otherSpecies))
	break;
    }
if (speciesList == NULL)
    errAbort("cannot find species %s in hal file %s\n",
	otherSpecies, fileName);

speciesList->next = NULL;

textOpen();

for (region = regionList; region != NULL; region = region->next)
    {
    if (halGetMAF(stdout, handle, speciesList,
		trackHubSkipHubName(database), region->chrom,
		region->start, region->end, FALSE, &errString) < 0 )
	errAbort("HAL get MAF error: %s\n", errString);

    }
#else // USE_HAL
errAbort("hgTables not compiled with HAL support.");
#endif // USE_HAL
}
