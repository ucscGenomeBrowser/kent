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

static char const rcsid[] = "$Id: maf.c,v 1.1 2004/09/24 05:34:01 kent Exp $";

boolean isMafTable(char *database, struct trackDb *track, char *table)
/* Return TRUE if table is maf. */
{
char *typeDupe = cloneString(track->type);
char *s = typeDupe;
char *type = nextWord(&s);
boolean retVal = FALSE;
if (type != NULL)
    retVal = (sameString(type, "maf") || sameString(type, "wigMaf"));
freeMem(typeDupe);
return retVal;
}

void doOutMaf(struct trackDb *track, char *table, struct sqlConnection *conn)
/* Output regions as MAF. */
{
struct region *region, *regionList = getRegions();
textOpen();
mafWriteStart(stdout, NULL);
for (region = regionList; region != NULL; region = region->next)
    {
    struct mafAli *mafList, *maf;
    char dbChrom[64];
    safef(dbChrom, sizeof(dbChrom), "%s.%s", database, region->chrom);
    mafList = mafLoadInRegion(conn, table, region->chrom, 
    	region->start, region->end);
    for (maf = mafList; maf != NULL; maf = maf->next)
        {
	struct mafAli *subset = mafSubset(maf, dbChrom, 
	    region->start, region->end);
	if (subset != NULL)
	    {
	    subset->score = mafScoreMultiz(subset);
	    mafWrite(stdout, subset);
	    }
	}
    }
mafWriteEnd(stdout);
}
