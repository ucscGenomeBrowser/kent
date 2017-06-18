/* hgSession - Interface with wiki login and do session saving/loading. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* WARNING: testing this CGI on hgwbeta can lead to missed bugs. This is
 * because on hgwbeta, the login links go just to hgwbeta. But on genome-euro
 * and genome-asia, the login links go to genome.ucsc.edu and session links to 
 * genome-euro/asia. For proper testing of session loading on hgSession and
 * hgLogin, configure your sandbox to use genome.ucsc.edu as a "remote login"
 * (wiki.host=genome.ucsc.edu). Make sure that links to load sessions go to
 * your sandbox, but login links go to genome.ucsc.edu. For QA, tell them to
 * use genome-preview to test this CGI. */

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
#include "udc.h"
#include "hgSession.h"
#include "hubConnect.h"
#include "hgConfig.h"
#include "sessionThumbnail.h"
#include "obscure.h"

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
#define confirmDeleteFormat "return confirm('Are you sure you want to delete ' + decodeURIComponent('%s') + '?');"

char *cgiDecodeClone(char *encStr)
/* Allocate and return a CGI-decoded copy of encStr. */
{
size_t len = strlen(encStr);
char *decStr = needMem(len+1);
cgiDecode(encStr, decStr, len);
return decStr;
}


void welcomeUser(char *wikiUserName)
/* Tell the user they are not logged in to the wiki or other login
 * system and tell them how to do so. */
{
char *wikiHost = wikiLinkHost();

cartWebStart(cart, NULL, "Welcome %s", wikiUserName);
jsInit();

jsIncludeDataTablesLibs();

if (loginSystemEnabled()) /* Using the new hgLogin CGI for login */
    {
    printf("<h4 style=\"margin: 0pt 0pt 7px;\">Your Account Information</h4>"
        "<ul style=\"list-style: none outside none; margin: 0pt; padding: 0pt;\">"
        "<li>Username:  %s</li>",wikiUserName);

    if (loginUseBasicAuth())
        printf("<li>The Genome Browser is configured to use HTTP Basic Authentication, so the password cannot be changed here.</li></ul>");
    else
        printf("<li><A HREF=\"%s\">Change password</A></li></ul>",
            wikiLinkChangePasswordUrl(cartSessionId(cart)));

    printf("<p><A id='logoutLink' HREF=\"%s\">Sign out</A></p>",
        wikiLinkUserLogoutUrl(cartSessionId(cart)));

    if (loginUseBasicAuth())
            wikiFixLogoutLinkWithJs();
    }

else
/* this part is not used anymore at UCSC since 2014 */
    {
    printf("If you are not %s (on the wiki at "
        "<A HREF=\"http://%s/\" TARGET=_BLANK>%s</A>) "
        "and would like to sign out or change identity, \n",
        wikiUserName, wikiHost, wikiHost);
    printf("<A HREF=\"%s\"><B>click here to sign out.</B></A>\n",
        wikiLinkUserLogoutUrl(cartSessionId(cart)));
    }
}

void offerLogin()
/* Tell the user they are not logged in to the system and tell them how to
 * do so. */
{
char *wikiHost = wikiLinkHost();

cartWebStart(cart, NULL, "Sign in to UCSC Genome Bioinformatics");
jsInit();
if (loginSystemEnabled())
    {
   printf("<ul style=\"list-style: none outside none; margin: 0pt; padding: 0pt;\">"
"<li><A HREF=\"%s\">Login</A></li>",
        wikiLinkUserLoginUrl(cartSessionId(cart)));
    printf("<li><A HREF=\"%s\">"
        "Create an account</A></li></ul>",
        wikiLinkUserSignupUrl(cartSessionId(cart)));
    printf("<P>Signing in enables you to save current settings into a "
        "named session, and then restore settings from the session later. <BR>"
        "If you wish, you can share named sessions with other users.</P>");
    }    
else
    // the following block is not used at UCSC anymore since 2014
    {
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

void addSessionLink(struct dyString *dy, char *userName, char *sessionName,
		    boolean encode)
/* Add to dy an URL that tells hgSession to load a saved session.
 * If encode, cgiEncodeFull the URL. 
 * The link is an absolute link that includes the server name so people can
 * copy-paste it into emails.  */
{
struct dyString *dyTmp = dyStringNew(1024);
dyStringPrintf(dyTmp, "%shgTracks?hgS_doOtherUser=submit&"
	       "hgS_otherUserName=%s&hgS_otherUserSessionName=%s",
	       hLocalHostCgiBinUrl(), userName, sessionName);
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
dyStringPrintf(dyTmp, "%shgTracks?hgS_doLoadUrl=submit&hgS_loadUrlName=%s",
	       hLocalHostCgiBinUrl(), encodedUrl);
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

/* DataTables configuration: only allow ordering on session name, creation date, and database.
 * https://datatables.net/reference/option/columnDefs */
jsInlineF(
        "if (theClient.isIePre11() === false)\n{\n"
        "$(document).ready(function () {\n"
        "    $('#sessionTable').DataTable({\"columnDefs\": [{\"orderable\":false, \"targets\":[0,4,5,6,7,8]}],\n"
        "                                       \"order\":[1,'asc'],\n"
        "                                       \"stateSave\":true,\n"
        "                                       \"stateSaveCallback\": %s,\n"
        "                                       \"stateLoadCallback\": %s\n"
        "                                 });\n"
        "} );\n"
        "}\n"
        , jsDataTableStateSave(hgSessionPrefix), jsDataTableStateLoad(hgSessionPrefix, cart));

printf("<H3>My Sessions</H3>\n");
printf("<div style=\"max-width:1024px\">");
printf("<table id=\"sessionTable\" class=\"sessionTable stripe hover row-border compact\" borderwidth=0>\n");
printf("<thead><tr>");
printf("<TH><TD><B>session name (click to load)</B></TD><TD><B>created on</B></TD><td><b>assembly</b></td>"
       "<TD align=center><B>view/edit&nbsp;<BR>details&nbsp;</B></TD>"
       "<TD align=center><B>delete this&nbsp;<BR>session&nbsp;</B></TD><TD align=center><B>share with&nbsp;<BR>others?&nbsp;</B></TD>"
       "<td align-center><b>post in&nbsp;<br><a href=\"../cgi-bin/hgPublicSessions?%s\">public listing</a>?</b></td>"
       "<TD align=center><B>send to<BR>mail</B></TD></TH>",
       cartSidUrlString(cart));
printf("</tr></thead>");
printf("<tbody>\n");

if (gotSettings)
    sqlSafef(query, sizeof(query), "SELECT sessionName, shared, firstUse, contents, settings from %s "
        "WHERE userName = '%s' ORDER BY sessionName;",
        namedSessionTable, encUserName);
else
    sqlSafef(query, sizeof(query), "SELECT sessionName, shared, firstUse, contents from %s "
        "WHERE userName = '%s' ORDER BY sessionName;",
        namedSessionTable, encUserName);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    char *encSessionName = row[0];
    char *sessionName = cgiDecodeClone(encSessionName);
    char *link = NULL;
    int shared = atoi(row[1]);
    char *firstUse = row[2];
    char buf[512];
    boolean inGallery = FALSE;

    if (shared >=2)
        inGallery = TRUE;

    printf("<TR><TD>&nbsp;&nbsp;</TD><TD>");

    struct dyString *dy = dyStringNew(1024);
    addSessionLink(dy, encUserName, encSessionName, FALSE);
    printf("<a href=\"%s\">%s</a>", dyStringContents(dy), sessionName);
    dyStringFree(&dy);

    char *spacePt = strchr(firstUse, ' ');
    if (spacePt != NULL) *spacePt = '\0';
        printf("&nbsp;&nbsp;</TD>"
	        "<TD><nobr>%s<nobr>&nbsp;&nbsp;</TD><TD align=center>", firstUse);

    char *dbIdx = NULL;
    if (startsWith("db=", row[3]))
        dbIdx = row[3]+3;
    else
        dbIdx = strstr(row[3], "&db=") + 4;
        
    if (dbIdx != NULL)
        {
        char *dbEnd = strchr(dbIdx, '&');
        char *db = NULL;
        if (dbEnd != NULL)
            db = cloneStringZ(dbIdx, dbEnd-dbIdx);
        else
            db = cloneString(dbIdx);
        printf("%s</td><td align=center>", db);
        }
    else
        printf("n/a</td><td align=center>");

    if (gotSettings)
        {
        safef(buf, sizeof(buf), "%s%s", hgsEditPrefix, encSessionName);
        cgiMakeButton(buf, "details");
        }
    else
        printf("unavailable");
    printf("</TD><TD align=center>");
    safef(buf, sizeof(buf), "%s%s", hgsDeletePrefix, encSessionName);
    char command[512];
    safef(command, sizeof(command), confirmDeleteFormat, encSessionName);
    cgiMakeOnClickSubmitButton(command, buf, "delete");

    printf("</TD><TD align=center>");
    safef(buf, sizeof(buf), "%s%s", hgsSharePrefix, encSessionName);
    cgiMakeCheckBoxWithId(buf, shared>0, buf);
    jsOnEventById("change",buf,"console.log('new status' + this.checked); document.mainForm.submit();");

    printf("</TD><TD align=center>");
    safef(buf, sizeof(buf), "%s%s", hgsGalleryPrefix, encSessionName);
    cgiMakeCheckBoxFourWay(buf, inGallery, shared>0, buf, NULL, NULL);
    jsOnEventById("change", buf, "document.mainForm.submit();");

    link = getSessionEmailLink(encUserName, encSessionName);
    printf("</td><td align=center>%s</td></tr>", link);
    freez(&link);
    foundAny = TRUE;
    struct slName *sn = slNameNew(sessionName);
    slAddHead(&existingSessionNames, sn);
    }
if (!foundAny)
    printf("<TR><TD>&nbsp;&nbsp;&nbsp;</TD><TD>(none)</TD>"
	   "<TD colspan=5></TD></TR>\n");

printf("</tbody>\n");

printf("</TABLE>\n");
printf("</div>\n");
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
printf("<TD><INPUT TYPE=FILE NAME=\"%s\" id='%s'>\n", hgsLoadLocalFileName,  hgsLoadLocalFileName);
jsOnEventById("keypress", hgsLoadLocalFileName,"return noSubmitOnEnter(event);");
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
        // MySQL does case-insensitive comparison because our DEFAULT CHARSET=latin1;
        // use case-insensitive comparison here to avoid clobbering (#15051).
        dyStringAppend(js, "var su, si = document.getElementsByName('" hgsNewSessionName "'); ");
        dyStringAppend(js, "if (si[0]) { su = si[0].value.toUpperCase(); if ( ");
	for (sn = existingSessionNames;  sn != NULL;  sn = sn->next)
	    {
            char nameUpper[PATH_LEN];
            safecpy(nameUpper, sizeof(nameUpper), sn->name);
            touppers(nameUpper);
            dyStringPrintf(js, "su === ");
            dyStringQuoteString(js, '\'', nameUpper);
            dyStringPrintf(js, "%s", (sn->next ? " || " : " ) { "));
	    }
	dyStringAppend(js, "return confirm('This will overwrite the contents of the existing "
                       "session ' + si[0].value + '.  Proceed?'); } }");
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
	NULL, NULL);
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
       "for more information about this tool. "
       "See the <A HREF=\"../goldenPath/help/sessions.html\" "
       "TARGET=_BLANK>Session Gallery</A> "
       "for example sessions.<P/>\n");

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
           "The Email link invokes your email tool with a message "
           "containing the Genome Browser link. The Email link can "
           "be bookmarked in your web browser and/or shared with "
           "others. If you right-click and copy the Browser link, "
           "it will be the same as the Email link. However, if you "
           "click the Browser link it will take you to the Genome "
           "Browser and become a uniquely identified URL once the "
           "session loads, so that resulting link is not advised "
           "for sharing.</LI>\n"
       "<li>Each previously saved named session also appears with "
           "a checkbox to add the session to our "
           "<a href=\"../cgi-bin/hgPublicSessions?%s\">Public Sessions</a> "
           "listing. Adding a session to this listing allows other "
           "browser users to view the description and a thumbnail "
           "image of your session, and to load the session if they "
           "are interested.</li>\n", cartSidUrlString(cart));
    }
else if (loginSystemEnabled() || wikiLinkEnabled())
    {
     printf("<LI>If you <A HREF=\"%s\">sign in</A>, you will be able " 
            " to save named sessions which will be displayed with "
            " Browser and Email links.</LI>\n",
            wikiLinkUserLoginUrl(cartSessionId(cart)));
    }
dyStringPrintf(dyUrl, "%shgTracks", hLocalHostCgiBinUrl());

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
       "the Genome Browser. For an example page using such links "
       "please see the <A HREF=\"../goldenPath/help/sessions.html\" "
       "TARGET=_BLANK>Session Gallery</A>.</LI>\n",
       dyUrl->string);
printf("</UL>\n");
dyStringFree(&dyUrl);
}

void doMainPage(char *userName, char *message)
/* Login status/links and session controls. */
{
puts("Content-Type:text/html\n");
if (loginSystemEnabled() || wikiLinkEnabled())
    {
    if (userName)
	welcomeUser(userName);
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
    showSessionControls(userName, TRUE, TRUE);
    showLinkingTemplates(userName);
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
safef(varName, sizeof(varName), "%s%s", cgiBooleanShadowPrefix(), hgsGalleryPrefix);
cartRemovePrefix(cart, varName);
cartRemovePrefix(cart, hgsGalleryPrefix);
cartRemovePrefix(cart, hgsLoadPrefix);
cartRemovePrefix(cart, hgsEditPrefix);
cartRemovePrefix(cart, hgsLoadLocalFileName);
cartRemovePrefix(cart, hgsDeletePrefix);
cartRemovePrefix(cart, hgsDo);
cartRemove(cart, hgsOldSessionName);
cartRemove(cart, hgsCancel);
}

static void outIfNotPresent(struct cart *cart, struct dyString *dy, char *track, int tdbVis)
/* Output default trackDb visibility if it's not mentioned in the cart. */
{
char *cartVis = cartOptionalString(cart, track);
if (cartVis == NULL)
    {
    if (dy)
        dyStringPrintf(dy,"&%s=%s", track, hStringFromTv(tdbVis));
    else
        printf("%s %s\n", track, hStringFromTv(tdbVis));
    }
}

static void outDefaultTracks(struct cart *cart, struct dyString *dy)
/* Output the default trackDb visibility for all tracks
 * in trackDb if the track is not mentioned in the cart. */
{
char *database = cartString(cart, "db");
struct trackDb *tdb = hTrackDb(database);
struct hash *parentHash = newHash(5);

for(; tdb; tdb = tdb->next)
    {
    struct trackDb *parent = tdb->parent;
    if (parent) 
        {
        if (hashLookup(parentHash, parent->track) == NULL)
            {
            hashStore(parentHash, parent->track);
            if (parent->isShow)
                outIfNotPresent(cart, dy, parent->track, tvShow);
            }
        }
    if (tdb->visibility != tvHide)
        outIfNotPresent(cart, dy, tdb->track, tdb->visibility);
    }

// Put a variable in the cart that says we put the default 
// visibilities in it.
if (dy)
    dyStringPrintf(dy,"&%s=on", CART_HAS_DEFAULT_VISIBILITY);
else
    printf("%s on", CART_HAS_DEFAULT_VISIBILITY);
}

#define INITIAL_USE_COUNT 0
char *doNewSession(char *userName)
/* Save current settings in a new named session.
 * Return a message confirming what we did. */
{
if (userName == NULL)
    return "Unable to save session -- please log in and try again.";
struct dyString *dyMessage = dyStringNew(2048);
char *sessionName = trimSpaces(cartString(cart, hgsNewSessionName));
char *encSessionName = cgiEncodeFull(sessionName);
boolean shareSession = cartBoolean(cart, hgsNewSessionShare);
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
    sqlDyStringPrintf(dy, "SELECT firstUse, useCount FROM %s "
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
    sqlDyStringPrintf(dy, "DELETE FROM %s WHERE userName = '%s' AND "
		       "sessionName = '%s';",
		   namedSessionTable, encUserName, encSessionName);
    sqlUpdate(conn, dy->string);

    dyStringClear(dy);
    sqlDyStringPrintf(dy, "INSERT INTO %s ", namedSessionTable);
    dyStringAppend(dy, "(userName, sessionName, contents, shared, "
		       "firstUse, lastUse, useCount, settings) VALUES (");
    dyStringPrintf(dy, "'%s', '%s', ", encUserName, encSessionName);
    dyStringAppend(dy, "'");
    cleanHgSessionFromCart(cart);
    struct dyString *encoded = newDyString(4096);
    cartEncodeState(cart, encoded);

    // Now add all the default visibilities to output.
    outDefaultTracks(cart, encoded);

    sqlDyAppendEscaped(dy, encoded->string);
    dyStringFree(&encoded);
    dyStringAppend(dy, "', ");
    dyStringPrintf(dy, "%d, ", (shareSession ? 1 : 0));
    dyStringPrintf(dy, "%s, now(), %d, '');", firstUse, useCount);
    sqlUpdate(conn, dy->string);
    dyStringFree(&dy);

    /* Prevent modification of custom tracks just saved to namedSessionDb: */
    cartCopyCustomTracks(cart);

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
    cartCheckForCustomTracks(cart, dyMessage);
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

int thumbnailAdd(char *encUserName, char *encSessionName, struct sqlConnection *conn, struct dyString *dyMessage)
/* Create a thumbnail image for the gallery.  If the necessary tools can't be found,
 * add a warning message to dyMessage unless the hg.conf setting
 * sessionThumbnail.suppressWarning is set to "on".
 * Leaks memory from a generated filename string, plus a couple of dyStrings.
 * Returns without determining if image creation succeeded (it happens in a separate
 * thread); the return value is 0 if a message was added to dyMessage, otherwise it's 1. */
{
char query[4096];

char *suppressConvert = cfgOption("sessionThumbnail.suppress");
if (suppressConvert != NULL && sameString(suppressConvert, "on"))
    return 1;

char *convertPath = cfgOption("sessionThumbnail.convertPath");
if (convertPath == NULL)
    convertPath = cloneString("convert");
char convertTestCmd[4096];
safef(convertTestCmd, sizeof(convertTestCmd), "which %s > /dev/null", convertPath);
int convertTestResult = system(convertTestCmd);
if (convertTestResult != 0)
    {
    dyStringPrintf(dyMessage,
         "Note: A thumbnail image for this session was not created because the ImageMagick convert "
         "tool could not be found.  Please contact your mirror administrator to resolve this "
         "issue, either by installing convert so that it is part of the webserver's PATH, "
         "by adding the \"sessionThumbnail.convertPath\" option to the mirror's hg.conf file "
         "to specify the path to that program, or by adding \"sessionThumbnail.suppress=on\" to "
         "the mirror's hg.conf file to suppress this warning.<br>");
    return 0;
    }

sqlSafef(query, sizeof(query),
    "select firstUse from namedSessionDb where userName = \"%s\" and sessionName = \"%s\"",
    encUserName, encSessionName);
char *firstUse = sqlNeedQuickString(conn, query);
sqlSafef(query, sizeof(query), "select idx from gbMembers where userName = '%s'", encUserName);
char *userIdx = sqlQuickString(conn, query);
char *userIdentifier = sessionThumbnailGetUserIdentifier(encUserName, userIdx);
char *destFile = sessionThumbnailFilePath(userIdentifier, encSessionName, firstUse);
if (destFile != NULL)
    {
    struct dyString *hgTracksUrl = dyStringNew(0);
    addSessionLink(hgTracksUrl, encUserName, encSessionName, FALSE);
    struct dyString *renderUrl =
        dyStringSub(hgTracksUrl->string, "cgi-bin/hgTracks", "cgi-bin/hgRenderTracks");
    dyStringAppend(renderUrl, "&pix=640");
    char *renderCmd[] = {"wget", "-q", "-O", "-", renderUrl->string, NULL};
    char *convertCmd[] = {convertPath, "-", "-resize", "320", "-crop", "320x240+0+0", destFile, NULL};
    char **cmdsImg[] = {renderCmd, convertCmd, NULL};
    pipelineOpen(cmdsImg, pipelineWrite, "/dev/null", NULL);
    }
return 1;
}


void thumbnailRemove(char *encUserName, char *encSessionName, struct sqlConnection *conn)
/* Unlink thumbnail image for the gallery.  Leaks memory from a generated filename string. */
{
char query[4096];
sqlSafef(query, sizeof(query),
    "select firstUse from namedSessionDb where userName = \"%s\" and sessionName = \"%s\"",
    encUserName, encSessionName);
char *firstUse = sqlNeedQuickString(conn, query);
sqlSafef(query, sizeof(query), "select idx from gbMembers where userName = '%s'", encUserName);
char *userIdx = sqlQuickString(conn, query);
char *userIdentifier = sessionThumbnailGetUserIdentifier(encUserName, userIdx);
char *filePath = sessionThumbnailFilePath(userIdentifier, encSessionName, firstUse);
if (filePath != NULL)
    unlink(filePath);
}

char *doSessionDetail(char *userName, char *sessionName)
/* Show details about a particular session. */
{
if (userName == NULL)
    return "Sorry, please log in again.";
struct dyString *dyMessage = dyStringNew(4096);
char *encSessionName = cgiEncodeFull(sessionName);
char *encUserName = cgiEncodeFull(userName);
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr = NULL;
char **row = NULL;
char query[512];
webPushErrHandlersCartDb(cart, cartUsualString(cart, "db", NULL));
boolean gotSettings = (sqlFieldIndex(conn, namedSessionTable, "settings") >= 0);

if (gotSettings)
    sqlSafef(query, sizeof(query), "SELECT shared, firstUse, settings from %s "
	  "WHERE userName = '%s' AND sessionName = '%s'",
          namedSessionTable, encUserName, encSessionName);
else
    sqlSafef(query, sizeof(query), "SELECT shared, firstUse from %s "
	  "WHERE userName = '%s' AND sessionName = '%s'",
          namedSessionTable, encUserName, encSessionName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    int shared = atoi(row[0]);
    char *firstUse = row[1];
    char *settings = NULL;
    if (gotSettings)
	settings = row[2];
    char *description = getSetting(settings, "description");
    if (description == NULL) description = "";

    dyStringPrintf(dyMessage, "<A HREF=\"../goldenPath/help/hgSessionHelp.html#Details\" "
		   "TARGET=_BLANK>Session Details Help</A><P/>\n");

#define highlightAccChanges " var b = document.getElementById('" hgsDoSessionChange "'); " \
                            "  if (b) { b.style.background = '#ff9999'; }"

#define toggleGalleryDisable \
                            "  var c = document.getElementById('detailsSharedCheckbox'); " \
                            "  var d = document.getElementById('detailsGalleryCheckbox'); " \
                            "  if (c.checked)" \
                            "    {d.disabled = false;} " \
                            "  else" \
                            "    {d.disabled = true; " \
                            "     d.checked = false; }"

    dyStringPrintf(dyMessage, "<B>%s</B><P>\n"
		   "<FORM ACTION=\"%s\" NAME=\"detailForm\" METHOD=GET>\n"
		   "<INPUT TYPE=HIDDEN NAME=\"%s\" VALUE=%s>"
		   "<INPUT TYPE=HIDDEN NAME=\"%s\" VALUE=\"%s\">"
		   "Session Name: "
		   "<INPUT TYPE=TEXT NAME=\"%s\" id='%s' SIZE=%d VALUE=\"%s\" >\n",
		   sessionName, hgSessionName(),
		   cartSessionVarName(cart), cartSessionId(cart), hgsOldSessionName, sessionName,
		   hgsNewSessionName, hgsNewSessionName, 32, sessionName);
    jsOnEventById("change"  , hgsNewSessionName, highlightAccChanges);
    jsOnEventById("keypress", hgsNewSessionName, highlightAccChanges);

    dyStringPrintf(dyMessage,
		   "&nbsp;&nbsp;<INPUT TYPE=SUBMIT NAME=\"%s%s\" VALUE=\"use\">"
		   "&nbsp;&nbsp;<INPUT TYPE=SUBMIT NAME=\"%s%s\" id='%s%s' VALUE=\"delete\">"
		   "&nbsp;&nbsp;<INPUT TYPE=SUBMIT ID=\"%s\" NAME=\"%s\" VALUE=\"accept changes\">"
		   "&nbsp;&nbsp;<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"cancel\"> "
		   "<BR>\n",
		   hgsLoadPrefix, encSessionName, 
		   hgsDeletePrefix, encSessionName, hgsDeletePrefix, encSessionName,
		   hgsDoSessionChange, hgsDoSessionChange, hgsCancel);
    char id[256];
    safef(id, sizeof id, "%s%s", hgsDeletePrefix, encSessionName);
    jsOnEventByIdF("click", id, confirmDeleteFormat, encSessionName);

    dyStringPrintf(dyMessage,
		   "Share with others? <INPUT TYPE=CHECKBOX NAME=\"%s%s\"%s VALUE=on "
		   "id=\"detailsSharedCheckbox\">\n"
		   "<INPUT TYPE=HIDDEN NAME=\"%s%s%s\" VALUE=0><BR>\n",
		   hgsSharePrefix, encSessionName, (shared>0 ? " CHECKED" : ""),
		   cgiBooleanShadowPrefix(), hgsSharePrefix, encSessionName);
    jsOnEventByIdF("change", "detailsSharedCheckbox", "{%s %s}", highlightAccChanges, toggleGalleryDisable);
    jsOnEventByIdF("click" , "detailsSharedCheckbox", "{%s %s}", highlightAccChanges, toggleGalleryDisable);

    dyStringPrintf(dyMessage,
		   "List in Public Sessions? <INPUT TYPE=CHECKBOX NAME=\"%s%s\"%s VALUE=on "
		   "id=\"detailsGalleryCheckbox\">\n"
		   "<INPUT TYPE=HIDDEN NAME=\"%s%s%s\" VALUE=0><BR>\n",
		   hgsGalleryPrefix, encSessionName, (shared>=2 ? " CHECKED" : ""),
		   cgiBooleanShadowPrefix(), hgsGalleryPrefix, encSessionName);
    jsOnEventById("change", "detailsGalleryCheckbox", highlightAccChanges);
    jsOnEventById("click" , "detailsGalleryCheckbox", highlightAccChanges);
    
    /* Set initial disabled state of the gallery checkbox */
    jsInline(toggleGalleryDisable);
    dyStringPrintf(dyMessage,
		   "Created on %s.<BR>\n", firstUse);
    /* Print custom track counts per assembly */
    struct cart *tmpCart = cartNew(NULL,NULL,NULL,NULL);
    struct sqlConnection *conn2 = hConnectCentral();
    cartLoadUserSession(conn2, userName, sessionName, tmpCart, NULL, NULL);
    hDisconnectCentral(&conn2);
    cartCheckForCustomTracks(tmpCart, dyMessage);

    if (gotSettings)
        {
        description = replaceChars(description, "\\\\", "\\__ESC__");
        description = replaceChars(description, "\\r", "\r");
        description = replaceChars(description, "\\n", "\n");
        description = replaceChars(description, "\\__ESC__", "\\");
        dyStringPrintf(dyMessage,
            "Description:<BR>\n"
            "<TEXTAREA NAME=\"%s\" id='%s' ROWS=%d COLS=%d "
            ">%s</TEXTAREA><BR>\n",
            hgsNewSessionDescription, hgsNewSessionDescription, 5, 80,
            description);
	    jsOnEventById("change"   , hgsNewSessionDescription, highlightAccChanges);
	    jsOnEventById("keypress" , hgsNewSessionDescription, highlightAccChanges);
        }
    dyStringAppend(dyMessage, "</FORM>\n");
    sqlFreeResult(&sr);
    }
else
    errAbort("doSessionDetail: got no results from query:<BR>\n%s\n", query);

return dyStringCannibalize(&dyMessage);
}

char *doUpdateSessions(char *userName)
/* Look for cart variables matching prefixes for sharing/unsharing,
 * loading or deleting a previously saved session.
 * Return a message confirming what we did, or NULL if no such variables
 * were in the cart. */
{
if (userName == NULL)
    return NULL;
struct dyString *dyMessage = dyStringNew(1024);
struct hashEl *cartHelList = NULL, *hel = NULL;
struct sqlConnection *conn = hConnectCentral();
char *encUserName = cgiEncodeFull(userName);
boolean didSomething = FALSE;
char query[512];

cartHelList = cartFindPrefix(cart, hgsGalleryPrefix);
if (cartHelList != NULL)
    {
    struct hash *galleryHash = hashNew(0);
    char **row;
    struct sqlResult *sr;
    sqlSafef(query, sizeof(query),
	  "select sessionName,shared from %s where userName = '%s'",
	  namedSessionTable, encUserName);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	    hashAddInt(galleryHash, row[0], atoi(row[1]));
    sqlFreeResult(&sr);
    for (hel = cartHelList;  hel != NULL;  hel = hel->next)
	{
	char *encSessionName = hel->name + strlen(hgsGalleryPrefix);
	char *sessionName = cgiDecodeClone(encSessionName);
	boolean inGallery = hashIntVal(galleryHash, encSessionName) >= 2 ? TRUE : FALSE;
	boolean newGallery  = cartUsualInt(cart, hel->name, 0) > 0 ? TRUE : FALSE;

	if (newGallery != inGallery)
	    {
	    sqlSafef(query, sizeof(query), "UPDATE %s SET shared = %d "
		  "WHERE userName = '%s' AND sessionName = '%s';",
		  namedSessionTable, newGallery == TRUE ? 2 : 1, encUserName, encSessionName);
	    sqlUpdate(conn, query);
	    sessionTouchLastUse(conn, encUserName, encSessionName);
	    dyStringPrintf(dyMessage,
			   "Marked session <B>%s</B> as %s.<BR>\n",
			   htmlEncode(sessionName),
			   (newGallery == TRUE ? "added to gallery" : "removed from public listing"));
        if (newGallery == FALSE)
            thumbnailRemove(encUserName, encSessionName, conn);
        if (newGallery == TRUE)
            thumbnailAdd(encUserName, encSessionName, conn, dyMessage);
	    didSomething = TRUE;
	    }
	}
    hashFree(&galleryHash);
    }

cartHelList = cartFindPrefix(cart, hgsSharePrefix);
if (cartHelList != NULL)
    {
    struct hash *sharedHash = hashNew(0);
    char **row;
    struct sqlResult *sr;
    sqlSafef(query, sizeof(query),
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
	boolean alreadyShared = hashIntVal(sharedHash, encSessionName) > 0 ? TRUE : FALSE;
    boolean inGallery = hashIntVal(sharedHash, encSessionName) >= 2 ? TRUE : FALSE;
	boolean newShared = cartUsualInt(cart, hel->name, 1) ? TRUE : FALSE;

	if (newShared != alreadyShared)
	    {
	    sqlSafef(query, sizeof(query), "UPDATE %s SET shared = %d "
		  "WHERE userName = '%s' AND sessionName = '%s';",
		  namedSessionTable, newShared, encUserName, encSessionName);
	    sqlUpdate(conn, query);
	    sessionTouchLastUse(conn, encUserName, encSessionName);
	    dyStringPrintf(dyMessage,
			   "Marked session <B>%s</B> as %s.<BR>\n",
			   htmlEncode(sessionName),
			   (newShared == TRUE ? "shared" : "unshared"));
        if (newShared == FALSE && inGallery == TRUE)
            thumbnailRemove(encUserName, encSessionName, conn);
	    didSomething = TRUE;
	    }
	}
    hashFree(&sharedHash);
    }

hel = cartFindPrefix(cart, hgsEditPrefix);
if (hel != NULL)
    {
    char *encSessionName = hel->name + strlen(hgsEditPrefix);
    char *sessionName = cgiDecodeClone(encSessionName);
    dyStringPrintf(dyMessage, "%s", doSessionDetail(userName, sessionName));
    didSomething = TRUE;
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
    cartHideDefaultTracks(cart);
    hubConnectLoadHubs(cart);
    cartCopyCustomTracks(cart);
    cartCheckForCustomTracks(cart, dyMessage);
    didSomething = TRUE;
    }

cartHelList = cartFindPrefix(cart, hgsDeletePrefix);
for (hel = cartHelList;  hel != NULL;  hel = hel->next)
    {
    char *encSessionName = hel->name + strlen(hgsDeletePrefix);
    char *sessionName = cgiDecodeClone(encSessionName);
    sqlSafef(query, sizeof(query), "select shared from %s "
      "where userName = '%s' and sessionName = '%s';",
      namedSessionTable, encUserName, encSessionName);
    int shared = sqlQuickNum(conn, query);
    if (shared >= 2)
        thumbnailRemove(encUserName, encSessionName, conn);
    sqlSafef(query, sizeof(query), "DELETE FROM %s "
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
cartHideDefaultTracks(cart);
hubConnectLoadHubs(cart);
cartCopyCustomTracks(cart);
cartCheckForCustomTracks(cart, dyMessage);
hDisconnectCentral(&conn);
return dyStringCannibalize(&dyMessage);
}

void doSaveLocal()
/* Output current settings to be saved as a file on the user's machine.
 * Return a message confirming what we did. */
{
char *fileName = textOutSanitizeHttpFileName(cartString(cart, hgsSaveLocalFileName));
char *compressType = cartString(cart, hgsSaveLocalFileCompress);
struct pipeline *compressPipe = textOutInit(fileName, compressType, NULL);

cleanHgSessionFromCart(cart);
cartDump(cart);

// Now add all the default visibilities to output.
outDefaultTracks(cart, NULL);

textOutClose(&compressPipe, NULL);
}

char *doLoad(boolean fromUrl, char *actionVar)
/* Load settings from a file or URL sent by the user.
 * Return a message confirming what we did. */
{
struct dyString *dyMessage = dyStringNew(1024);
struct lineFile *lf = NULL;
webPushErrHandlersCartDb(cart, cartUsualString(cart, "db", NULL));
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
	   "<A HREF=\"%shgTracks?%s=%s\">Browser</A>",
	   hLocalHostCgiBinUrl(), 
	   cartSessionVarName(), cartSessionId(cart));
    }
if (lf != NULL)
    {
    cartLoadSettings(lf, cart, NULL, actionVar);
    cartHideDefaultTracks(cart);
    hubConnectLoadHubs(cart);
    cartCopyCustomTracks(cart);
    cartCheckForCustomTracks(cart, dyMessage);
    lineFileClose(&lf);
    }
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

char *doSessionChange(char *userName, char *oldSessionName)
/* Process changes to session from session details page. */
{
if (userName == NULL)
    return "Unable to make changes to session.  Please log in again.";
struct dyString *dyMessage = dyStringNew(1024);
webPushErrHandlersCartDb(cart, cartUsualString(cart, "db", NULL));
char *sessionName = oldSessionName;
char *encSessionName = cgiEncodeFull(sessionName);
char *encOldSessionName = encSessionName;
char *encUserName = cgiEncodeFull(userName);
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr = NULL;
char **row = NULL;
char query[512];
int shared = 1;
char *settings = NULL;
boolean gotSettings = (sqlFieldIndex(conn, namedSessionTable, "settings") >= 0);

if (gotSettings)
    sqlSafef(query, sizeof(query), "SELECT shared, settings from %s "
	  "WHERE userName = '%s' AND sessionName = '%s'",
          namedSessionTable, encUserName, encSessionName);
else
    sqlSafef(query, sizeof(query), "SELECT shared from %s "
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
    sqlSafef(query, sizeof(query),
	  "UPDATE %s set sessionName = '%s' WHERE userName = '%s' AND sessionName = '%s';",
	  namedSessionTable, encNewName, encUserName, encSessionName);
	sqlUpdate(conn, query);
    dyStringPrintf(dyMessage, "Changed session name from %s to <B>%s</B>.\n",
		   sessionName, newName);
    sessionName = newName;
    encSessionName = encNewName;
    renamePrefixedCartVar(hgsEditPrefix, encOldSessionName, encNewName);
    renamePrefixedCartVar(hgsLoadPrefix, encOldSessionName, encNewName);
    renamePrefixedCartVar(hgsDeletePrefix, encOldSessionName, encNewName);
    if (shared >= 2)
        {
        thumbnailRemove(encUserName, encSessionName, conn);
        thumbnailAdd(encUserName, encNewName, conn, dyMessage);
        }
    }

char sharedVarName[256];
char galleryVarName[256];
safef(sharedVarName, sizeof(sharedVarName), hgsSharePrefix "%s", encOldSessionName);
safef(galleryVarName, sizeof(galleryVarName), hgsGalleryPrefix "%s", encOldSessionName);
if (cgiBooleanDefined(sharedVarName) || cgiBooleanDefined(galleryVarName))
    {
    int newShared = shared;
    if (cgiBooleanDefined(sharedVarName))
        newShared = cartBoolean(cart, sharedVarName) ? 1 : 0;
    if (cgiBooleanDefined(galleryVarName))
        newShared = cartBoolean(cart, galleryVarName) ? 2 : newShared;
    if (newShared != shared)
        {
        sqlSafef(query, sizeof(query),
            "UPDATE %s set shared = %d WHERE userName = '%s' AND sessionName = '%s';",
            namedSessionTable, newShared, encUserName, encSessionName);
        sqlUpdate(conn, query);
        dyStringPrintf(dyMessage, "Marked session <B>%s</B> as %s.<BR>\n",
            htmlEncode(sessionName), (newShared>0 ? newShared>=2 ? "shared in public listing" :
                "shared, but not in public listing" : "unshared"));
        if (shared >= 2 && newShared < 2)
            thumbnailRemove(encUserName, encSessionName, conn);
        if (shared < 2 && newShared >= 2)
            thumbnailAdd(encUserName, encSessionName, conn, dyMessage);
        }
    cartRemove(cart, sharedVarName);
    cartRemove(cart, galleryVarName);
    char shadowVarName[512];
    safef(shadowVarName, sizeof(shadowVarName), "%s%s", cgiBooleanShadowPrefix(), sharedVarName);
    cartRemove(cart, shadowVarName);
    safef(shadowVarName, sizeof(shadowVarName), "%s%s", cgiBooleanShadowPrefix(), galleryVarName);
    cartRemove(cart, shadowVarName);
    }
if (gotSettings)
    {
    struct hash *settingsHash = raFromString(settings);
    char *description = hashFindVal(settingsHash, "description");
    char *newDescription = cartOptionalString(cart, hgsNewSessionDescription);
    if (newDescription != NULL)
	{
	// newline escaping of \n is needed for ra syntax.
        // not sure why \r and \ are being escaped, but it may be too late to change
        // since there are probably records in the database that way now.
	newDescription = replaceChars(newDescription, "\\", "\\\\");
	newDescription = replaceChars(newDescription, "\r", "\\r");
	newDescription = replaceChars(newDescription, "\n", "\\n");
	}
    else
	newDescription = "";
    if (description == NULL)
	description = "";
    if (!sameString(description, newDescription))
	{
	hashRemove(settingsHash, "description");
	hashAdd(settingsHash, "description", newDescription);
	struct dyString *dyRa = dyStringNew(512);
	struct hashEl *hel = hashElListHash(settingsHash);
	while (hel != NULL)
	    {
	    dyStringPrintf(dyRa, "%s %s\n", hel->name, (char *)hel->val);
	    hel = hel->next;
	    }
	struct dyString *dyQuery = dyStringNew(1024);
	sqlDyStringPrintf(dyQuery, "UPDATE %s set settings = '%s' "
				   "WHERE userName = '%s' AND sessionName = '%s';",
			namedSessionTable, dyRa->string, encUserName, encSessionName);
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

char *userName = (loginSystemEnabled() || wikiLinkEnabled()) ? wikiLinkUserName() : NULL;

if (cartVarExists(cart, hgsDoMainPage) || cartVarExists(cart, hgsCancel))
    doMainPage(userName, NULL);
else if (cartVarExists(cart, hgsDoNewSession))
    {
    char *message = doNewSession(userName);
    doMainPage(userName, message);
    }
else if (cartVarExists(cart, hgsDoOtherUser))
    {
    char *message = doOtherUser(hgsDoOtherUser);
    doMainPage(userName, message);
    }
else if (cartVarExists(cart, hgsDoSaveLocal))
    {
    doSaveLocal();
    }
else if (cartVarExists(cart, hgsDoLoadLocal))
    {
    char *message = doLoad(FALSE, hgsDoLoadLocal);
    doMainPage(userName, message);
    }
else if (cartVarExists(cart, hgsDoLoadUrl))
    {
    char *message = doLoad(TRUE, hgsDoLoadUrl);
    doMainPage(userName, message);
    }
else if (cartVarExists(cart, hgsDoSessionDetail))
    {
    char *message = doSessionDetail(userName, cartString(cart, hgsDoSessionDetail));
    doMainPage(userName, message);
    }
else if (cartVarExists(cart, hgsDoSessionChange))
    {
    char *message = doSessionChange(userName, cartString(cart, hgsOldSessionName));
    doMainPage(userName, message);
    }
else if (cartVarExists(cart, hgsOldSessionName))
    {
    char *message1 = doSessionChange(userName, cartString(cart, hgsOldSessionName));
    char *message2 = doUpdateSessions(userName);
    char *message = message2;
    if (!startsWith("No changes to session", message1))
	{
	size_t len = (sizeof message1[0]) * (strlen(message1) + strlen(message2) + 1);
	message = needMem(len);
	safef(message, len, "%s%s", message1, message2);
	}
    doMainPage(userName, message);
    }
else
    {
    char *message = doUpdateSessions(userName);
    doMainPage(userName, message);
    }

cleanHgSessionFromCart(cart);
/* Save the cart state: */
cartCheckout(&cart);
}

int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();
htmlPushEarlyHandlers();
cgiSpoof(&argc, argv);
hgSession();
cgiExitTime("hgSession", enteredMainTime);
return 0;
}

