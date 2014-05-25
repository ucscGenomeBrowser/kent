/* edwScriptSubmit - Handle submission by a script via web services.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jksql.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwScriptSubmit - Web services CGI for script-driven submission to ENCODE Data Warehouse"
  "This program is meant to be called as a CGI from a web server using HTTPS.");
}

void doMiddle()
/* edwScriptSubmit - Handle submission by a script via web services.. */
{
if (!cgiServerHttpsIsOn())
     usage();
struct edwScriptRegistry *reg = edwScriptRegistryFromCgi();
struct sqlConnection *conn = edwConnectReadWrite();

/* Get email associated with script. */
char query[256];
safef(query, sizeof(query), "select email from edwUser where id=%d", reg->userId);
sqlSafef(query, sizeof(query), "select email from edwUser where id=%d", reg->userId);
char *email = sqlNeedQuickString(conn, query);

/* Add submission URL to the queue. */
char *url = cgiString("url");
sqlSafef(query, sizeof(query), "update edwScriptRegistry set submitCount = submitCount+1 "
    "where id=%d", reg->id);
sqlUpdate(conn, query);
edwAddSubmitJob(conn, email, url, FALSE);
printf("%s submitted", url);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (!cgiIsOnWeb())
    usage();
htmShell("Web script submit", doMiddle, NULL);
return 0;
}
