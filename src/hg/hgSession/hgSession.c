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

static char const rcsid[] = "$Id: hgSession.c,v 1.7 2006/09/20 23:31:27 angie Exp $";

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


void welcomeUser(char *wikiUserName)
/* Tell the user they are not logged in to the wiki and tell them how to 
 * do so. */
{
char *wikiHost = wikiLinkHost();

cartWebStart(cart, "Welcome %s", wikiUserName);
printf("If you are not %s (on the wiki at "
       "<A HREF=\"http://%s/\" TARGET=_BLANK>%s</A>) "
       "and would like to sign out or change identity, \n",
       wikiUserName, wikiHost, wikiHost);
printf("<A HREF=\"%s\"><B>click here to sign out.</B></A>\n",
       wikiLinkUserLogoutUrl(cartSessionId(cart)));
}

void offerLogin()
/* Tell the user they are not logged in to the wiki and tell them how to 
 * do so. */
{
char *wikiHost = wikiLinkHost();

cartWebStart(cart, "Sign in to UCSC Genome Bioinformatics");
printf("Signing in enables you to save current settings into a "
       "named session, and then restore settings from the session later.\n"
       "If you wish, you can share named sessions with other users.\n"
       "The wiki also serves as a forum for users "
       "to share knowledge and ideas.\n");
printf("<P>The sign in page is handled by our "
       "<A HREF=\"http://%s/\" TARGET=_BLANK>wiki system</A>:\n", wikiHost);
printf("<A HREF=\"%s\"><B>click here to sign in.</B></A>\n",
       wikiLinkUserLoginUrl(cartSessionId(cart)));
printf("</P>\n");
}

void showCartLinks()
/* Print out links to cartDump and cartReset. */
{
char *session = cartSidUrlString(cart);
char returnAddress[512];

safef(returnAddress, sizeof(returnAddress), "/cgi-bin/hgSession?%s", session);
printf("<A HREF=\"/cgi-bin/cartReset?%s&destination=%s\">Reset session</A>"
       "</P>\n",
       session, cgiEncode(returnAddress));
}


char *destAppScriptName()
/* Return the complete path (/cgi-bin/... on our systems) of the destination
 * CGI for share-able links.  Currently hardcoded; there might be a way to 
 * offer the user a choice. */
{
static char *thePath = NULL;
if (thePath == NULL)
    {
    char path[512];
    char buf[512];
    char *ptr = NULL;
    safef(path, sizeof(path), "%s", cgiScriptName());
    ptr = strrchr(path, '/');
    if (ptr == NULL)
	path[0] = '\0';
    else
	*(ptr+1) = '\0';
    safef(buf, sizeof(buf), "%s%s", path, "hgTracks");
    thePath = cloneString(buf);
    }
return thePath;
}

void addSessionLink(struct dyString *dy, char *userName, char *sessionName,
		    boolean encode)
/* Add to dy an URL that tells hgSession to load a saved session.  
 * If encode, cgiEncode the URL. */
{
struct dyString *dyTmp = dyStringNew(1024);
dyStringPrintf(dyTmp, "http://%s%s?hgS_doOtherUser=submit&"
	       "hgS_otherUserName=%s&hgS_otherUserSessionName=%s",
	       cgiServerName(), destAppScriptName(), userName, sessionName);
if (encode)
    {
    dyStringPrintf(dy, "%s", cgiEncode(dyTmp->string));
    }
else
    {
    dyStringPrintf(dy, "%s", dyTmp->string);
    }
dyStringFree(&dyTmp);
}

char *getSessionLink(char *userName, char *sessionName)
/* Form a link that will take the user to a bookmarkable page that 
 * will load the given session. */
{
struct dyString *dy = dyStringNew(1024);
dyStringPrintf(dy, "<A HREF=\"");
addSessionLink(dy, userName, sessionName, FALSE);
dyStringPrintf(dy, "\">Link</A>\n");
return dyStringCannibalize(&dy);
}

char *getSessionEmailLink(char *userName, char *sessionName)
/* Invoke mailto: with a cgi-encoded link that will take the user to a 
 * bookmarkable page that will load the given session. */
{
struct dyString *dy = dyStringNew(1024);
dyStringPrintf(dy, "<A HREF=\"mailto:?subject=UCSC browser session %s&"
	       "body=This is my new favorite UCSC browser session:%%20",
	       sessionName);
addSessionLink(dy, userName, sessionName, TRUE);
dyStringPrintf(dy, "\">Email</A>\n");
return dyStringCannibalize(&dy);
}

void addUrlLink(struct dyString *dy, char *url, boolean encode)
/* Add to dy an URL that tells hgSession to load settings from the given url. 
 * If encode, cgiEncode the whole thing. */
{
struct dyString *dyTmp = dyStringNew(1024);
char *encodedUrl = cgiEncode(url);
dyStringPrintf(dyTmp, "http://%s%s?hgS_doLoadUrl=submit&hgS_loadUrlName=%s",
	       cgiServerName(), destAppScriptName(), encodedUrl);
if (encode)
    {
    dyStringPrintf(dy, "%s", cgiEncode(dyTmp->string));
    }
else
    {
    dyStringPrintf(dy, "%s", dyTmp->string);
    }
freeMem(encodedUrl);
dyStringFree(&dyTmp);
}	       

char *getUrlLink(char *url)
/* Form a link that will take the user to a bookmarkable page that 
 * will load the given url. */
{
struct dyString *dy = dyStringNew(1024);
dyStringPrintf(dy, "<A HREF=\"");
addUrlLink(dy, url, FALSE);
dyStringPrintf(dy, "\">Link</A>\n");
return dyStringCannibalize(&dy);
}

char *getUrlEmailLink(char *url)
/* Invoke mailto: with a cgi-encoded link that will take the user to a 
 * bookmarkable page that will load the given url. */
{
struct dyString *dy = dyStringNew(1024);
dyStringPrintf(dy, "<A HREF=\"mailto:?subject=UCSC browser session&"
	       "body=This is my new favorite UCSC browser session:%%20");
addUrlLink(dy, url, TRUE);
dyStringPrintf(dy, "\">Email</A>\n");
return dyStringCannibalize(&dy);
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

printf("<TABLE BORDERWIDTH=0>\n");
printf("<TR><TD colspan=7>Previously saved sessions:</TD></TR>\n");
safef(query, sizeof(query), "SELECT sessionName, shared from %s "
      "WHERE userName = '%s';",
      namedSessionTable, userName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *sessionName = row[0];
    char *link = NULL;
    boolean shared = atoi(row[1]);
    char buf[512];
    printf("<TR><TD>&nbsp;&nbsp;&nbsp;</TD><TD>%s</TD><TD>", sessionName);
    safef(buf, sizeof(buf), "%s%s", hgsLoadPrefix, sessionName);
    cgiMakeButton(buf, "load as current session");
    printf("</TD><TD>");
    safef(buf, sizeof(buf), "%s%s", hgsDeletePrefix, sessionName);
    cgiMakeButton(buf, "delete");
    printf("</TD><TD>");
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
    link = getSessionLink(userName, sessionName);
    printf("</TD><TD>%s</TD>\n", link);
    freez(&link);
    link = getSessionEmailLink(userName, sessionName);
    printf("<TD>%s</TD></TR>\n", link);
    freez(&link);
    foundAny = TRUE;
    }
if (!foundAny)
    printf("<TR><TD>&nbsp;&nbsp;&nbsp;</TD><TD>(none)</TD>"
	   "<TD colspan=5></TD></TR>\n");
printf("<TR><TD colspan=7></TD></TR>\n");
printf("</TABLE>\n");
printf("<P></P>\n");
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
}

void showOtherUserOptions()
/* Print out inputs for loading another user's saved session. */
{
printf("<TABLE BORDERWIDTH=0>\n");
printf("<TR><TD colspan=2>"
       "Load settings from another user's saved session:</TD></TR>\n"
       "<TR><TD>&nbsp;&nbsp;&nbsp;</TD><TD>user: \n");
cgiMakeTextVar(hgsOtherUserName,
	       cartUsualString(cart, hgsOtherUserName, ""),
	       20);
printf("&nbsp;&nbsp;&nbsp;session name: \n");
cgiMakeTextVar(hgsOtherUserSessionName,
	       cartUsualString(cart, hgsOtherUserSessionName, ""),
	       20);
printf("&nbsp;&nbsp;");
cgiMakeButton(hgsDoOtherUser, "submit");
printf("</TD></TR>\n");
printf("<TR><TD colspan=2></TD></TR>\n");
printf("</TABLE>\n");
}

void showLoadingOptions(char *userName, boolean savedSessionsSupported)
/* Show options for loading settings from another user's session, a file 
 * or URL. */
{
if (savedSessionsSupported)
    showOtherUserOptions();

printf("<TABLE BORDERWIDTH=0>\n");
printf("<TR><TD colspan=2>Load settings from a local file:</TD>\n");
printf("<TD><INPUT TYPE=FILE NAME=\"%s\">\n", hgsLoadLocalFileName);
printf("&nbsp;&nbsp;");
cgiMakeButton(hgsDoLoadLocal, "submit");
printf("</TD></TR>\n");
printf("<TR><TD colspan=2></TD></TR>\n");

printf("<TR><TD colspan=2>Load settings from a URL:</TD>\n");
printf("<TD>\n");
cgiMakeTextVar(hgsLoadUrlName,
	       cartUsualString(cart, hgsLoadUrlName, ""),
	       20);
printf("&nbsp;&nbsp;");
cgiMakeButton(hgsDoLoadUrl, "submit");
printf("</TD></TR>\n");
printf("</TABLE>\n");
printf("<P></P>\n");
}

void showSavingOptions(char *userName)
/* Show options for saving a new named session in our db or to a file. */
{
static char *textOutCompressMenu[] = textOutCompressMenuContents;
static char *textOutCompressValues[] = textOutCompressValuesContents;
static int textOutCompressMenuSize = ArraySize(textOutCompressMenu) - 1;

printf("<TABLE BORDERWIDTH=0>\n");

if (isNotEmpty(userName))
    {
    printf("<TR><TD colspan=4>Save current settings as named session:"
	   "</TD></TR>\n"
	   "<TR><TD>&nbsp;&nbsp;&nbsp;</TD><TD>name:</TD><TD>\n");
    cgiMakeTextVar(hgsNewSessionName,
		   cartUsualString(cart, "db", hGetDb()),
		   20);
    printf("&nbsp;&nbsp;&nbsp;");
    cgiMakeCheckBox(hgsNewSessionShare,
		    cartUsualBoolean(cart, hgsNewSessionShare, TRUE));
    printf("allow this session to be loaded by others\n");
    printf("</TD><TD>");
    printf("&nbsp;");
    cgiMakeButton(hgsDoNewSession, "submit");
    printf("</TD></TR>\n");
    printf("<TR><TD colspan=4></TD></TR>\n");
    }

printf("<TR><TD colspan=4>Save current settings to a local file:</TD></TR>\n");
printf("<TR><TD>&nbsp;&nbsp;&nbsp;</TD><TD>file:</TD><TD>\n");
cgiMakeTextVar(hgsSaveLocalFileName,
	       cartUsualString(cart, hgsSaveLocalFileName, ""),
	       20);
printf("&nbsp;&nbsp;&nbsp;");
printf("file type returned: ");
cgiMakeDropListFull(hgsSaveLocalFileCompress, 
	textOutCompressMenu, textOutCompressValues, textOutCompressMenuSize, 
	cartUsualString(cart, hgsSaveLocalFileCompress, textOutCompressNone),
	NULL);
printf("</TD><TD>");
printf("&nbsp;");
cgiMakeButton(hgsDoSaveLocal, "submit");
printf("</TD></TR>\n");
printf("<TR><TD></TD><TD colspan=3>(leave file blank to get output in "
       "browser window)</TD></TR>\n");
printf("<TR><TD colspan=4></TD></TR>\n");
printf("</TABLE>\n");
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

if (isNotEmpty(userName))
    showExistingSessions(userName);
else
    printf("If you <A HREF=\"%s\">sign in</A>, "
	   "you will also have the option to save named sessions.<P>\n",
	   wikiLinkUserLoginUrl(cartSessionId(cart)));
showLoadingOptions(userName, savedSessionsSupported);
showSavingOptions(userName);
printf("</FORM>\n");
}

void showLinkingTemplates(char *userName)
/* Explain how to create links to us for sharing sessions. */
{
struct dyString *dyUrl = dyStringNew(1024);
webNewSection("Sharing Sessions");
printf("There are several ways to share saved sessions with others.\n");
printf("<UL>\n");
if (userName != NULL)
    {
    printf("<LI>Each previously saved named session appears with "
	   "Link and Email options.  Link takes you to a page with "
	   "a static link to the session which you can bookmark in your "
	   "web browser.  Email is for sending the link to others.</LI>\n");
    }
else
    {
    printf("<LI>If you sign in, you will be able to save named sessions "
	   "which will be displayed with Link and Email options.</LI>\n");
    }
dyStringPrintf(dyUrl, "http://%s%s", cgiServerName(), cgiScriptName());

printf("<LI>If you have saved your settings to a local file, you can send "
       "email to others with the file as an attachment and direct them to "
       "<A HREF=\"%s\">%s</A> .</LI>\n",
       dyUrl->string, dyUrl->string);
dyStringPrintf(dyUrl, "?hgS_doLoadUrl=submit&hgS_loadUrlName="
	       "http://www.mysite.edu/~me/mySession.txt");
printf("<LI>If a saved settings file is available from a web server, "
       "you can send email to others with a link such as "
       "<A HREF=\"%s\">%s</A> .  In this type of link, you can replace "
       "\"hgSession\" with \"hgTracks\" in order to proceed directly to "
       "the Genome Browser.</LI>\n",
       dyUrl->string, dyUrl->string);
printf("</UL>\n");
dyStringFree(&dyUrl);
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
    showLinkingTemplates(wikiUserName);
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
    showLinkingTemplates(NULL);
    }
cartWebEnd();
}


char *doNewSession()
/* Save current settings in a new named session.  
 * Return a message confirming what we did. */
{
struct dyString *dyMessage = dyStringNew(2048);
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

    dyStringPrintf(dyMessage,
	  "Added a new session <B>%s</B> that %s be shared with other users.  "
	  "%s %s",
	  sessionName, (shareSession ? "may" : "may not"),
	  getSessionLink(userName, sessionName),
	  getSessionEmailLink(userName, sessionName));
    dyStringFree(&dy);
    }
else
    dyStringPrintf(dyMessage,
	  "Sorry, required table %s does not exist yet in the central "
	  "database (%s).  Please ask a developer to create it using "
	  "kent/src/hg/lib/namedSessionDb.sql .",
	  namedSessionTable, sqlGetDatabase(conn));
hDisconnectCentral(&conn);
return dyStringCannibalize(&dyMessage);
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
    loadUserSession(conn, userName, sessionName, cart);
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
loadUserSession(conn, otherUser, sessionName, cart);
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

char *doLoad(boolean fromUrl)
/* Load settings from a file or URL sent by the user.  
 * Return a message confirming what we did. */
{
struct dyString *dyMessage = dyStringNew(1024);
struct lineFile *lf = NULL;
webPushErrHandlersCart(cart);
if (fromUrl)
    {
    char *url = cartString(cart, hgsLoadUrlName);
    lf = netLineFileOpen(url);
    dyStringPrintf(dyMessage, "Loaded settings from URL %s .  %s %s",
		   url, getUrlLink(url), getUrlEmailLink(url));
    }
else
    {
    char *settings = cartString(cart, hgsLoadLocalFileName);
    lf = lineFileOnString("settingsFromFile", TRUE, settings);
    dyStringPrintf(dyMessage, "Loaded settings from local file (%lu bytes).",
		   (unsigned long)strlen(settings));
    }
loadSettings(lf, cart);
lineFileClose(&lf);
return dyStringCannibalize(&dyMessage);
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

