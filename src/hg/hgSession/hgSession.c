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
#include "customTrack.h"
#include "customFactory.h"
#include "hgSession.h"

static char const rcsid[] = "$Id: hgSession.c,v 1.26 2007/03/28 21:14:24 angie Exp $";

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
       "If you wish, you can share named sessions with other users.\n");
printf("<P>The sign-in page is handled by our "
       "<A HREF=\"http://%s/\" TARGET=_BLANK>wiki system</A>:\n", wikiHost);
printf("<A HREF=\"%s\"><B>click here to sign in.</B></A>\n",
       wikiLinkUserLoginUrl(cartSessionId(cart)));
printf("The wiki also serves as a forum for users "
       "to share knowledge and ideas.\n");
}

void showCartLinks()
/* Print out links to cartDump and cartReset. */
{
char *session = cartSidUrlString(cart);
char returnAddress[512];

safef(returnAddress, sizeof(returnAddress), "/cgi-bin/hgSession?%s", session);
printf("<A HREF=\"/cgi-bin/cartReset?%s&destination=%s\">Click here to "
       "reset</A> the browser user interface settings to their defaults.\n",
       session, cgiEncodeFull(returnAddress));
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
 * If encode, cgiEncodeFull the URL. */
{
struct dyString *dyTmp = dyStringNew(1024);
dyStringPrintf(dyTmp, "http://%s%s?hgS_doOtherUser=submit&"
	       "hgS_otherUserName=%s&hgS_otherUserSessionName=%s",
	       cgiServerName(), destAppScriptName(), userName, sessionName);
if (encode)
    {
    dyStringPrintf(dy, "%s", cgiEncodeFull(dyTmp->string));
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
dyStringPrintf(dy, "\">Browser</A>\n");
return dyStringCannibalize(&dy);
}

char *getSessionEmailLink(char *userName, char *sessionName)
/* Invoke mailto: with a cgi-encoded link that will take the user to a 
 * bookmarkable page that will load the given session. */
{
struct dyString *dy = dyStringNew(1024);
dyStringPrintf(dy, "<A HREF=\"mailto:?subject=UCSC browser session %s&"
	       "body=Here is a UCSC browser session I%%27d like to share with "
	       "you:%%20",
	       sessionName);
addSessionLink(dy, userName, sessionName, TRUE);
dyStringPrintf(dy, "\">Email</A>\n");
return dyStringCannibalize(&dy);
}

void addUrlLink(struct dyString *dy, char *url, boolean encode)
/* Add to dy an URL that tells hgSession to load settings from the given url. 
 * If encode, cgiEncodeFull the whole thing. */
{
struct dyString *dyTmp = dyStringNew(1024);
char *encodedUrl = cgiEncodeFull(url);
dyStringPrintf(dyTmp, "http://%s%s?hgS_doLoadUrl=submit&hgS_loadUrlName=%s",
	       cgiServerName(), destAppScriptName(), encodedUrl);
if (encode)
    {
    dyStringPrintf(dy, "%s", cgiEncodeFull(dyTmp->string));
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
dyStringPrintf(dy, "\">Browser</A>\n");
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


char *cgiDecodeClone(char *encStr)
/* Allocate and return a CGI-decoded copy of encStr. */
{
size_t len = strlen(encStr);
char *decStr = needMem(len+1);
cgiDecode(encStr, decStr, len);
return decStr;
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
char *encUserName = cgiEncodeFull(userName);

printf("<H3>My Sessions</H3>\n");
printf("<TABLE BORDERWIDTH=0>\n");
safef(query, sizeof(query), "SELECT sessionName, shared from %s "
      "WHERE userName = '%s' ORDER BY sessionName;",
      namedSessionTable, encUserName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *encSessionName = row[0];
    char *sessionName = cgiDecodeClone(encSessionName);
    char *link = NULL;
    boolean shared = atoi(row[1]);
    char buf[512];
    printf("<TR><TD>&nbsp;&nbsp;&nbsp;</TD><TD>");
    htmlTextOut(sessionName);
    printf("</TD><TD>");
    safef(buf, sizeof(buf), "%s%s", hgsLoadPrefix, encSessionName);
    cgiMakeButton(buf, "load as current session");
    printf("</TD><TD>");
    safef(buf, sizeof(buf), "%s%s", hgsDeletePrefix, encSessionName);
    cgiMakeButton(buf, "delete");
    printf("</TD><TD>");
    if (shared)
	{
	safef(buf, sizeof(buf), "%s%s", hgsUnsharePrefix, encSessionName);
	cgiMakeButton(buf, "don't share");
	}
    else
	{
	safef(buf, sizeof(buf), "%s%s", hgsSharePrefix, encSessionName);
	cgiMakeButton(buf, "share");
	}
    link = getSessionLink(userName, encSessionName);
    printf("</TD><TD>%s</TD>\n", link);
    freez(&link);
    link = getSessionEmailLink(userName, encSessionName);
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
printf("<H3>Load Settings</H3>\n");
if (savedSessionsSupported)
    showOtherUserOptions();

printf("<TABLE BORDERWIDTH=0>\n");
printf("<TR><TD colspan=2>Load settings from a local file:</TD>\n");
printf("<TD><INPUT TYPE=FILE NAME=\"%s\">\n", hgsLoadLocalFileName);
printf("&nbsp;&nbsp;");
cgiMakeButton(hgsDoLoadLocal, "submit");
printf("</TD></TR>\n");
printf("<TR><TD colspan=2></TD></TR>\n");

printf("<TR><TD colspan=2>Load settings from a URL (http://..., ftp://...):"
       "</TD>\n");
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

printf("<H3>Save Settings</H3>\n");
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
else if (savedSessionsSupported)
    printf("<P>If you <A HREF=\"%s\">sign in</A>, "
	   "you will also have the option to save named sessions.\n",
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
	   "Browser and Email links.  "
	   "The Browser link takes you to the Genome Browser "
	   "with that session loaded.  The resulting Genome Browser page "
	   "can be bookmarked in your web browser and/or shared with others.  "
	   "The Email link invokes your email tool with a message "
	   "containing the Genome Browser link.</LI>\n");
    }
else if (wikiLinkEnabled())
    {
    printf("<LI>If you <A HREF=\"%s\">sign in</A>, you will be able to save "
	   "named sessions which will be displayed with Browser and Email "
	   "links.</LI>\n", wikiLinkUserLoginUrl(cartSessionId(cart)));
    }
dyStringPrintf(dyUrl, "http://%s%s", cgiServerName(), cgiScriptName());

printf("<LI>If you have saved your settings to a local file, you can send "
       "email to others with the file as an attachment and direct them to "
       "<A HREF=\"%s\">%s</A> .</LI>\n",
       dyUrl->string, dyUrl->string);
dyStringPrintf(dyUrl, "?hgS_doLoadUrl=submit&hgS_loadUrlName=");
printf("<LI>If a saved settings file is available from a web server, "
       "you can send email to others with a link such as "
       "%s<B>U</B> where <B>U</B> is the URL of your "
       "settings file, e.g. http://www.mysite.edu/~me/mySession.txt .  "
       "In this type of link, you can replace "
       "\"hgSession\" with \"hgTracks\" in order to proceed directly to "
       "the Genome Browser.</LI>\n",
       dyUrl->string);
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


void cleanHgSessionFromCart(struct cart *cart)
/* Remove hgSession action variables that should not stay in the cart. */
{
cartRemovePrefix(cart, hgsUnsharePrefix);
cartRemovePrefix(cart, hgsSharePrefix);
cartRemovePrefix(cart, hgsLoadPrefix);
cartRemovePrefix(cart, hgsDeletePrefix);
cartRemovePrefix(cart, hgsDo);
}

#define INITIAL_USE_COUNT 0
char *doNewSession()
/* Save current settings in a new named session.  
 * Return a message confirming what we did. */
{
struct dyString *dyMessage = dyStringNew(2048);
char *sessionName = trimSpaces(cartString(cart, hgsNewSessionName));
char *encSessionName = cgiEncodeFull(sessionName);
boolean shareSession = cartBoolean(cart, hgsNewSessionShare);
char *userName = wikiLinkUserName();
char *encUserName = cgiEncodeFull(userName);
struct sqlConnection *conn = hConnectCentral();

if (sqlTableExists(conn, namedSessionTable))
    {
    struct sqlResult *sr = NULL;
    struct dyString *dy = dyStringNew(16 * 1024);
    char **row;
    char *firstUse = "now()";
    int useCount = INITIAL_USE_COUNT;
    char firstUseBuf[32];

    /* If this session already existed, preserve its firstUse and useCount. */
    dyStringPrintf(dy, "SELECT firstUse, useCount FROM %s "
		       "WHERE userName = '%s' AND sessionName = '%s';",
		   namedSessionTable, encUserName, encSessionName);
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
		   namedSessionTable, encUserName, encSessionName);
    sqlUpdate(conn, dy->string);

    dyStringClear(dy);
    dyStringPrintf(dy, "INSERT INTO %s ", namedSessionTable);
    dyStringAppend(dy, "(userName, sessionName, contents, shared, "
		       "firstUse, lastUse, useCount) VALUES (");
    dyStringPrintf(dy, "'%s', '%s', ", encUserName, encSessionName);
    dyStringAppend(dy, "'");
    cleanHgSessionFromCart(cart);
    cartEncodeState(cart, dy);
    dyStringAppend(dy, "', ");
    dyStringPrintf(dy, "%d, ", (shareSession ? 1 : 0));
    dyStringPrintf(dy, "%s, now(), %d);", firstUse, useCount);
    sqlUpdate(conn, dy->string);

    if (useCount > INITIAL_USE_COUNT)
	dyStringPrintf(dyMessage,
	  "Overwrote the contents of session <B>%s</B> "
	  "(that %s be shared with other users).  "
	  "%s %s",
	  htmlEncode(sessionName), (shareSession ? "may" : "may not"),
	  getSessionLink(userName, encSessionName),
	  getSessionEmailLink(userName, encSessionName));
    else
	dyStringPrintf(dyMessage,
	  "Added a new session <B>%s</B> that %s be shared with other users.  "
	  "%s %s",
	  htmlEncode(sessionName), (shareSession ? "may" : "may not"),
	  getSessionLink(userName, encSessionName),
	  getSessionEmailLink(userName, encSessionName));
    if (cartFindPrefix(cart, CT_FILE_VAR_PREFIX) != NULL)
	dyStringPrintf(dyMessage,
		"<P>Note: the session contains a reference to at least one "
		"custom track.  Custom tracks are "
		"subject to an expiration policy described in the "
		"<A HREF=\"/goldenPath/help/customTrack.html\" "
		"TARGET=_BLANK>custom track documentation</A>.  "
		"In order to keep a custom track from expiring, you can "
		"periodically view the custom track in the genome browser."
		"</P>");
    if (cartOptionalString(cart, "ss") != NULL)
	dyStringPrintf(dyMessage,
		"<P>Note: the session contains a reference to saved BLAT "
		"results, which are subject to the same expiration policy as "
		"<A HREF=\"/goldenPath/help/customTrack.html\" "
		"TARGET=_BLANK>custom tracks</A>.  "
		"In order to keep BLAT results from expiring, you can "
		"periodically view them in the genome browser."
		"</P>");
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

void checkForCustomTracks(struct dyString *dyMessage)
/* Scan cart for ctfile_<db> variables.  Tally up the databases that have 
 * live custom tracks and those that have expired custom tracks. */
/* While we're at it, also look for saved blat results. */
{
struct hashEl *helList = cartFindPrefix(cart, CT_FILE_VAR_PREFIX);
if (helList != NULL)
    {
    struct hashEl *hel;
    boolean gotLiveCT = FALSE, gotExpiredCT = FALSE;
    struct slName *liveDbList = NULL, *expiredDbList = NULL, *sln = NULL;
    for (hel = helList;  hel != NULL;  hel = hel->next)
	{
	char *db = hel->name + strlen(CT_FILE_VAR_PREFIX);
	boolean thisGotLiveCT = FALSE, thisGotExpiredCT = FALSE;
	customFactoryTestExistence(hel->val, &thisGotLiveCT, &thisGotExpiredCT);
	if (thisGotLiveCT)
	    slNameAddHead(&liveDbList, db);
	if (thisGotExpiredCT)
	    slNameAddHead(&expiredDbList, db);
	gotLiveCT |= thisGotLiveCT;
	gotExpiredCT |= thisGotExpiredCT;
	}
    if (gotLiveCT)
	{
	slSort(&liveDbList, slNameCmp);
	dyStringPrintf(dyMessage,
		       "<P>Note: the session has at least one active custom "
		       "track (in database ");
	for (sln = liveDbList;  sln != NULL;  sln = sln->next)
	    dyStringPrintf(dyMessage, "%s%s",
			   sln->name, (sln->next ? sln->next->next ? ", " : " and " : ""));
	dyStringPrintf(dyMessage,
		       ").  Custom track(s) can be viewed "
		       "<A HREF=\"hgCustom?%s\">here</A> "
		       "or in the genome browser.</P>",
		       cartSidUrlString(cart));
	}
    if (gotExpiredCT)
	{
	slSort(&expiredDbList, slNameCmp);
	dyStringPrintf(dyMessage,
		       "<P>Note: the session has at least one expired custom "
		       "track (in database ");
	for (sln = expiredDbList;  sln != NULL;  sln = sln->next)
	    dyStringPrintf(dyMessage, "%s%s",
			   sln->name, (sln->next ? sln->next->next ? ", " : " and " : ""));
	dyStringPrintf(dyMessage,
		       "), so it may not appear as originally intended."
		       "</P>");
	}
    slNameFreeList(&liveDbList);
    slNameFreeList(&expiredDbList);
    }
/* Check for saved blat results (quasi custom track). */
char *ss = cartOptionalString(cart, "ss");
if (isNotEmpty(ss))
    {
    char buf[1024];
    char *words[2];
    int wordCount;
    boolean exists = FALSE;
    safef(buf, sizeof(buf), ss);
    wordCount = chopLine(buf, words);
    if (wordCount < 2)
	exists = FALSE;
    else
	exists = fileExists(words[0]) && fileExists(words[1]);

    if (exists)
	dyStringPrintf(dyMessage,
		"<P>Note: the session contains saved BLAT results "
		"which are subject to the same expiration policy as "
		"<A HREF=\"/goldenPath/help/customTrack.html\" "
		"TARGET=_BLANK>custom tracks</A>.  "
		"In order to keep blat results from expiring, you can "
		"periodically view them in the genome browser."
		"</P>");
    else
	dyStringPrintf(dyMessage,
		"<P>Note: the session contains an expired reference to "
		"previously saved BLAT results, so it may not appear as "
		"originally intended.</P>");
    }
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
char *encUserName = cgiEncodeFull(userName);
boolean didSomething = FALSE;
char query[512];

helList = cartFindPrefix(cart, hgsUnsharePrefix);
for (hel = helList;  hel != NULL;  hel = hel->next)
    {
    char *encSessionName = hel->name + strlen(hgsUnsharePrefix);
    char *sessionName = cgiDecodeClone(encSessionName);
    safef(query, sizeof(query), "UPDATE %s SET shared = 0 "
	  "WHERE userName = '%s' AND sessionName = '%s';",
	  namedSessionTable, encUserName, encSessionName);
    sqlUpdate(conn, query);
    sessionTouchLastUse(conn, encUserName, encSessionName);
    dyStringPrintf(dyMessage,
		   "Marked session <B>%s</B> as unshared.<BR>\n",
		   htmlEncode(sessionName));
    didSomething = TRUE;
    }

helList = cartFindPrefix(cart, hgsSharePrefix);
for (hel = helList;  hel != NULL;  hel = hel->next)
    {
    char *encSessionName = hel->name + strlen(hgsSharePrefix);
    char *sessionName = cgiDecodeClone(encSessionName);
    safef(query, sizeof(query), "UPDATE %s SET shared = 1 "
	  "WHERE userName = '%s' AND sessionName = '%s';",
	  namedSessionTable, encUserName, encSessionName);
    sqlUpdate(conn, query);
    sessionTouchLastUse(conn, encUserName, encSessionName);
    dyStringPrintf(dyMessage,
		   "Marked session <B>%s</B> as shared.<BR>\n",
		   htmlEncode(sessionName));
    didSomething = TRUE;
    }

hel = cartFindPrefix(cart, hgsLoadPrefix);
if (hel != NULL)
    {
    char *encSessionName = hel->name + strlen(hgsLoadPrefix);
    char *sessionName = cgiDecodeClone(encSessionName);
    dyStringPrintf(dyMessage,
		   "Loaded settings from session <B>%s</B>.<BR>\n",
		   htmlEncode(sessionName));
    cartLoadUserSession(conn, userName, sessionName, cart);
    checkForCustomTracks(dyMessage);
    didSomething = TRUE;
    }

helList = cartFindPrefix(cart, hgsDeletePrefix);
for (hel = helList;  hel != NULL;  hel = hel->next)
    {
    char *encSessionName = hel->name + strlen(hgsDeletePrefix);
    char *sessionName = cgiDecodeClone(encSessionName);
    safef(query, sizeof(query), "DELETE FROM %s "
	  "WHERE userName = '%s' AND sessionName = '%s';",
	  namedSessionTable, encUserName, encSessionName);
    sqlUpdate(conn, query);
    dyStringPrintf(dyMessage,
		   "Deleted session <B>%s</B>.<BR>\n",
		   htmlEncode(sessionName));
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
struct dyString *dyMessage = dyStringNew(1024);
char *otherUser = trimSpaces(cartString(cart, hgsOtherUserName));
char *sessionName = trimSpaces(cartString(cart, hgsOtherUserSessionName));

dyStringPrintf(dyMessage,
      "Loaded settings from user <B>%s</B>'s session <B>%s</B>.",
      otherUser, htmlEncode(sessionName));
cartLoadUserSession(conn, otherUser, sessionName, cart);
checkForCustomTracks(dyMessage);
hDisconnectCentral(&conn);
return dyStringCannibalize(&dyMessage);
}

void doSaveLocal()
/* Output current settings to be saved as a file on the user's machine.  
 * Return a message confirming what we did. */
{
char *fileName = trimSpaces(cartString(cart, hgsSaveLocalFileName));
char *compressType = cartString(cart, hgsSaveLocalFileCompress);
struct pipeline *compressPipe = textOutInit(fileName, compressType);

cleanHgSessionFromCart(cart);
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
    char *url = trimSpaces(cartString(cart, hgsLoadUrlName));
    if (isEmpty(url))
	errAbort("Please go back and enter the URL (http://..., ftp://...) "
		 "of a file that contains "
		 "previously saved browser settings, and then click "
		 "\"submit\" again.");
    lf = netLineFileOpen(url);
    dyStringPrintf(dyMessage, "Loaded settings from URL %s .  %s %s",
		   url, getUrlLink(url), getUrlEmailLink(url));
    }
else
    {
    char *settings = trimSpaces(cartString(cart, hgsLoadLocalFileName));
    dyStringPrintf(dyMessage, "Loaded settings from local file (%lu bytes).",
		   (unsigned long)strlen(settings));
    lf = lineFileOnString("settingsFromFile", TRUE, cloneString(settings));
    }
cartLoadSettings(lf, cart);
checkForCustomTracks(dyMessage);
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

cleanHgSessionFromCart(cart);
/* Save the cart state: */
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

