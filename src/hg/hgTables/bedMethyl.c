/* bedMethyl - stuff to handle Hi-C stuff in table browser. */

/* Copyright (C) 2019 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
//#include "linefile.h"
//#include "dystring.h"
//#include "localmem.h"
#include "jksql.h"
#include "grp.h"
//#include "cheapcgi.h"
//#include "cart.h"
//#include "web.h"
////#include "bed.h"
//#include "hdb.h"
#include "trackDb.h"
//#include "obscure.h"
//#include "hmmstats.h"
//#include "correlate.h"
//#include "asParse.h"
//#include "bbiFile.h"
//#include "bigBed.h"
#include "hubConnect.h"
//#include "trackHub.h"
#include "hgTables.h"
//#include "asFilter.h"
//#include "xmlEscape.h"
#include "bedMethyl.h"
//#include "interact.h"
//#include "hgConfig.h"
//#include "chromAlias.h"

boolean isBedMethylTable(char *table)
/* Return TRUE if table corresponds to a bedMethyl file. */
{
if (isHubTrack(table))
    {
    struct trackDb *tdb = hashFindVal(fullTableToTdbHash, table);
    return sameWord("bedMethyl", tdb->type);
    }
else
    return trackIsType(database, table, curTrack, "bedMethyl", ctLookupName);
}

struct sqlFieldType *bedMethylListFieldsAndTypes()
/* Get fields of bedMethyl as list of sqlFieldType (again, this is really just the list of interact fields. */
{
struct asObject *as = bedMethylAsObj();
return sqlFieldTypesFromAs(as);
}
