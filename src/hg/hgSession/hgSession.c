/* hgSession - Interface with wiki login and do session saving/loading. */

#include "common.h"
#include "hash.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "linefile.h"
#include "net.h"
#include "textOut.h"
#include "hCommon.h"
#include "hui.h"
#include "cart.h"
#include "jsHelper.h"
#include "web.h"
#include "hdb.h"
#include "ra.h"
#include "wikiLink.h"
#include "customTrack.h"
#include "customFactory.h"
#include "hgSession.h"

static char const rcsid[] = "$Id: hgSession.c,v 1.53 2010/03/05 22:26:29 angie Exp $";

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
struct slName *existingSessionNames = NULL;

/* Javascript to confirm that the user truly wants to delete a session. */
#define confirmDeleteFormat "return confirm('Are you sure you want to delete %s?');"

char *cgiDecodeClone(char *encStr)
/* Allocate and return a CGI-decoded copy of encStr. */
{
size_t len = strlen(encStr);
char *decStr = needMem(len+1);
cgiDecode(encStr, decStr, len);
return decStr;
}


void welcomeUser(char *wikiUserName)
/* Tell the user they are not logged in to the wiki and tell them how to
 * do so. */
{
char *wikiHost = wikiLinkHost();

cartWebStart(cart, NULL, "Welcome %s", wikiUserName);
jsInit();
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

cartWebStart(cart, NULL, "Sign in to UCSC Genome Bioinformatics");
jsInit();
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

safef(returnAddress, sizeof(returnAddress), "%s?%s", hgSessionName(), session);
printf("<A HREF=\"../cgi-bin/cartReset?%s&destination=%s\">Click here to "
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
	       cgiServerNamePort(), destAppScriptName(), userName, sessionName);
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

char *getSessionLink(char *encUserName, char *encSessionName)
/* Form a link that will take the user to a bookmarkable page that
 * will load the given session. */
{
struct dyString *dy = dyStringNew(1024);
dyStringPrintf(dy, "<A HREF=\"");
addSessionLink(dy, encUserName, encSessionName, FALSE);
dyStringPrintf(dy, "\">Browser</A>\n");
return dyStringCannibalize(&dy);
}

char *getSessionEmailLink(char *encUserName, char *encSessionName)
/* Invoke mailto: with a cgi-encoded link that will take the user to a
 * bookmarkable page that will load the given session. */
{
struct dyString *dy = dyStringNew(1024);
dyStringPrintf(dy, "<A HREF=\"mailto:?subject=UCSC browser session %s&"
	       "body=Here is a UCSC browser session I%%27d like to share with "
	       "you:%%20",
	       cgiDecodeClone(encSessionName));
addSessionLink(dy, encUserName, encSessionName, TRUE);
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
	       cgiServerNamePort(), destAppScriptName(), encodedUrl);
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
	       "body=Here is a UCSC browser session I%%27d like to share with "
	       "you:%%20");
addUrlLink(dy, url, TRUE);
dyStringPrintf(dy, "\">Email</A>\n");
return dyStringCannibalize(&dy);
}

static char *getSetting(char *settings, char *name)
/* Dig out one setting from a settings string that we're only going to
 * look at once (so we don't keep the hash around). */
{
if (isEmpty(settings))
    return NULL;
struct hash *settingsHash = raFromString(settings);
char *val = cloneString(hashFindVal(settingsHash, name));
hashFree(&settingsHash);
return val;
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
boolean gotSettings = (sqlFieldIndex(conn, namedSessionTable, "settings") >= 0);

printf("<H3>My Sessions</H3>\n");
printf("<TABLE BORDERWIDTH=0>\n");
safef(query, sizeof(query), "SELECT sessionName, shared, firstUse from %s "
      "WHERE userName = '%s' ORDER BY sessionName;",
      namedSessionTable, encUserName);
sr = sqlGetResult(conn, query);
printf("<TH><TD><B>session name</B></TD><TD><B>created on</B></TD><TD align=center><B>use this&nbsp;<BR>session&nbsp;</B></TD>"
       "<TD align=center><B>delete this&nbsp;<BR>session&nbsp;</B></TD><TD align=center><B>share with&nbsp;<BR>others?&nbsp;</B></TD><TD align=center><B>link to<BR>session</B></TD>"
       "<TD align=center><B>send to<BR>mail</B></TD></TH>");
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *encSessionName = row[0];
    char *sessionName = cgiDecodeClone(encSessionName);
    char *link = NULL;
    boolean shared = atoi(row[1]);
    char *firstUse = row[2];
    char buf[512];
    printf("<TR><TD>&nbsp;&nbsp;</TD><TD>");
    if (gotSettings)
	printf("<A HREF=\"%s?%s&%s=%s\">",
	       hgSessionName(), cartSidUrlString(cart), hgsDoSessionDetail, encSessionName);
    htmlTextOut(sessionName);
    char *spacePt = strchr(firstUse, ' ');
    if (spacePt != NULL) *spacePt = '\0';
    if (gotSettings)
	printf("</A>");
    printf("&nbsp;&nbsp;</TD>"
	   "<TD>%s&nbsp;&nbsp;</TD><TD align=center>", firstUse);
    safef(buf, sizeof(buf), "%s%s", hgsLoadPrefix, encSessionName);
    cgiMakeButton(buf, "use");
    printf("</TD><TD align=center>");
    safef(buf, sizeof(buf), "%s%s", hgsDeletePrefix, encSessionName);
    char command[512];
    safef(command, sizeof(command), confirmDeleteFormat, sessionName);
    cgiMakeOnClickSubmitButton(command, buf, "delete");
    printf("</TD><TD align=center>");
    safef(buf, sizeof(buf), "%s%s", hgsSharePrefix, encSessionName);
    cgiMakeCheckBoxJS(buf, shared, "onchange=\"document.mainForm.submit();\"");
    link = getSessionLink(encUserName, encSessionName);
    printf("</TD><TD align=center>%s</TD>\n", link);
    freez(&link);
    link = getSessionEmailLink(encUserName, encSessionName);
    printf("<TD align=center>%s</TD></TR>", link);
    freez(&link);
    foundAny = TRUE;
    struct slName *sn = slNameNew(sessionName);
    slAddHead(&existingSessionNames, sn);
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
       "Use settings from another user's saved session:</TD></TR>\n"
       "<TR><TD>&nbsp;&nbsp;&nbsp;</TD><TD>user: \n");
cgiMakeOnKeypressTextVar(hgsOtherUserName,
			 cartUsualString(cart, hgsOtherUserName, ""),
			 20, "return noSubmitOnEnter(event);");
printf("&nbsp;&nbsp;&nbsp;session name: \n");
cgiMakeOnKeypressTextVar(hgsOtherUserSessionName,
			 cartUsualString(cart, hgsOtherUserSessionName, ""),
			 20, jsPressOnEnter(hgsDoOtherUser));
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
printf("<H3>Restore Settings</H3>\n");
if (savedSessionsSupported)
    showOtherUserOptions();

printf("<TABLE BORDERWIDTH=0>\n");
printf("<TR><TD colspan=2>Use settings from a local file:</TD>\n");
printf("<TD><INPUT TYPE=FILE NAME=\"%s\" "
       "onkeypress=\"return noSubmitOnEnter(event);\">\n",
       hgsLoadLocalFileName);
printf("&nbsp;&nbsp;");
cgiMakeButton(hgsDoLoadLocal, "submit");
printf("</TD></TR>\n");
printf("<TR><TD colspan=2></TD></TR>\n");

printf("<TR><TD colspan=2>Use settings from a URL (http://..., ftp://...):"
       "</TD>\n");
printf("<TD>\n");
cgiMakeOnKeypressTextVar(hgsLoadUrlName,
			 cartUsualString(cart, hgsLoadUrlName, ""),
			 20, jsPressOnEnter(hgsDoLoadUrl));
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
    cgiMakeOnKeypressTextVar(hgsNewSessionName,
			     cartUsualString(cart, "db", NULL),
			     20, jsPressOnEnter(hgsDoNewSession));
    printf("&nbsp;&nbsp;&nbsp;");
    cgiMakeCheckBox(hgsNewSessionShare,
		    cartUsualBoolean(cart, hgsNewSessionShare, TRUE));
    printf("allow this session to be loaded by others\n");
    printf("</TD><TD>");
    printf("&nbsp;");
    if (existingSessionNames)
	{
	struct dyString *js = dyStringNew(1024);
	struct slName *sn;
	dyStringAppend(js, "var si = document.getElementsByName('" hgsNewSessionName "'); ");
	dyStringAppend(js, "if (si[0] && ( ");
	for (sn = existingSessionNames;  sn != NULL;  sn = sn->next)
	    {
	    dyStringPrintf(js, "si[0].value == ");
	    dyStringQuoteString(js, '\'', sn->name);
	    dyStringPrintf(js, "%s", (sn->next ? " || " : " )) { "));
	    }
	dyStringAppend(js, "return confirm('This will overwrite the contents of the existing "
		       "session ' + si[0].value + '.  Proceed?'); ");
	dyStringAppend(js, "}");
	cgiMakeOnClickSubmitButton(js->string, hgsDoNewSession, "submit");
	dyStringFree(&js);
	}
    else
	cgiMakeButton(hgsDoNewSession, "submit");
    printf("</TD></TR>\n");
    printf("<TR><TD colspan=4></TD></TR>\n");
    }

printf("<TR><TD colspan=4>Save current settings to a local file:</TD></TR>\n");
printf("<TR><TD>&nbsp;&nbsp;&nbsp;</TD><TD>file:</TD><TD>\n");
cgiMakeOnKeypressTextVar(hgsSaveLocalFileName,
			 cartUsualString(cart, hgsSaveLocalFileName, ""),
			 20, jsPressOnEnter(hgsDoSaveLocal));
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
    {
    cartWebStart(cart, NULL, "Session Management");
    jsInit();
    }

printf("See the <A HREF=\"../goldenPath/help/hgSessionHelp.html\" "
       "TARGET=_BLANK>Sessions User's Guide</A> "
       "for more information about this tool.<P/>\n");

showCartLinks();

printf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=%s "
       "ENCTYPE=\"multipart/form-data\">\n",
       hgSessionName(), formMethod);
cartSaveSession(cart);

if (isNotEmpty(userName))
    showExistingSessions(userName);
else if (savedSessionsSupported)
    printf("<P>If you <A HREF=\"%s\">sign in</A>, "
	   "you will also have the option to save named sessions.\n",
	   wikiLinkUserLoginUrl(cartSessionId(cart)));
showSavingOptions(userName);
showLoadingOptions(userName, savedSessionsSupported);
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
dyStringPrintf(dyUrl, "http://%s%s", cgiServerNamePort(), cgiScriptName());

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
	if (cartVarExists(cart, hgsDoSessionDetail))
	    webNewSection("Session Details");
	else
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
	if (cartVarExists(cart, hgsDoSessionDetail))
	    webNewSection("Session Details");
	else
	    cartWebStart(cart, NULL, "Updated Session");
	jsInit();
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
char varName[256];
safef(varName, sizeof(varName), "%s%s", cgiBooleanShadowPrefix(), hgsSharePrefix);
cartRemovePrefix(cart, varName);
cartRemovePrefix(cart, hgsSharePrefix);
cartRemovePrefix(cart, hgsLoadPrefix);
cartRemovePrefix(cart, hgsLoadLocalFileName);
cartRemovePrefix(cart, hgsDeletePrefix);
cartRemovePrefix(cart, hgsDo);
cartRemove(cart, hgsOldSessionName);
cartRemove(cart, hgsCancel);
}

void checkForCustomTracks(struct dyString *dyMessage);

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
    dyStringFree(&dy);

    if (useCount > INITIAL_USE_COUNT)
	dyStringPrintf(dyMessage,
	  "Overwrote the contents of session <B>%s</B> "
	  "(that %s be shared with other users).  "
	  "%s %s",
	  htmlEncode(sessionName), (shareSession ? "may" : "may not"),
	  getSessionLink(encUserName, encSessionName),
	  getSessionEmailLink(encUserName, encSessionName));
    else
	dyStringPrintf(dyMessage,
	  "Added a new session <B>%s</B> that %s be shared with other users.  "
	  "%s %s",
	  htmlEncode(sessionName), (shareSession ? "may" : "may not"),
	  getSessionLink(encUserName, encSessionName),
	  getSessionEmailLink(encUserName, encSessionName));
    checkForCustomTracks(dyMessage);
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
	/* If the file doesn't exist, just remove the cart variable so it
	 * doesn't get copied from session to session.  If it does exist,
	 * leave it up to customFactoryTestExistence to parse the file for
	 * possible customTrash table references, some of which may exist
	 * and some not. */
	if (!fileExists(hel->val))
	    {
	    cartRemove(cart, hel->name);
	    thisGotExpiredCT = TRUE;
	    }
	else
	    {
	    customFactoryTestExistence(db, hel->val,
				       &thisGotLiveCT, &thisGotExpiredCT);
	    }
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
	    dyStringPrintf(dyMessage, "<A HREF=\"hgCustom?%s&db=%s\">%s</A>%s",
			   cartSidUrlString(cart), sln->name,
			   sln->name, (sln->next ? sln->next->next ? ", " : " and " : ""));
	dyStringAppend(dyMessage, "; click on the database link "
		       "to manage custom tracks).  ");

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
		       "), so it may not appear as originally intended.  ");
	}
    dyStringPrintf(dyMessage,
		   "Custom tracks are subject to an expiration policy described in the "
		   "<A HREF=\"../goldenPath/help/hgSessionHelp.html#CTs\" TARGET=_BLANK>"
		   "Session documentation</A>.</P>");
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
    safecpy(buf, sizeof(buf), ss);
    wordCount = chopLine(buf, words);
    if (wordCount < 2)
	exists = FALSE;
    else
	exists = fileExists(words[0]) && fileExists(words[1]);

    if (exists)
	dyStringPrintf(dyMessage,
		       "<P>Note: the session contains BLAT results.  ");
    else
	dyStringPrintf(dyMessage,
		"<P>Note: the session contains an expired reference to "
		"previously saved BLAT results, so it may not appear as "
		"originally intended.  ");
    dyStringPrintf(dyMessage,
		   "BLAT results are subject to an "
		   "<A HREF=\"../goldenPath/help/hgSessionHelp.html#CTs\" TARGET=_BLANK>"
		   "expiration policy</A>.");
    }
}

char *doUpdateSessions()
/* Look for cart variables matching prefixes for sharing/unsharing,
 * loading or deleting a previously saved session.
 * Return a message confirming what we did, or NULL if no such variables
 * were in the cart. */
{
struct dyString *dyMessage = dyStringNew(1024);
struct hashEl *cartHelList = NULL, *hel = NULL;
struct sqlConnection *conn = hConnectCentral();
char *userName = wikiLinkUserName();
char *encUserName = cgiEncodeFull(userName);
boolean didSomething = FALSE;
char query[512];

cartHelList = cartFindPrefix(cart, hgsSharePrefix);
if (cartHelList != NULL)
    {
    struct hash *sharedHash = hashNew(0);
    char **row;
    struct sqlResult *sr;
    safef(query, sizeof(query),
	  "select sessionName,shared from %s where userName = '%s'",
	  namedSessionTable, encUserName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	hashAddInt(sharedHash, row[0], atoi(row[1]));
    sqlFreeResult(&sr);
    for (hel = cartHelList;  hel != NULL;  hel = hel->next)
	{
	char *encSessionName = hel->name + strlen(hgsSharePrefix);
	char *sessionName = cgiDecodeClone(encSessionName);
	boolean alreadyShared = hashIntVal(sharedHash, encSessionName);
	boolean shared  = cartUsualBoolean(cart, hel->name, TRUE);
	if (shared != alreadyShared)
	    {
	    safef(query, sizeof(query), "UPDATE %s SET shared = %d "
		  "WHERE userName = '%s' AND sessionName = '%s';",
		  namedSessionTable, shared, encUserName, encSessionName);
	    sqlUpdate(conn, query);
	    sessionTouchLastUse(conn, encUserName, encSessionName);
	    dyStringPrintf(dyMessage,
			   "Marked session <B>%s</B> as %s.<BR>\n",
			   htmlEncode(sessionName),
			   (shared ? "shared" : "unshared"));
	    didSomething = TRUE;
	    }
	}
    hashFree(&sharedHash);
    }
hel = cartFindPrefix(cart, hgsLoadPrefix);
if (hel != NULL)
    {
    char *encSessionName = hel->name + strlen(hgsLoadPrefix);
    char *sessionName = cgiDecodeClone(encSessionName);
    char wildStr[256];
    safef(wildStr, sizeof(wildStr), "%s*", hgsLoadPrefix);
    dyStringPrintf(dyMessage,
		   "Loaded settings from session <B>%s</B>. %s %s<BR>\n",
		   htmlEncode(sessionName),
		   getSessionLink(encUserName, encSessionName),
		   getSessionEmailLink(encUserName, encSessionName));
    cartLoadUserSession(conn, userName, sessionName, cart, NULL, wildStr);
    checkForCustomTracks(dyMessage);
    didSomething = TRUE;
    }

cartHelList = cartFindPrefix(cart, hgsDeletePrefix);
for (hel = cartHelList;  hel != NULL;  hel = hel->next)
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

char *doOtherUser(char *actionVar)
/* Load settings from another user's named session.
 * Return a message confirming what we did. */
{
struct sqlConnection *conn = hConnectCentral();
struct dyString *dyMessage = dyStringNew(1024);
char *otherUser = trimSpaces(cartString(cart, hgsOtherUserName));
char *sessionName = trimSpaces(cartString(cart, hgsOtherUserSessionName));
char *encOtherUser = cgiEncodeFull(otherUser);
char *encSessionName = cgiEncodeFull(sessionName);

dyStringPrintf(dyMessage,
       "Loaded settings from user <B>%s</B>'s session <B>%s</B>. %s %s",
	       otherUser, htmlEncode(sessionName),
	       getSessionLink(otherUser, encSessionName),
	       getSessionEmailLink(encOtherUser, encSessionName));
cartLoadUserSession(conn, otherUser, sessionName, cart, NULL, actionVar);
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

char *doLoad(boolean fromUrl, char *actionVar)
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
    char *filePlainContents = cartOptionalString(cart, hgsLoadLocalFileName);
    char *fileBinaryCoords = cartOptionalString(cart,
					hgsLoadLocalFileName "__binary");
    char *fileName = cartOptionalString(cart,
					hgsLoadLocalFileName "__filename");
    if (isNotEmpty(filePlainContents))
	{
	char *settings = trimSpaces(filePlainContents);
	dyStringAppend(dyMessage, "Loaded settings from local file ");
	if (isNotEmpty(fileName))
	    dyStringPrintf(dyMessage, "<B>%s</B> ", fileName);
	dyStringPrintf(dyMessage, "(%lu bytes).",
		       (unsigned long)strlen(settings));
	lf = lineFileOnString("settingsFromFile", TRUE, cloneString(settings));
	}
    else if (isNotEmpty(fileBinaryCoords))
	{
	char *binInfo = cloneString(fileBinaryCoords);
	char *words[2];
	char *mem;
	unsigned long size;
	chopByWhite(binInfo, words, ArraySize(words));
	mem = (char *)sqlUnsignedLong(words[0]);
	size = sqlUnsignedLong(words[1]);
	lf = lineFileDecompressMem(TRUE, mem, size);
	if (lf != NULL)
	    {
	    dyStringAppend(dyMessage, "Loaded settings from local file ");
	    if (isNotEmpty(fileName))
		dyStringPrintf(dyMessage, "<B>%s</B> ", fileName);
	    dyStringPrintf(dyMessage, "(%lu bytes).", size);
	    }
	else
	    dyStringPrintf(dyMessage,
			   "Sorry, I don't recognize the file type of "
			   "<B>%s</B>.  Please submit plain text or "
			   "compressed text in one of the formats offered in "
			   "<B>Save Settings</B>.", fileName);
	}
    else
	{
	dyStringAppend(dyMessage, "Sorry, your web browser seems to have "
		       "posted no data");
	if (isNotEmpty(fileName))
	    dyStringPrintf(dyMessage, ", only the filename <B>%s</B>",
			   fileName);
	dyStringAppend(dyMessage, ".  Your settings have not been changed.");
	lf = NULL;
	}
    dyStringPrintf(dyMessage, "&nbsp;&nbsp;"
	   "<A HREF=\"http://%s%s?%s=%u\">Browser</A>",
	   cgiServerNamePort(), destAppScriptName(),
	   cartSessionVarName(), cartSessionId(cart));
    }
if (lf != NULL)
    {
    cartLoadSettings(lf, cart, NULL, actionVar);
    checkForCustomTracks(dyMessage);
    lineFileClose(&lf);
    }
return dyStringCannibalize(&dyMessage);
}

char *doSessionDetail(char *sessionName)
/* Show details about a particular session. */
{
struct dyString *dyMessage = dyStringNew(4096);
char *encSessionName = cgiEncodeFull(sessionName);
char *userName = wikiLinkUserName();
char *encUserName = cgiEncodeFull(userName);
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr = NULL;
char **row = NULL;
char query[512];
webPushErrHandlersCart(cart);
boolean gotSettings = (sqlFieldIndex(conn, namedSessionTable, "settings") >= 0);

if (gotSettings)
    safef(query, sizeof(query), "SELECT shared, firstUse, settings from %s "
	  "WHERE userName = '%s' AND sessionName = '%s'",
          namedSessionTable, encUserName, encSessionName);
else
    safef(query, sizeof(query), "SELECT shared, firstUse from %s "
	  "WHERE userName = '%s' AND sessionName = '%s'",
          namedSessionTable, encUserName, encSessionName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    boolean shared = atoi(row[0]);
    char *firstUse = row[1];
    char *settings = NULL;
    if (gotSettings)
	settings = row[2];
    char *description = getSetting(settings, "description");
    if (description == NULL) description = "";

    dyStringPrintf(dyMessage, "<A HREF=\"../goldenPath/help/hgSessionHelp.html#Details\" "
		   "TARGET=_BLANK>Session Details Help</A><P/>\n");

#define highlightAccChanges "{ var b = document.getElementById('" hgsDoSessionChange "'); " \
                            "  if (b) { b.style.background = '#ff9999'; } }"

    dyStringPrintf(dyMessage, "<B>%s</B><P>\n"
		   "<FORM ACTION=\"%s\" NAME=\"detailForm\" METHOD=GET>\n"
		   "<INPUT TYPE=HIDDEN NAME=\"%s\" VALUE=%u>"
		   "<INPUT TYPE=HIDDEN NAME=\"%s\" VALUE=\"%s\">"
		   "Session Name: "
		   "<INPUT TYPE=TEXT NAME=\"%s\" SIZE=%d VALUE=\"%s\" "
		   "onChange=\"%s\" onKeypress=\"%s\">\n",
		   sessionName, hgSessionName(),
		   cartSessionVarName(cart), cartSessionId(cart), hgsOldSessionName, sessionName,
		   hgsNewSessionName, 32, sessionName, highlightAccChanges, highlightAccChanges);
    dyStringPrintf(dyMessage,
		   "&nbsp;&nbsp;<INPUT TYPE=SUBMIT NAME=\"%s%s\" VALUE=\"use\">"
		   "&nbsp;&nbsp;<INPUT TYPE=SUBMIT NAME=\"%s%s\" VALUE=\"delete\" "
		   "onClick=\"" confirmDeleteFormat "\">"
		   "&nbsp;&nbsp;<INPUT TYPE=SUBMIT ID=\"%s\" NAME=\"%s\" VALUE=\"accept changes\">"
		   "&nbsp;&nbsp;<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"cancel\"> "
		   "<BR>\n",
		   hgsLoadPrefix, encSessionName, hgsDeletePrefix, encSessionName,
		   sessionName, hgsDoSessionChange, hgsDoSessionChange, hgsCancel);
    dyStringPrintf(dyMessage,
		   "Share with others? <INPUT TYPE=CHECKBOX NAME=\"%s%s\"%s VALUE=on "
		   "onChange=\"%s\" onClick=\"%s\">\n"
		   "<INPUT TYPE=HIDDEN NAME=\"%s%s%s\" VALUE=0><BR>\n",
		   hgsSharePrefix, encSessionName, (shared ? " CHECKED" : ""),
		   highlightAccChanges, highlightAccChanges,
		   cgiBooleanShadowPrefix(), hgsSharePrefix, encSessionName);
    dyStringPrintf(dyMessage,
		   "Created on %s.<BR>\n", firstUse);
    if (gotSettings)
	{
	description = replaceChars(description, "\\\\", "\\__ESC__");
	description = replaceChars(description, "\\r", "\r");
	description = replaceChars(description, "\\n", "\n");
	description = replaceChars(description, "\\__ESC__", "\\");
	dyStringPrintf(dyMessage,
		       "Description:<BR>\n"
		       "<TEXTAREA NAME=\"%s\" ROWS=%d COLS=%d "
		       "onChange=\"%s\" onKeypress=\"%s\">%s</TEXTAREA><BR>\n",
		       hgsNewSessionDescription, 5, 80,
		       highlightAccChanges, highlightAccChanges, description);
	}
    dyStringAppend(dyMessage, "</FORM>\n");
    sqlFreeResult(&sr);
    }
else
    errAbort("doSessionDetail: got no results from query:<BR>\n%s\n", query);

return dyStringCannibalize(&dyMessage);
}

void renamePrefixedCartVar(char *prefix, char *oldName, char *newName)
/* If cart has prefix+oldName, replace it with prefix+newName = submit. */
{
char varName[256];
safef(varName, sizeof(varName), "%s%s", prefix, oldName);
if (cartVarExists(cart, varName))
    {
    cartRemove(cart, varName);
    safef(varName, sizeof(varName), "%s%s", prefix, newName);
    cartSetString(cart, varName, "submit");
    }
}

char *doSessionChange(char *oldSessionName)
/* Process changes to session from session details page. */
{
struct dyString *dyMessage = dyStringNew(1024);
webPushErrHandlersCart(cart);
char *sessionName = oldSessionName;
char *encSessionName = cgiEncodeFull(sessionName);
char *encOldSessionName = encSessionName;
char *userName = wikiLinkUserName();
char *encUserName = cgiEncodeFull(userName);
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr = NULL;
char **row = NULL;
char query[512];
boolean shared = TRUE;
char *settings = NULL;
boolean gotSettings = (sqlFieldIndex(conn, namedSessionTable, "settings") >= 0);

if (gotSettings)
    safef(query, sizeof(query), "SELECT shared, settings from %s "
	  "WHERE userName = '%s' AND sessionName = '%s'",
          namedSessionTable, encUserName, encSessionName);
else
    safef(query, sizeof(query), "SELECT shared from %s "
	  "WHERE userName = '%s' AND sessionName = '%s'",
          namedSessionTable, encUserName, encSessionName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    shared = atoi(row[0]);
    if (gotSettings)
	settings = cloneString(row[1]);
    sqlFreeResult(&sr);
    }
else
    errAbort("doSessionChange: got no results from query:<BR>\n%s\n", query);

char *newName = cartOptionalString(cart, hgsNewSessionName);
if (isNotEmpty(newName) && !sameString(sessionName, newName))
    {
    char *encNewName = cgiEncodeFull(newName);
    safef(query, sizeof(query),
	  "UPDATE %s set sessionName = '%s' WHERE userName = '%s' AND sessionName = '%s';",
	  namedSessionTable, encNewName, encUserName, encSessionName);
	sqlUpdate(conn, query);
    dyStringPrintf(dyMessage, "Changed session name from %s to <B>%s</B>.\n",
		   sessionName, newName);
    sessionName = newName;
    encSessionName = encNewName;
    renamePrefixedCartVar(hgsLoadPrefix, encOldSessionName, encNewName);
    renamePrefixedCartVar(hgsDeletePrefix, encOldSessionName, encNewName);
    }
char varName[256];
safef(varName, sizeof(varName), hgsSharePrefix "%s", encOldSessionName);
if (cgiBooleanDefined(varName))
    {
    boolean newShared = cartBoolean(cart, varName);
    if (newShared != shared)
	{
	safef(query, sizeof(query),
	      "UPDATE %s set shared = %d WHERE userName = '%s' AND sessionName = '%s';",
	      namedSessionTable, newShared, encUserName, encSessionName);
	sqlUpdate(conn, query);
	dyStringPrintf(dyMessage, "Marked session <B>%s</B> as %s.<BR>\n",
		       htmlEncode(sessionName), (newShared ? "shared" : "unshared"));

	}
    cartRemove(cart, varName);
    char shadowVarName[512];
    safef(shadowVarName, sizeof(shadowVarName), "%s%s", cgiBooleanShadowPrefix(), varName);
    cartRemove(cart, shadowVarName);
    }
if (gotSettings)
    {
    struct hash *settingsHash = raFromString(settings);
    char *description = hashFindVal(settingsHash, "description");
    char *newDescription = cartOptionalString(cart, hgsNewSessionDescription);
    if (newDescription != NULL)
	{
	newDescription = replaceChars(newDescription, "\\", "\\\\\\\\");
	newDescription = replaceChars(newDescription, "\r", "\\\\r");
	newDescription = replaceChars(newDescription, "\n", "\\\\n");
	}
    else
	newDescription = "";
    if (description != NULL)
	description = replaceChars(description, "\\", "\\\\");
    else
	description = "";
    if (!sameString(description, newDescription))
	{
	hashRemove(settingsHash, "description");
	hashAdd(settingsHash, "description", newDescription);
	struct dyString *dyRa = dyStringNew(512);
	struct hashEl *hel = hashElListHash(settingsHash);
	while (hel != NULL)
	    {
	    if (sameString(hel->name, "description"))
		dyStringPrintf(dyRa, "%s %s\n", hel->name, newDescription);
	    else
		dyStringPrintf(dyRa, "%s %s\n", hel->name, (char *)hel->val);
	    hel = hel->next;
	    }
	struct dyString *dyQuery = dyStringNew(1024);
	dyStringPrintf(dyQuery, "UPDATE %s set settings = ", namedSessionTable);
	dyStringQuoteString(dyQuery, '"', dyRa->string);
	dyStringPrintf(dyQuery, "WHERE userName = '%s' AND sessionName = '%s';",
		       encUserName, encSessionName);
	sqlUpdate(conn, dyQuery->string);
	dyStringPrintf(dyMessage, "Updated description of <B>%s</B>.\n", sessionName);
	}
    }
if (isEmpty(dyMessage->string))
    dyStringPrintf(dyMessage, "No changes to session <B>%s</B>.\n", sessionName);
dyStringPrintf(dyMessage, "%s %s",
	       getSessionLink(encUserName, encSessionName),
	       getSessionEmailLink(encUserName, encSessionName));
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

if (cartVarExists(cart, hgsDoMainPage) || cartVarExists(cart, hgsCancel))
    doMainPage(NULL);
else if (cartVarExists(cart, hgsDoNewSession))
    {
    char *message = doNewSession();
    doMainPage(message);
    }
else if (cartVarExists(cart, hgsDoOtherUser))
    {
    char *message = doOtherUser(hgsDoOtherUser);
    doMainPage(message);
    }
else if (cartVarExists(cart, hgsDoSaveLocal))
    {
    doSaveLocal();
    }
else if (cartVarExists(cart, hgsDoLoadLocal))
    {
    char *message = doLoad(FALSE, hgsDoLoadLocal);
    doMainPage(message);
    }
else if (cartVarExists(cart, hgsDoLoadUrl))
    {
    char *message = doLoad(TRUE, hgsDoLoadUrl);
    doMainPage(message);
    }
else if (cartVarExists(cart, hgsDoSessionDetail))
    {
    char *message = doSessionDetail(cartString(cart, hgsDoSessionDetail));
    doMainPage(message);
    }
else if (cartVarExists(cart, hgsDoSessionChange))
    {
    char *message = doSessionChange(cartString(cart, hgsOldSessionName));
    doMainPage(message);
    }
else if (cartVarExists(cart, hgsOldSessionName))
    {
    char *message1 = doSessionChange(cartString(cart, hgsOldSessionName));
    char *message2 = doUpdateSessions();
    char *message = message2;
    if (!startsWith("No changes to session", message1))
	{
	size_t len = (sizeof message1[0]) * (strlen(message1) + strlen(message2) + 1);
	message = needMem(len);
	safef(message, len, "%s%s", message1, message2);
	}
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

