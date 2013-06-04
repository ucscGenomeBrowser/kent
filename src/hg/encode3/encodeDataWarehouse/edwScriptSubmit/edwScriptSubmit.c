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

void accessDenied()
/* Sleep a bit and then deny access. */
{
sleep(5);
errAbort("Access denied!");
}

void doMiddle()
/* edwScriptSubmit - Handle submission by a script via web services.. */
{
/* Security check - make sure we are on https, and check user/password. */
if (!cgiServerHttpsIsOn())
     usage();
char *user = cgiString("user");
char *password = cgiString("password");
struct sqlConnection *conn = edwConnectReadWrite();
char query[256];
sqlSafef(query, sizeof(query), "select * from edwScriptRegistry where name='%s'", user);
struct edwScriptRegistry *reg = edwScriptRegistryLoadByQuery(conn, query);
if (reg == NULL)
    accessDenied();
char key[EDW_SID_SIZE];
edwMakeSid(password, key);
if (!sameString(reg->secretHash, key))
    accessDenied();

/* Get email associated with script. */
sqlSafef(query, sizeof(query), "select email from edwUser where id=%d", reg->userId);
char *email = sqlNeedQuickString(conn, query);

/* Add submission URL to the queue. */
char *url = cgiString("url");
sqlSafef(query, sizeof(query), "update edwScriptRegistry set submitCount = submitCount+1 "
    "where id=%d", reg->id);
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
