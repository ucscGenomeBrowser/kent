#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "trackDb.h"
#include "grp.h"
#include "joiner.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: schema.c,v 1.1 2004/07/13 22:40:51 kent Exp $";

void doOutSchema(struct trackDb *track, struct sqlConnection *conn)
/* Show page that describes schema. */
{
struct joiner *joiner = joinerRead("all.joiner");
struct hTableInfo *hti = hFindTableInfo(NULL, track->tableName);
char *table = tableForTrack(track);
struct joinerPair *jpList, *jp;
htmlOpen("Schema %s - %s", track->shortLabel, track->longLabel);
hPrintf("<B>Primary Table:</B> %s\n", track->tableName);
if (hti != NULL && hti->isSplit)
    {
    hPrintf(" (split across chromosomes)");
    }
hPrintf("<BR>\n");
jpList = joinerRelate(joiner, database, table);
if (jpList != NULL)
    {
    hPrintf("<B>Connected Tables and Joining Fields:</B><BR>\n");
    for (jp = jpList; jp != NULL; jp = jp->next)
	{
	hPrintSpaces(6);
	hPrintf("%s.<B>%s</B>.%s (via %s.%s.<B>%s</B> field)<BR>\n", 
	    jp->b->database, jp->b->table, jp->b->field,
	    jp->a->database, jp->a->table, jp->a->field);
	}
    }
htmlClose();
}

