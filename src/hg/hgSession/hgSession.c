/* hgSession - Interface with wiki login and do session saving/loading. */

#include "common.h"
#include "hash.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "linefile.h"
#include "net.h"
#include "textOut.h"
#include "hui.h"
#include "cart.h"
#include "web.h"
#include "hdb.h"
#include "wikiLink.h"
#include "hgSession.h"

static char const rcsid[] = "$Id: hgSession.c,v 1.1 2006/04/26 18:12:46 angie Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSession - Interface with wiki login and do session saving/loading.\n"
  "usage:\n"
  "    hgSession <various CGI settings>\n"
  );
}

/* Global variables. */
struct cart *cart;
char *excludeVars[] = {"Submit", "submit", NULL};

#define namedSessionTable "namedSessionDb"


void welcomeUser(char *wikiUserName)
/* Tell the user they are not logged in to the wiki and tell them how to 
 * do so. */
{
char *wikiHost = wikiLinkHost();

cartWebStart(cart, "Welcome %s", wikiUserName);
printf("If you are not %s (on the wiki at "
       "<A HREF=\"http://%s/\" TARGET=_BLANK>%s</A>) "
       "and would like to log out or change identity, \n",
       wikiUserName, wikiHost, wikiHost);
printf("<UL><LI>\n");
printf("<A HREF=\"%s\" TARGET=_BLANK><B>click here to log out</B></A>\n",
       wikiLinkUserLogoutUrl());
printf("in a new window (and if desired, to log in as somebody else).  ");
printf("</LI><LI>\n");
printf("When that is complete, return to this window and " 
       "<A HREF=\"/cgi-bin/hgSession?%s\"><B>click here to refresh</B></A>.\n",
       cartSidUrlString(cart));
printf("</LI></UL>\n");
}

void offerLogin()
/* Tell the user they are not logged in to the wiki and tell them how to 
 * do so. */
{
char *wikiHost = wikiLinkHost();

cartWebStart(cart, "Log in to UCSC Genome Bioinformatics");
printf("Logging in enables you to save current settings into a "
       "named session, and then restore settings from the session later.\n"
       "If you wish, you can share named sessions with other users.\n"
       "The wiki also serves as a forum for users "
       "to share knowledge and ideas.\n");
printf("<P>The log in page is handled by our "
       "<A HREF=\"http://%s/\" TARGET=_BLANK>wiki system</A>:\n", wikiHost);
printf("<UL><LI>\n");
printf("<A HREF=\"%s\" TARGET=_BLANK><B>click here to log in</B></A>\n",
       wikiLinkUserLoginUrl());
printf("in a new window.  ");
printf("</LI><LI>\n");
printf("When that is complete, return to this window and "
       "<A HREF=\"/cgi-bin/hgSession?%s\"><B>click here to refresh</B></A>.\n",
       cartSidUrlString(cart));
printf("</LI></UL></P>\n");
}

void showCartLinks()
/* Print out links to cartDump and cartReset. */
{
char *session = cartSidUrlString(cart);
char returnAddress[512];

printf("<P><A HREF=\"/cgi-bin/cartDump?%s\" TARGET=_BLANK>"
       "View/edit current settings</A>&nbsp;&nbsp;&nbsp;\n",
       session);

safef(returnAddress, sizeof(returnAddress), "/cgi-bin/hgSession?%s", session);
printf("<A HREF=\"/cgi-bin/cartReset?%s&destination=%s\">Reset session</A>"
       "</P>\n",
       session, cgiEncode(returnAddress));
}

void showNewSessionOptions()
/* Print out inputs for saving current settings to a new session. */
{
printf("<P>Save current settings as named session:<BR>\n"
       "&nbsp;&nbsp;&nbsp;Name: \n");
cgiMakeTextVar(hgsNewSessionName,
	       cartUsualString(cart, hgsNewSessionName, "session1"),
	       20);
printf("&nbsp;&nbsp;&nbsp;");
cgiMakeCheckBox(hgsNewSessionShare,
		cartUsualBoolean(cart, hgsNewSessionShare, TRUE));
printf("Allow this session to be loaded by others\n");
printf("&nbsp;&nbsp;&nbsp;");
cgiMakeButton(hgsDoNewSession, "submit");
printf("</P>\n");
}

void showExistingSessions(char *userName)
/* Print out a table with buttons for sharing/unsharing/loading/deleting 
 * previously saved sessions. */
{
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr = NULL;
char **row = NULL;
char query[512];
boolean foundAny = FALSE;

printf("<P>Previously saved sessions:<BR>\n");
printf("<TABLE BORDERWIDTH=0>\n");
safef(query, sizeof(query), "SELECT sessionName, shared from %s "
      "WHERE userName = '%s';",
      namedSessionTable, userName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *sessionName = row[0];
    boolean shared = atoi(row[1]);
    char buf[512];
    printf("<TR><TD>&nbsp;</TD><TD>%s</TD><TD>", sessionName);
    if (shared)
	{
	safef(buf, sizeof(buf), "%s%s", hgsUnsharePrefix, sessionName);
	cgiMakeButton(buf, "don't share");
	}
    else
	{
	safef(buf, sizeof(buf), "%s%s", hgsSharePrefix, sessionName);
	cgiMakeButton(buf, "share");
	}
    printf("</TD><TD>");
    safef(buf, sizeof(buf), "%s%s", hgsLoadPrefix, sessionName);
    cgiMakeButton(buf, "load into current session");
    printf("</TD><TD>");
    safef(buf, sizeof(buf), "%s%s", hgsDeletePrefix, sessionName);
    cgiMakeButton(buf, "delete");
    printf("</TD></TR>\n");
    foundAny = TRUE;
    }
printf("</TABLE>\n");
if (!foundAny)
    printf("&nbsp;&nbsp;&nbsp;(none)\n");
printf("</P>\n");
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
}

void showOtherUserOptions()
/* Print out inputs for loading another user's saved session. */
{
printf("<P>Load settings from another user's saved session:<BR>\n"
       "&nbsp;&nbsp;&nbsp;User: \n");
cgiMakeTextVar(hgsOtherUserName,
	       cartUsualString(cart, hgsOtherUserName, ""),
	       20);
printf("&nbsp;&nbsp;&nbsp;Session name: \n");
cgiMakeTextVar(hgsOtherUserSessionName,
	       cartUsualString(cart, hgsOtherUserSessionName, ""),
	       20);
printf("&nbsp;&nbsp;&nbsp;");
cgiMakeButton(hgsDoOtherUser, "submit");
printf("</P>\n");
}

void showSaveLoadOptions()
/* Print out inputs for saving current settings to a local file, 
 * loading from a local file, and loading from a URL. */
{
static char *textOutCompressMenu[] = textOutCompressMenuContents;
static char *textOutCompressValues[] = textOutCompressValuesContents;
static int textOutCompressMenuSize = ArraySize(textOutCompressMenu) - 1;

printf("<P>Save current settings to a local file:\n");
cgiMakeTextVar(hgsSaveLocalFileName,
	       cartUsualString(cart, hgsSaveLocalFileName, ""),
	       20);
printf("&nbsp;&nbsp;&nbsp;");
printf("file type returned: ");
cgiMakeDropListFull(hgsSaveLocalFileCompress, 
	textOutCompressMenu, textOutCompressValues, textOutCompressMenuSize, 
	cartUsualString(cart, hgsSaveLocalFileCompress, textOutCompressNone),
	NULL);
printf("&nbsp;&nbsp;&nbsp;");
cgiMakeButton(hgsDoSaveLocal, "submit");
printf("</P>\n");

printf("<P>Load settings from a local file:\n");
printf("<INPUT TYPE=FILE NAME=\"%s\">\n", hgsLoadLocalFileName);
printf("&nbsp;&nbsp;&nbsp;");
cgiMakeButton(hgsDoLoadLocal, "submit");
printf("</P>\n");

printf("<P>Load settings from a URL:\n");
cgiMakeTextVar(hgsLoadUrlName,
	       cartUsualString(cart, hgsLoadUrlName, ""),
	       20);
printf("&nbsp;&nbsp;&nbsp;");
cgiMakeButton(hgsDoLoadUrl, "submit");
printf("</P>\n");
}

void showSessionControls(char *userName, boolean savedSessionsSupported,
			 boolean webStarted)
/* If userName is non-null, show sessions that belong to user and allow 
 * saving of named sessions.
 * If savedSessionsSupported, allow import of named sessions.
 * Allow export/import of settings from file/URL. */
{
char *formMethod = cartUsualString(cart, "formMethod", "POST");

if (webStarted)
    webNewSection("Session Management");
else
    cartWebStart(cart, "Session Management");

showCartLinks();

printf("<FORM ACTION=\"/cgi-bin/hgSession\" NAME=\"mainForm\" METHOD=%s "
       "ENCTYPE=\"multipart/form-data\">\n",
       formMethod);
cartSaveSession(cart);
if (userName != NULL)
    {
    showNewSessionOptions();
    showExistingSessions(userName);
    }

if (savedSessionsSupported)
    {
    showOtherUserOptions();
    }

showSaveLoadOptions();
printf("</FORM>\n");
}

void doMainPage(char *message)
/* Login status/links and session controls. */
{
puts("Content-Type:text/html\n");
if (wikiLinkEnabled())
    {
    char *wikiUserName = wikiLinkUserName();
    if (wikiUserName)
	welcomeUser(wikiUserName);
    else
	offerLogin();
    if (isNotEmpty(message))
	{
	webNewSection("Updated Session");
	puts(message);
	}
    showSessionControls(wikiUserName, TRUE, TRUE);
    }
else
    {
    if (isNotEmpty(message))
	{
	cartWebStart(cart, "Updated Session");
	puts(message);
	showSessionControls(NULL, FALSE, TRUE);
	}
    else
	showSessionControls(NULL, FALSE, FALSE);
    }
cartWebEnd();
}


char *doNewSession()
/* Save current settings in a new named session.  
 * Return a message confirming what we did. */
{
char message[1024];
char *sessionName = cartString(cart, hgsNewSessionName);
boolean shareSession = cartBoolean(cart, hgsNewSessionShare);
char *userName = wikiLinkUserName();
struct sqlConnection *conn = hConnectCentral();

if (sqlTableExists(conn, namedSessionTable))
    {
    struct sqlResult *sr = NULL;
    struct dyString *dy = dyStringNew(16 * 1024);
    char **row;
    char *firstUse = "now()";
    int useCount = 0;
    char firstUseBuf[32];

    /* If this session already existed, preserve its firstUse and useCount. */
    dyStringPrintf(dy, "SELECT firstUse, useCount FROM %s "
		       "WHERE userName = '%s' AND sessionName = '%s';",
		   namedSessionTable, userName, sessionName);
    sr = sqlGetResult(conn, dy->string);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	safef(firstUseBuf, sizeof(firstUseBuf), "'%s'", row[0]);
	firstUse = firstUseBuf;
	useCount = atoi(row[1]) + 1;
	}
    sqlFreeResult(&sr);

    /* Remove pre-existing session (if any) before updating. */
    dyStringClear(dy);
    dyStringPrintf(dy, "DELETE FROM %s WHERE userName = '%s' AND "
		       "sessionName = '%s';",
		   namedSessionTable, userName, sessionName);
    sqlUpdate(conn, dy->string);

    dyStringClear(dy);
    dyStringPrintf(dy, "INSERT INTO %s ", namedSessionTable);
    dyStringAppend(dy, "(userName, sessionName, contents, shared, "
		       "firstUse, lastUse, useCount) VALUES (");
    dyStringPrintf(dy, "'%s', ", userName);
    dyStringPrintf(dy, "'%s', ", sessionName);
    dyStringAppend(dy, "'");
    cartEncodeState(cart, dy);
    dyStringAppend(dy, "', ");
    dyStringPrintf(dy, "%d, ", (shareSession ? 1 : 0));
    dyStringPrintf(dy, "%s, now(), %d);", firstUse, useCount);
    sqlUpdate(conn, dy->string);

    safef(message, sizeof(message),
	  "Added a new session <B>%s</B> that %s be shared with other users.",
	  sessionName, (shareSession ? "may" : "may not"));
    dyStringFree(&dy);
    }
else
    safef(message, sizeof(message),
	  "Sorry, required table %s does not exist yet in the central "
	  "database (%s).  Please ask a developer to create it using "
	  "kent/src/hg/lib/namedSessionDb.sql .",
	  namedSessionTable, sqlGetDatabase(conn));
hDisconnectCentral(&conn);
return cloneString(message);
}

void loadUserSession(struct sqlConnection *conn, char *sessionOwner,
		     char *sessionName)
/* If permitted, load the contents of the given user's session. */
{
struct sqlResult *sr = NULL;
char **row = NULL;
char *userName = wikiLinkUserName();
char query[512];

safef(query, sizeof(query), "SELECT shared, contents FROM %s "
      "WHERE userName = '%s' AND sessionName = '%s';",
      namedSessionTable, sessionOwner, sessionName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    boolean shared = atoi(row[0]);
    if (shared ||
	(userName && sameString(sessionOwner, userName)))
	{
	char *sessionVar = cartSessionVarName();
	unsigned hgsid = cartSessionId(cart);
	cartRemoveLike(cart, "*");
	cartParseOverHash(cart, row[1]);
	cartSetInt(cart, sessionVar, hgsid);
	}
    else
	errAbort("Sharing has not been enabled for user %s's session %s.",
		 sessionOwner, sessionName);
    }
else
    errAbort("Could not find session %s for user %s.",
	     sessionName, userName);
sqlFreeResult(&sr);
}

char *doUpdateSessions()
/* Look for cart variables matching prefixes for sharing/unsharing, 
 * loading or deleting a previously saved session.  
 * Return a message confirming what we did, or NULL if no such variables 
 * were in the cart. */
{
struct dyString *dyMessage = dyStringNew(1024);
struct hashEl *helList = NULL, *hel = NULL;
struct sqlConnection *conn = hConnectCentral();
char *userName = wikiLinkUserName();
boolean didSomething = FALSE;
char query[512];

helList = cartFindPrefix(cart, hgsUnsharePrefix);
for (hel = helList;  hel != NULL;  hel = hel->next)
    {
    char *sessionName = hel->name + strlen(hgsUnsharePrefix);
    safef(query, sizeof(query), "UPDATE %s SET shared = 0 "
	  "WHERE userName = '%s' AND sessionName = '%s';",
	  namedSessionTable, userName, sessionName);
    sqlUpdate(conn, query);
    dyStringPrintf(dyMessage,
		   "Marked session <B>%s</B> as unshared.<BR>\n",
		   sessionName);
    didSomething = TRUE;
    }

helList = cartFindPrefix(cart, hgsSharePrefix);
for (hel = helList;  hel != NULL;  hel = hel->next)
    {
    char *sessionName = hel->name + strlen(hgsSharePrefix);
    safef(query, sizeof(query), "UPDATE %s SET shared = 1 "
	  "WHERE userName = '%s' AND sessionName = '%s';",
	  namedSessionTable, userName, sessionName);
    sqlUpdate(conn, query);
    dyStringPrintf(dyMessage,
		   "Marked session <B>%s</B> as shared.<BR>\n",
		   sessionName);
    didSomething = TRUE;
    }

hel = cartFindPrefix(cart, hgsLoadPrefix);
if (hel != NULL)
    {
    char *sessionName = hel->name + strlen(hgsLoadPrefix);
    dyStringPrintf(dyMessage,
		   "Loaded settings from session <B>%s</B>.<BR>\n",
		   sessionName);
    loadUserSession(conn, userName, sessionName);
    didSomething = TRUE;
    }

helList = cartFindPrefix(cart, hgsDeletePrefix);
for (hel = helList;  hel != NULL;  hel = hel->next)
    {
    char *sessionName = hel->name + strlen(hgsDeletePrefix);
    safef(query, sizeof(query), "DELETE FROM %s "
	  "WHERE userName = '%s' AND sessionName = '%s';",
	  namedSessionTable, userName, sessionName);
    sqlUpdate(conn, query);
    dyStringPrintf(dyMessage,
		   "Deleted session <B>%s</B>.<BR>\n",
		   sessionName);
    didSomething = TRUE;
    }

hDisconnectCentral(&conn);
if (didSomething)
    return(dyStringCannibalize(&dyMessage));
else
    {
    dyStringFree(&dyMessage);
    return NULL;
    }
}

char *doOtherUser()
/* Load settings from another user's named session.  
 * Return a message confirming what we did. */
{
struct sqlConnection *conn = hConnectCentral();
char message[1024];
char *otherUser = cartString(cart, hgsOtherUserName);
char *sessionName = cartString(cart, hgsOtherUserSessionName);

safef(message, sizeof(message),
      "Loaded settings from user <B>%s</B>'s session <B>%s</B>.",
      otherUser, sessionName);
loadUserSession(conn, otherUser, sessionName);
hDisconnectCentral(&conn);
return cloneString(message);
}

void doSaveLocal()
/* Output current settings to be saved as a file on the user's machine.  
 * Return a message confirming what we did. */
{
char *fileName = cartString(cart, hgsSaveLocalFileName);
char *compressType = cartString(cart, hgsSaveLocalFileCompress);
struct pipeline *compressPipe = textOutInit(fileName, compressType);

cartDump(cart);

textOutClose(&compressPipe);
}

void loadSettings(struct lineFile *lf)
/* Load settings (cartDump output) into current session. */
{
char *line = NULL;
int size = 0;
char *words[2];
int wordCount = 0;
char *sessionVar = cartSessionVarName();
unsigned hgsid = cartSessionId(cart);

cartRemoveLike(cart, "*");
cartSetInt(cart, sessionVar, hgsid);
while (lineFileNext(lf, &line, &size))
    {
    wordCount = chopString(line, " ", words, ArraySize(words));
    if (sameString(words[0], sessionVar))
	continue;
    else
	{
	if (wordCount == 2)
	    {
	    struct dyString *dy = dyStringSub(words[1], "\\n", "\n");
	    cartSetString(cart, words[0], dy->string);
	    dyStringFree(&dy);
	    }
	else if (wordCount == 1)
	    {
	    cartSetString(cart, words[0], "");
	    }
	} /* not hgsid */
    } /* each line */
}

char *doLoad(boolean fromUrl)
/* Load settings from a file or URL sent by the user.  
 * Return a message confirming what we did. */
{
char message[1024];
struct lineFile *lf = NULL;
webPushErrHandlersCart(cart);
if (fromUrl)
    {
    char *url = cartString(cart, hgsLoadUrlName);
    lf = netLineFileOpen(url);
    safef(message, sizeof(message),
	  "Loaded settings from URL %s .", url);
    }
else
    {
    char *settings = cartString(cart, hgsLoadLocalFileName);
    lf = lineFileOnString("settingsFromFile", TRUE, settings);
    safef(message, sizeof(message),
	  "Loaded settings from local file length %d.",
	  strlen(settings));
    }
loadSettings(lf);
lineFileClose(&lf);
return cloneString(message);
}


void hgSession()
/* hgSession - Interface with wiki login and do session saving/loading.
 * Here we set up cart and some global variables, dispatch the command,
 * and put away the cart when it is done. */
{
struct hash *oldVars = hashNew(10);

/* Sometimes we output HTML and sometimes plain text; let each outputter 
 * take care of headers instead of using a fixed cart*Shell(). */
cart = cartAndCookieNoContent(hUserCookie(), excludeVars, oldVars);

if (cartVarExists(cart, hgsDoMainPage))
    doMainPage(NULL);
else if (cartVarExists(cart, hgsDoNewSession))
    {
    char *message = doNewSession();
    doMainPage(message);
    }
else if (cartVarExists(cart, hgsDoOtherUser))
    {
    char *message = doOtherUser();
    doMainPage(message);
    }
else if (cartVarExists(cart, hgsDoSaveLocal))
    {
    doSaveLocal();
    }
else if (cartVarExists(cart, hgsDoLoadLocal))
    {
    char *message = doLoad(FALSE);
    doMainPage(message);
    }
else if (cartVarExists(cart, hgsDoLoadUrl))
    {
    char *message = doLoad(TRUE);
    doMainPage(message);
    }
else
    {
    char *message = doUpdateSessions();
    doMainPage(message);
    }

/* Remove cart variables that should not stay there (actions that have been 
 * performed),  and save the cart state. */
cartRemovePrefix(cart, hgsUnsharePrefix);
cartRemovePrefix(cart, hgsSharePrefix);
cartRemovePrefix(cart, hgsLoadPrefix);
cartRemovePrefix(cart, hgsDeletePrefix);
cartRemovePrefix(cart, hgsDo);
cartCheckout(&cart);
}

int main(int argc, char *argv[])
/* Process command line. */
{
htmlPushEarlyHandlers();
cgiSpoof(&argc, argv);
hgSession();
return 0;
}

