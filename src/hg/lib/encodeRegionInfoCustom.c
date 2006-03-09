/* encodeRegionInfoCustom - custom (not autoSQL generated) code for working
 * with ENCODE region information table in hgFixed */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "encodeRegionInfo.h"

static char const rcsid[] = "$Id: encodeRegionInfoCustom.c,v 1.2 2006/03/09 18:26:57 angie Exp $";

char *getEncodeRegionDescr(char *name)
/* Get descriptive text for a region */
{

    struct sqlConnection *conn = sqlConnect("hgFixed");
    char buf[128];
    char query[256];
    char *res = NULL;
    safef(query, sizeof(query), 
	  "select descr from encodeRegionInfo where name = '%s'", name);
    if (sqlQuickQuery(conn, query, buf, sizeof(buf)) != NULL)
            {
            res = cloneString(buf);
            }
    sqlDisconnect(&conn);
    return res;
}
