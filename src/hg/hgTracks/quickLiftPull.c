
// Code to parse list of exported data track hubs from table and print as browser dialog
//      for client dialog (js)
//
/* Copyright (C) 2024 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "dystring.h"
#include "hCommon.h"
#include "hgConfig.h"
#include "htmshell.h"
#include "hash.h"
#include "web.h"
#include "ra.h"
#include "hgTracks.h"
#include "hgFind.h"
#include "obscure.h"
#include "net.h"
#include "hubConnect.h"
#include "trackHub.h"
#include "exportedDataHubs.h"

boolean quickLiftPullEnabled()
{
return TRUE;
}

void quickLiftPull(char *db)
/* Fill out exported data hubs popup. */
{
if (!quickLiftPullEnabled())
    return;

hPrintf("<div style='display:none;' id='quickLiftPullPopup' title='QuickLift One Track'>\n");
hPrintf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=%s>\n", hgTracksName(),
	cartUsualString(cart, "formMethod", "POST"));
hPrintf("<br>Choose an assembly:<div> <input id=\"speciesSearch\" value=\"\" size=42> </div>\n");
hPrintf("<br>Choose a track:<div> <input id=\"trackSearch\" value=\"\" size=42> </div>\n");
cgiMakeButton("topSubmit", "Submit");
hPrintf("</div>");
#ifdef NOTNOW
struct sqlConnection *conn = hConnectCentral();
char **row;
char query[2048];
sqlSafef(query, sizeof(query), "select x.id,q.id,x.db,x.label,x.description,x.path,q.path from quickLiftChain q,exportedDataHubs x where q.fromDb=x.db and q.toDb='%s' and x.label not like 'Private';", trackHubSkipHubName(db));

hPrintf("<table style=\"border: 1px solid black\">\n");
struct sqlResult *sr;
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hPrintf("<tr><td>%s</td><td><A HREF=\"./hgTracks?quickLift.%s.%s=%s&%s=on&hgsid=%s\">%s</A></td><td>%s</td></tr>",row[2], row[0],trackHubSkipHubName(db), row[1], hgHubConnectRemakeTrackHub,cartSessionId(cart),  row[3],row[4]);
    }
hPrintf("</table>\n");
hPrintf("</div>\n");
hDisconnectCentral(&conn);
#endif
hPrintf("</div>\n");
}
