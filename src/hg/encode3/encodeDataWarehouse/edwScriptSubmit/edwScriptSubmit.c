/* edwScriptSubmit - Handle submission by a script via web services.. */
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
  "This program is meant to be called as a CGI from a web server using https.");
}

void doMiddle()
/* edwScriptSubmit - Handle submission by a script via web services.. */
{
if (!cgiServerHttpsIsOn())
     usage();
char *email = sqlEscapeString(cgiString("email"));
char *script = sqlEscapeString(cgiString("script"));
char *access = sqlEscapeString(cgiString("access"));
char *url = cgiString("url");
struct sqlConnection *conn = edwConnectReadWrite();
char query[256];
safef(query, sizeof(query), "select id from edwUser where email='%s'", email);
int userId = sqlQuickNum(conn, query);
if (userId <= 0)
    errAbort("Access denied");
safef(query, sizeof(query), "select * from edwScriptRegistry where userId=%d and name='%s'",
    userId, script);
struct edwScriptRegistry *reg = edwScriptRegistryLoadByQuery(conn, query);
if (reg == NULL)
    errAbort("%s does not have a registered script named %s", email, script);
char key[EDW_SID_SIZE];
edwMakeSid(access, key);
if (!sameString(reg->secretHash, key))
    errAbort("Access denied");
safef(query, sizeof(query), 
    "update edwScriptRegistry set submitCount = submitCount+1 "
    "where userId=%d and name='%s'", userId, script);
sqlUpdate(conn, query);
edwAddSubmitJob(conn, email, url);
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
