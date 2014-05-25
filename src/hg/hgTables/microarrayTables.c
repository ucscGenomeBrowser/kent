/* Stuff to handle microarray tables */

/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "trackDb.h"
#include "customTrack.h"
#include "microarray.h"
#include "hgTables.h"

boolean isMicroarray(struct trackDb *parent, char *table)
/* Return TRUE if table is specified as a microarray in the current database's
 * trackDb. */
{
struct trackDb *tdb = findTdbForTable(database, parent, table, ctLookupName);
return (tdb && (startsWith("expRatio", tdb->type) || startsWith("array", tdb->type)));
}

void doOutMicroarrayNames(struct trackDb *tdb)
/* Show the microarray names from .ra file */
{
struct microarrayGroups *allGroups;
if (anyIntersection())
    errAbort("Can't do microarray names output when intersection is on. "
    "Please go back and select another output type (BED or custom track is good), or clear the intersection.");
allGroups = maGetTrackGroupings(database, tdb);
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
