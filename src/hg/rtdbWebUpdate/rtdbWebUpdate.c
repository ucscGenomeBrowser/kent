/* rtdbWebUpdate - CGI program for remote triggering of MGC RTDB update. */

#include "common.h"
#include "errabort.h"
#include "hCommon.h"
#include "jksql.h"
#include "portable.h"
#include "linefile.h"
#include "dnautil.h"
#include "net.h"
#include "fa.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hui.h"
#include "cart.h"
#include "web.h"
#include "hash.h"
#include "hgConfig.h"

/* Global Variables */
struct cart *cart;	        /* CGI and other variables */
struct hash *oldCart = NULL;

void makeForm(struct slName *dbs)
/* If the button wasn't pressed already, show it. */
{
struct slName *cur;
cgiParagraph("Pressing the button below will trigger an update to the MGC RTDB database:");
/* HTML form */
puts("<FORM ACTION=\"../cgi-bin/rtdbWebUpdate\" METHOD=\"POST\" "
       " ENCTYPE=\"multipart/form-data\" NAME=\"mainForm\">\n");
cartSaveSession(cart);
for (cur = dbs; cur != NULL; cur = cur->next)
    {
    cgiMakeRadioButton("db", cur->name, FALSE);
    printf("&nbsp;%s\n<BR>\n", cur->name);
    }
puts("<BR>\n");
cgiMakeButton("RTDBSubmit","Update RTDB");
cartSaveSession(cart);
puts("</FORM>");
}

int updateServer(char *hostName, char *portName, char *database)
/* Send status message to server arnd report result. */
{
char buf[256];
int sd = 0;
int ret = 0;
/* Put together command. */
sd = netConnect(hostName, atoi(portName));
if (sd >= 0)
    {
    safef(buf, ArraySize(buf), "update.%s", database);
    netSendString(sd, buf);
    for (;;)
	{
	if (netGetString(sd, buf) == NULL)
	    {
	    printf("Error reading status information from %s:%s",hostName,portName);
	    ret = -1;
	    break;
	    }
	if (sameString(buf, "$end$"))
	    break;
	else
	    printf("%s\n", buf);
	}
    close(sd);
    }
else
    {
    ret = -1;
    printf("Couldn't connect");
    }
return(ret); 
}

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
char *database = cgiOptionalString("db");
char *rtdbServer = cfgOption("rtdb.server");
char *rtdbPort = cfgOption("rtdb.port");
char *rtdbChoices = cfgOption("rtdb.databases");
struct slName *dbs = slNameListFromComma(rtdbChoices);
cart = theCart;
cartWebStart(cart, database, "MGC RTDB Update");
if (!rtdbServer)
    errAbort("rtdb.update not defined in the hg.conf file. "
	     "Chances are this CGI isn't meant for this machine.");
if (!rtdbPort)
    errAbort("rtdb.update not defined in the hg.conf file. "
	     "Chances are this CGI isn't meant for this machine.");
/* create HMTL form if button wasn't pressed.  Otherwise, run the update */
if (!cgiVarExists("RTDBSubmit"))
    makeForm(dbs);
else if ((database == NULL) || (!slNameInList(dbs, database)))
    {
    makeForm(dbs);
    printf("<br>Error: Select one of databases listed.");
    }
else
    updateServer(rtdbServer, rtdbPort, database);
cartWebEnd();
slNameFreeList(&dbs);
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
oldCart = hashNew(8);
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldCart);
return 0;
}
