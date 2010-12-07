/* hubConnect - stuff to manage connections to track hubs.  Most of this is mediated through
 * the hubConnect table in the hgCentral database.  Here there are routines to translate between
 * hub symbolic names and hub URLs,  to see if a hub is up or down or sideways (up but badly
 * formatted) etc. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "trackHub.h"
#include "hubConnect.h"



boolean hubConnectTableExists()
/* Return TRUE if the hubConnect table exists. */
{
struct sqlConnection *conn = hConnectCentral();
boolean exists = sqlTableExists(conn, hubConnectTableName);
hDisconnectCentral(&conn);
return exists;
}

