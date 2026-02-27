/* hgSession - Interface with wiki login and do session saving/loading. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

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
#include "hgConfig.h"
#include "sessionThumbnail.h"
#include "filePath.h"
#include "obscure.h"
#include "trashDir.h"
#include "hubConnect.h"
#include "trackHub.h"
#include "errCatch.h"
#include "sessionData.h"

char *database = NULL;

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
char *excludeVars[] = {"Submit", "submit", hgsSessionDataDbSuffix, NULL};

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
		    boolean encode, boolean tryShortLink)
/* Add to dy an URL that tells hgSession to load a saved session.
 * If encode, cgiEncodeFull the URL. 
 * If tryShortLink, print a shortened link that apache can redirect.
 * The link is an absolute link that includes the server name so people can
 * copy-paste it into emails.
 *
 * NOTE: Do not append CGI variables here, as it will break short links if they are enabled. */
{
struct dyString *dyTmp = dyStringNew(1024);
if (tryShortLink && cfgOptionBooleanDefault("hgSession.shortLink", FALSE) &&
        !stringIn("%2F", userName) && !stringIn("%2F", sessionName))
    dyStringPrintf(dyTmp, "http%s://%s/s/%s/%s", cgiAppendSForHttps(), cgiServerNamePort(),
        userName, sessionName);
else
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

void printCopyToClipboardButton(struct dyString *dy, char *iconId, char *targetId, char *buttonLabel) 
/* print a copy-to-clipboard button with DOM id iconId that copies the node text of targetId */
{
dyStringPrintf(dy, "&nbsp;<button title='Copy URL to clipboard' id='%s' data-target='%s'><svg style='width:0.9em' xmlns='http://www.w3.org/2000/svg' viewBox='0 0 512 512'><!--! Font Awesome Pro 6.1.1 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license (Commercial License) Copyright 2022 Fonticons, Inc. --><path d='M502.6 70.63l-61.25-61.25C435.4 3.371 427.2 0 418.7 0H255.1c-35.35 0-64 28.66-64 64l.0195 256C192 355.4 220.7 384 256 384h192c35.2 0 64-28.8 64-64V93.25C512 84.77 508.6 76.63 502.6 70.63zM464 320c0 8.836-7.164 16-16 16H255.1c-8.838 0-16-7.164-16-16L239.1 64.13c0-8.836 7.164-16 16-16h128L384 96c0 17.67 14.33 32 32 32h47.1V320zM272 448c0 8.836-7.164 16-16 16H63.1c-8.838 0-16-7.164-16-16L47.98 192.1c0-8.836 7.164-16 16-16H160V128H63.99c-35.35 0-64 28.65-64 64l.0098 256C.002 483.3 28.66 512 64 512h192c35.2 0 64-28.8 64-64v-32h-47.1L272 448z'/></svg>%s</button>\n", iconId, targetId, buttonLabel);

jsOnEventById("click", iconId, "copyToClipboard(event);");
}

void printShareMessage(struct dyString *dy, char *userName, char *sessionName,
            boolean encode)
{
struct dyString *dyTmp = dyStringNew(0);
addSessionLink(dyTmp, userName, sessionName, encode, TRUE);
dyStringPrintf(dy,
    "<p>You can share this session with the following URL:<br><span id='urlText'>%s</span>&nbsp;",
    dyTmp->string);

printCopyToClipboardButton(dy, "copyIcon", "urlText", "&nbsp;Copy to clipboard");
dyStringAppend(dy, "</p>");
}

char *getSessionLink(char *encUserName, char *encSessionName)
/* Form a link that will take the user to a bookmarkable page that
 * will load the given session. */
{
struct dyString *dy = dyStringNew(1024);
dyStringPrintf(dy, "<A HREF=\"");
addSessionLink(dy, encUserName, encSessionName, FALSE, TRUE);
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
addSessionLink(dy, encUserName, encSessionName, TRUE, TRUE);
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

static struct slName *showExistingSessions(char *userName)
/* Print out a table with buttons for sharing/unsharing/loading/deleting
 * previously saved sessions.  Return a list of session names. */
{
struct slName *existingSessionNames = NULL;
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
        "                                       \"order\":[2,'desc'],\n"
        "                                       \"stateSave\":true,\n"
        "                                       \"stateSaveCallback\": %s,\n"
        "                                       \"stateLoadCallback\": %s\n"
        "                                 });\n"
        "} );\n"
        "}\n"
        , jsDataTableStateSave(hgSessionPrefix), jsDataTableStateLoad(hgSessionPrefix, cart));

printf("<style>#sessionTable_filter { float:left !important; margin-left:20px; }</style>\n");
printf("<H3>My Sessions</H3>\n");
printf("<table id=\"sessionTable\" class=\"sessionTable stripe hover row-border compact\" borderwidth=0>\n");
printf("<thead><tr>");
printf("<th></th>"
       "<th style=\"white-space:nowrap;text-align:left\">Session name (click to load)</th>"
       "<th style=\"text-align:left\">Created on</th><th style=\"text-align:left\">View count</th>"
       "<th style=\"text-align:left\">Assembly</th>"
       "<th style=\"text-align:left\">View/edit&nbsp;<BR>details&nbsp;</th>"
       "<th style=\"text-align:left\">Delete this&nbsp;<BR>session&nbsp;</th>"
       "<th style=\"text-align:left\">Share with&nbsp;<BR>others?&nbsp;</th>"
       "<th style=\"text-align:left\">Post in&nbsp;<br><a href=\"../cgi-bin/hgPublicSessions?%s\">public listing</a>?</th>"
       "<th style=\"text-align:left\">Send to<BR>mail</th>",
       cartSidUrlString(cart));
printf("</tr></thead>");
printf("<tbody>\n");

if (gotSettings)
    sqlSafef(query, sizeof(query), "SELECT sessionName, shared, firstUse, useCount, contents, settings from %s "
        "WHERE userName = '%s' ORDER BY sessionName;",
        namedSessionTable, encUserName);
else
    sqlSafef(query, sizeof(query), "SELECT sessionName, shared, firstUse, useCount, contents from %s "
        "WHERE userName = '%s' ORDER BY sessionName;",
        namedSessionTable, encUserName);
sr = sqlGetResult(conn, query);

int rowIdx = 0;

while ((row = sqlNextRow(sr)) != NULL)
    {
    char *encSessionName = row[0];
    char *sessionName = cgiDecodeClone(encSessionName);
    char *link = NULL;
    int shared = atoi(row[1]);
    char *firstUse = row[2];
    char buf[512];
    boolean inGallery = FALSE;
    boolean hasDescription = FALSE;

    if (shared >=2)
        inGallery = TRUE;

    printf("<TR><TD>&nbsp;&nbsp;</TD><TD>");

    char iconId[256];
    char linkId[256];
    safef(linkId, sizeof(linkId), "linkEl-%d", rowIdx);
    safef(iconId, sizeof(iconId), "iconEl-%d", rowIdx);
    struct dyString *buttonText = dyStringNew(4096);
    printCopyToClipboardButton(buttonText, iconId, linkId, "");
    puts(dyStringCannibalize(&buttonText));
    puts("&nbsp;");

    struct dyString *dy = dyStringNew(1024);
    addSessionLink(dy, encUserName, encSessionName, FALSE, TRUE);
    char *sessionUrl = dyStringContents(dy);
    printf("<a id='linkEl-%d' data-copy='%s' href=\"%s\">%s</a>", rowIdx, sessionUrl, sessionUrl, htmlEncode(sessionName));
    dyStringFree(&dy);
    rowIdx++;

    struct tm firstUseTm;
    ZeroVar(&firstUseTm);
    strptime(firstUse, "%Y-%m-%d %T", &firstUseTm);
    char *spacePt = strchr(firstUse, ' ');
    if (spacePt != NULL) *spacePt = '\0';

    char *useCount = row[3];
    printf("&nbsp;&nbsp;</TD>"
            "<TD data-order=\"%ld\"><nobr>%s</nobr>&nbsp;&nbsp;</TD>"
            "<TD>%s</TD>"
            "<TD align='left'>", mktime(&firstUseTm), firstUse, useCount);

    char *dbIdx = NULL;
    if (startsWith("db=", row[4]))
        dbIdx = row[4]+3;
    else
        dbIdx = strstr(row[4], "&db=") + 4;
        
    if (dbIdx != NULL)
        {
        char *dbEnd = strchr(dbIdx, '&');
        char *db = NULL;
        if (dbEnd != NULL)
            db = cloneStringZ(dbIdx, dbEnd-dbIdx);
        else
            db = cloneString(dbIdx);
        printf("%s</td><td align='center'>", db);
        }
    else
        printf("n/a</td><td align=center>");

    if (gotSettings)
        {
        safef(buf, sizeof(buf), "%s%s", hgsEditPrefix, encSessionName);
        cgiMakeButton(buf, "View/edit");
        char *description = getSetting(row[5], "description");
        if (!isEmpty(description))
            hasDescription = TRUE;
        }
    else
        printf("unavailable");

    printf("</TD><TD align=center>");
    safef(buf, sizeof(buf), "%s%s", hgsDeletePrefix, encSessionName);
    char command[512];
    safef(command, sizeof(command), confirmDeleteFormat, encSessionName);
    cgiMakeOnClickSubmitButton(command, buf, "Delete");

    printf("</TD><TD align=center>");
    safef(buf, sizeof(buf), "%s%s", hgsSharePrefix, encSessionName);
    cgiMakeCheckBoxWithId(buf, shared>0, buf);
    jsOnEventById("change",buf,"document.mainForm.submit();");

    printf("</TD><TD align=center>");
    safef(buf, sizeof(buf), "%s%s", hgsGalleryPrefix, encSessionName);
    cgiMakeCheckBoxFourWay(buf, inGallery, shared>0, buf, NULL, NULL);
    if (hasDescription || inGallery)
        jsOnEventById("change", buf, "document.mainForm.submit();");
    else
        jsOnEventById("change", buf, "warn('Please first use the view/edit option to "
                "add a description for this session.'); this.checked = false;");

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
printf("<P></P>\n");
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
return existingSessionNames;
}

void showOtherUserOptions()
/* Print out inputs for loading another user's saved session. */
{
printf("<TABLE BORDERWIDTH=0>\n");
printf("<TR><TD colspan=2>"
       "Use settings from another user's saved session:</TD></TR>\n"
       "<TR><TD>&nbsp;&nbsp;&nbsp;</TD><TD>User: \n");
cgiMakeOnKeypressTextVar(hgsOtherUserName,
			 cartUsualString(cart, hgsOtherUserName, ""),
			 20, "return noSubmitOnEnter(event);");
printf("&nbsp;&nbsp;&nbsp;Session name: \n");
cgiMakeOnKeypressTextVar(hgsOtherUserSessionName,
			 cartUsualString(cart, hgsOtherUserSessionName, ""),
			 20, jsPressOnEnter(hgsDoOtherUser));
printf("&nbsp;&nbsp;");
cgiMakeButton(hgsDoOtherUser, "Submit");
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
cgiMakeButton(hgsDoLoadLocal, "Submit");
printf("</TD></TR>\n");
printf("<TR><TD colspan=2></TD></TR>\n");

printf("<TR><TD colspan=2>Use settings from a URL (http://..., ftp://...):"
       "</TD>\n");
printf("<TD>\n");
cgiMakeOnKeypressTextVar(hgsLoadUrlName,
			 cartUsualString(cart, hgsLoadUrlName, ""),
			 20, jsPressOnEnter(hgsDoLoadUrl));
printf("&nbsp;&nbsp;");
cgiMakeButton(hgsDoLoadUrl, "Submit");
printf("</TD></TR>\n");

printf("</TABLE>\n");

printf("<P></P>\n");
printf("Please note: the above URL option is <em>not</em> for loading track hubs or assembly hubs.\n");
printf("To load those data resources into the browser, please visit the <a href=\"../cgi-bin/hgHubConnect\"\n");
printf("target=\"_blank\">Track Hubs</a> listing page, click the \"Connected Hubs\" tab, and enter the hub URL there.\n");

printf("<P></P>\n");
}

static struct dyString *dyPrintCheckExistingSessionJs(struct slName *existingSessionNames,
                                                      char *exceptName)
/* Write JS that will pop up a confirm dialog if the user's new session name is the same
 * (case-insensitive) as any existing session name, i.e. they would be overwriting it.
 * If exceptName is given, then it's OK for the new session name to match that. */
{
struct dyString *js = dyStringNew(1024);
struct slName *sn;
// MySQL does case-insensitive comparison because our DEFAULT CHARSET=latin1;
// use case-insensitive comparison here to avoid clobbering (#15051).
dyStringAppend(js, "var su, si = document.getElementsByName('" hgsNewSessionName "'); ");
dyStringAppend(js, "if (si[0]) { su = si[0].value.trim().toUpperCase(); ");
if (isNotEmpty(exceptName))
    dyStringPrintf(js, "if (su !== '%s'.toUpperCase()) { ", exceptName);
dyStringAppend(js, "if ( ");
for (sn = existingSessionNames;  sn != NULL;  sn = sn->next)
    {
    char nameUpper[PATH_LEN];
    safecpy(nameUpper, sizeof(nameUpper), sn->name);
    touppers(nameUpper);
    dyStringPrintf(js, "su === ");
    dyStringQuoteString(js, '\'', nameUpper);
    dyStringPrintf(js, "%s", (sn->next ? " || " : " )"));
    }
dyStringAppend(js, " { return confirm('This will overwrite the contents of the existing "
               "session ' + si[0].value.trim() + '.  Proceed?'); } }");
if (isNotEmpty(exceptName))
    dyStringAppend(js, " }");
return js;
}

void showSavingOptions(char *userName, struct slName *existingSessionNames)
/* Show options for saving a new named session in our db or to a file. */
{
printf("<H3>Save Settings</H3>\n");
printf("<TABLE BORDERWIDTH=0>\n");

if (isNotEmpty(userName))
    {
    printf("<TR><TD colspan=4>Save current settings as named session:"
	   "</TD></TR>\n"
	   "<TR><TD>&nbsp;&nbsp;&nbsp;</TD><TD>Name:</TD><TD>\n");
    cgiMakeOnKeypressTextVar(hgsNewSessionName,
			     hubConnectSkipHubPrefix(cartUsualString(cart, "db", "mySession")),
			     20, jsPressOnEnter(hgsDoNewSession));
    printf("&nbsp;&nbsp;&nbsp;");
    cgiMakeCheckBox(hgsNewSessionShare,
		    cartUsualBoolean(cart, hgsNewSessionShare, TRUE));
    printf("Allow this session to be loaded by others\n");
    printf("</TD><TD>");
    printf("&nbsp;");
    if (existingSessionNames)
	{
        struct dyString *js = dyPrintCheckExistingSessionJs(existingSessionNames, NULL);
	cgiMakeOnClickSubmitButton(js->string, hgsDoNewSession, "Submit");
	dyStringFree(&js);
	}
    else
	cgiMakeButton(hgsDoNewSession, "submit");
    printf("</TD></TR>\n");
    printf("<TR><TD colspan=4></TD></TR>\n");
    }

printf("<TR><TD colspan=4>Save current settings to a local file:</TD></TR>\n");
printf("<TR><TD>&nbsp;&nbsp;&nbsp;</TD><TD>File:</TD><TD>\n");
cgiMakeOnKeypressTextVar(hgsSaveLocalFileName,
			 cartUsualString(cart, hgsSaveLocalFileName, ""),
			 20, jsPressOnEnter(hgsDoSaveLocal));
printf("&nbsp;&nbsp;&nbsp;");
printf("File type returned: ");
char *compressType = cartUsualString(cart, hgsSaveLocalFileCompress, textOutCompressNone);
cgiMakeRadioButton(hgsSaveLocalFileCompress, textOutCompressNone,
		   differentWord(textOutCompressGzip, compressType));
printf("&nbsp;plain text&nbsp&nbsp");
cgiMakeRadioButton(hgsSaveLocalFileCompress, textOutCompressGzip,
		   sameWord(textOutCompressGzip, compressType));
printf("&nbsp;gzip compressed (ignored if output file is blank)");
printf("</TD><TD>");
printf("&nbsp;");
cgiMakeButton(hgsDoSaveLocal, "Submit");
printf("</TD></TR>\n");
printf("<TR><TD></TD><TD colspan=3>(leave file blank to get output in "
       "browser window)</TD></TR>\n");
printf("<TR><TD colspan=4></TD></TR>\n");

printf("<TR><TD colspan=4>Save Custom Tracks:</TD></TR>\n");
printf("<TR><TD>&nbsp;&nbsp;&nbsp;</TD><TD colspan=2>");
printf("Back up custom tracks to archive .tar.gz</TD>");
printf("<TD>");
printf("&nbsp;");
cgiMakeButton(hgsShowDownloadPrefix, "Submit");
printf("</TD></TR>\n");

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

printf("<P>See the <A HREF=\"../goldenPath/help/hgSessionHelp.html\" "
       "TARGET=_BLANK>Sessions User's Guide</A> "
       "for more information about this tool. "
       "See the <A HREF=\"../goldenPath/help/sessions.html\" "
       "TARGET=_BLANK>Session Gallery</A> "
       "for example sessions.</P>\n");

showCartLinks();

printf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=%s "
       "ENCTYPE=\"multipart/form-data\">\n",
       hgSessionName(), formMethod);
cartSaveSession(cart);

struct slName *existingSessionNames = NULL;
if (isNotEmpty(userName))
    existingSessionNames = showExistingSessions(userName);
else if (savedSessionsSupported)
     printf("<P>If you <A HREF=\"%s\">sign in</A>, "
         "you will also have the option to save named sessions.</P>\n",
         wikiLinkUserLoginUrl(cartSessionId(cart)));
showSavingOptions(userName, existingSessionNames);
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
cartRemovePrefix(cart, hgsShowDownloadPrefix);
cartRemovePrefix(cart, hgsMakeDownloadPrefix);
cartRemovePrefix(cart, hgsDoDownloadPrefix);
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

static void outAttachedHubUrls(struct cart *cart, struct dyString *dy)
/* output the hubUrls for all attached hubs in the cart. */
{
struct hubConnectStatus *statusList = hubConnectStatusListFromCart(cart, NULL);

if (statusList == NULL)
    return;

if (dy)
    dyStringPrintf(dy,"&assumesHub=");
else
    printf("assumesHub ");
for(; statusList; statusList = statusList->next)
    {
    if (dy)
        dyStringPrintf(dy,"%d=%s ", statusList->id, cgiEncode(statusList->hubUrl));
    else
        printf("%d=%s ", statusList->id, statusList->hubUrl);
    }
if (dy == NULL)
    printf("\n");
}

static void outDefaultTracks(struct cart *cart, struct dyString *dy)
/* Output the default trackDb visibility for all tracks
 * in trackDb if the track is not mentioned in the cart. */
{
database = cartString(cart, "db");
struct trackDb *tdb = NULL;
// Some old sessions reference databases that are no longer present, and that triggers an errAbort
// when calling hgTrackDb.  Just move on instead of errAborting.
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    tdb = hTrackDb(database);
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    fprintf(stderr, "outDefaultTracks: Error from hTrackDb: '%s'; Continuing...",
            errCatch->message->string);
    tdb = NULL;
    }
errCatchFree(&errCatch);

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
static int saveCartAsSession(struct sqlConnection *conn, char *encUserName, char *encSessionName,
                             int sharingLevel)
/* Save all settings in cart, either adding a new session or overwriting an existing session.
 * Return useCount so that the caller can distinguish between adding and overWriting. */
{
struct sqlResult *sr = NULL;
struct dyString *dy = dyStringNew(16 * 1024);
char **row;
char *firstUse = NULL;
int useCount = INITIAL_USE_COUNT;
char *settings = "";

boolean gotSettings = (sqlFieldIndex(conn, namedSessionTable, "settings") >= 0);

/* If this session already existed, preserve its firstUse, useCount,
 * and settings (if available). */
if (gotSettings)
    sqlDyStringPrintf(dy, "SELECT firstUse, useCount, settings FROM %s "
                  "WHERE userName = '%s' AND sessionName = '%s';",
                  namedSessionTable, encUserName, encSessionName);
else
    sqlDyStringPrintf(dy, "SELECT firstUse, useCount FROM %s "
                  "WHERE userName = '%s' AND sessionName = '%s';",
                  namedSessionTable, encUserName, encSessionName);
sr = sqlGetResult(conn, dy->string);
if ((row = sqlNextRow(sr)) != NULL)
    {
    firstUse = cloneString(row[0]);
    useCount = atoi(row[1]) + 1;
    if (gotSettings)
        {
        settings = cloneString(row[2]);
        if (settings == NULL)
            settings = "";
        }
    }
sqlFreeResult(&sr);

sessionDataSaveSession(cart, encUserName, encSessionName, cgiOptionalString(hgsSessionDataDbSuffix));

/* Remove pre-existing session (if any) before updating. */
dyStringClear(dy);
sqlDyStringPrintf(dy, "DELETE FROM %s WHERE userName = '%s' AND "
                  "sessionName = '%s';",
                  namedSessionTable, encUserName, encSessionName);
sqlUpdate(conn, dy->string);

dyStringClear(dy);
sqlDyStringPrintf(dy, "INSERT INTO %s ", namedSessionTable);
sqlDyStringPrintf(dy, "(userName, sessionName, contents, shared, "
               "firstUse, lastUse, useCount");
if (gotSettings)
    sqlDyStringPrintf(dy, ", settings");
sqlDyStringPrintf(dy, ") VALUES (");
sqlDyStringPrintf(dy, "'%s', '%s', ", encUserName, encSessionName);
sqlDyStringPrintf(dy, "'");
cleanHgSessionFromCart(cart);
struct dyString *encoded = dyStringNew(4096);
cartEncodeState(cart, encoded);

// First output the hubStatus id's for attached trackHubs
outAttachedHubUrls(cart, encoded);

// Now add all the default visibilities to output.
outDefaultTracks(cart, encoded);

sqlDyAppendEscaped(dy, encoded->string);
dyStringFree(&encoded);
sqlDyStringPrintf(dy, "', ");
sqlDyStringPrintf(dy, "%d, ", sharingLevel);
if (firstUse)
    sqlDyStringPrintf(dy, "'%s', ", firstUse);
else
    sqlDyStringPrintf(dy, "now(), ");
sqlDyStringPrintf(dy, "now(), %d", useCount);
if (gotSettings)
    sqlDyStringPrintf(dy, ", '%s'", settings);
sqlDyStringPrintf(dy, ")");
sqlUpdate(conn, dy->string);
dyStringFree(&dy);

/* Prevent modification of custom track collections or quickLifts just saved to namedSessionDb: */
cartCopyLocalHubs(cart);
return useCount;
}

char *doNewSession(char *userName)
/* Save current settings in a new named session.
 * Return a message confirming what we did. */
{
if (userName == NULL)
    return "Unable to save session -- please log in and try again.";
struct dyString *dyMessage = dyStringNew(2048);
char *sessionName = trimSpaces(cartString(cart, hgsNewSessionName));
if (isEmpty(sessionName))
    return "Error: Unable to save a session without a name.  Please add one and try again.";

char *encSessionName = cgiEncodeFull(sessionName);
boolean shareSession = cartBoolean(cart, hgsNewSessionShare);
char *encUserName = cgiEncodeFull(userName);
struct sqlConnection *conn = hConnectCentral();

if (sqlTableExists(conn, namedSessionTable))
    {
    int useCount = saveCartAsSession(conn, encUserName, encSessionName, shareSession);
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
    if (shareSession)
        {
        printShareMessage(dyMessage, encUserName, encSessionName, FALSE);
        }
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
char *whichCmd[] = {"which", convertPath, NULL};
struct pipeline *pl = pipelineOpen1(whichCmd, pipelineWrite | pipelineNoAbort, "/dev/null", NULL, 0);
int convertTestResult = pipelineWait(pl);

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
    "select firstUse from %s where userName = \"%s\" and sessionName = \"%s\"",
    namedSessionTable, encUserName, encSessionName);
char *firstUse = sqlNeedQuickString(conn, query);
sqlSafef(query, sizeof(query), "select idx from gbMembers where userName = '%s'", encUserName);
char *userIdx = sqlQuickString(conn, query);
char *userIdentifier = sessionThumbnailGetUserIdentifier(encUserName, userIdx);
char *destFile = sessionThumbnailFilePath(userIdentifier, encSessionName, firstUse);
if (destFile != NULL)
    {
    struct dyString *hgTracksUrl = dyStringNew(0);
    addSessionLink(hgTracksUrl, encUserName, encSessionName, FALSE, FALSE);
    struct dyString *renderUrl =
        dyStringSub(hgTracksUrl->string, "cgi-bin/hgTracks", "cgi-bin/hgRenderTracks");
    dyStringAppend(renderUrl, "&pix=640");
    char *renderCmd[] = {"wget", "-q", "-O", "-", renderUrl->string, NULL};
    char *convertCmd[] = {convertPath, "-", "-resize", "320", "-crop", "320x240+0+0", destFile, NULL};
    char **cmdsImg[] = {renderCmd, convertCmd, NULL};
    pipelineOpen(cmdsImg, pipelineWrite, "/dev/null", NULL, 0);
    }
return 1;
}


void thumbnailRemove(char *encUserName, char *encSessionName, struct sqlConnection *conn)
/* Unlink thumbnail image for the gallery.  Leaks memory from a generated filename string. */
{
char query[4096];
sqlSafef(query, sizeof(query),
    "select firstUse from %s where userName = \"%s\" and sessionName = \"%s\"",
    namedSessionTable, encUserName, encSessionName);
char *firstUse = sqlNeedQuickString(conn, query);
sqlSafef(query, sizeof(query), "select idx from gbMembers where userName = '%s'", encUserName);
char *userIdx = sqlQuickString(conn, query);
char *userIdentifier = sessionThumbnailGetUserIdentifier(encUserName, userIdx);
char *filePath = sessionThumbnailFilePath(userIdentifier, encSessionName, firstUse);
if (filePath != NULL)
    unlink(filePath);
}

static struct slName *getUserSessionNames(char *encUserName)
/* Return a list of unencoded session names belonging to user. */
{
struct slName *existingSessionNames = NULL;
struct sqlConnection *conn = hConnectCentral();
char query[1024];
sqlSafef(query, sizeof(query), "select sessionName from %s where userName = '%s';",
        namedSessionTable, encUserName);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *encSessionName = row[0];
    char *sessionName = cgiDecodeClone(encSessionName);
    slNameAddHead(&existingSessionNames, sessionName);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
return existingSessionNames;
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
    jsOnEventById("keydown", hgsNewSessionName, highlightAccChanges);

    dyStringPrintf(dyMessage,
		   "&nbsp;&nbsp;<INPUT TYPE=SUBMIT ID=\"%s\" NAME=\"%s\" VALUE=\"Accept changes\">"
		   "&nbsp;&nbsp;<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"Cancel\"> "
		   "<BR>\n",
		   hgsDoSessionChange, hgsDoSessionChange, 
		   hgsCancel);
    struct slName *existingSessionNames = getUserSessionNames(encUserName);
    struct dyString *checkExistingNameJs = dyPrintCheckExistingSessionJs( existingSessionNames, sessionName);
    struct dyString *onClickJs = dyStringCreate(
                    "var pattern = /^\\s*$/;"
                    "if (document.getElementById(\"detailsGalleryCheckbox\").checked &&"
                    "   pattern.test(document.getElementById(\"%s\").value)) {"
                    "       warn('Please add a description to allow this session to be included in the Public Gallery');"
                    "       event.preventDefault();"
                    "} else {"
                    "       %s"
                    "}", hgsNewSessionDescription, checkExistingNameJs->string);
    jsOnEventById("click", hgsDoSessionChange, onClickJs->string);
    dyStringFree(&onClickJs);
    dyStringFree(&checkExistingNameJs);

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
    hubConnectLoadHubs(tmpCart);
    cartCheckForCustomTracks(tmpCart, dyMessage);

    if (gotSettings)
        {
        description = replaceChars(description, "\\\\", "\\__ESC__");
        description = replaceChars(description, "\\r", "\r");
        description = replaceChars(description, "\\n", "\n");
        description = replaceChars(description, "\\__ESC__", "\\");
        char *encDescription = htmlEncode(description);
        dyStringPrintf(dyMessage,
            "Description:<BR>\n"
            "<TEXTAREA NAME=\"%s\" id='%s' ROWS=%d COLS=%d "
            ">%s</TEXTAREA><BR>\n",
            hgsNewSessionDescription, hgsNewSessionDescription, 5, 80,
            encDescription);
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
    cartCopyLocalHubs(cart);
    hubConnectLoadHubs(cart);
    cartHideDefaultTracks(cart);
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
cartCopyLocalHubs(cart);
hubConnectLoadHubs(cart);
cartHideDefaultTracks(cart);
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

cartDumpHgSession(cart);

// First output the hubStatus id's for attached trackHubs
outAttachedHubUrls(cart, NULL);

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
    if (!startsWith("http://",url) && !startsWith("https://",url) && !startsWith("ftp://",url))
        errAbort("Unsupported protocol for loading a file via URL.  Please use http, https, or ftp");
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
	dyStringAppend(dyMessage, " (empty file?).  Your settings have not been changed.");
	lf = NULL;
	}
    dyStringPrintf(dyMessage, "&nbsp;&nbsp;"
	   "<A HREF=\"%shgTracks?%s=%s\">Browser</A>",
	   hLocalHostCgiBinUrl(), 
	   cartSessionVarName(), cartSessionId(cart));
    }
if (lf != NULL)
    {
    lineFileCarefulNewlines(lf);
    struct dyString *dyLoadMessage = dyStringNew(0);
    boolean ok = cartLoadSettingsFromUserInput(lf, cart, NULL, actionVar, dyLoadMessage);
    lineFileClose(&lf);
    if (ok)
        {
        dyStringAppend(dyMessage, dyLoadMessage->string);
        cartCopyLocalHubs(cart);
        hubConnectLoadHubs(cart);
        cartHideDefaultTracks(cart);
        cartCheckForCustomTracks(cart, dyMessage);
        }
    else
        {
        dyStringClear(dyMessage);
        dyStringAppend(dyMessage, "<span style='color: red;'><b>"
                       "Unable to load session: </b></span>");
        dyStringAppend(dyMessage, dyLoadMessage->string);
        dyStringAppend(dyMessage, "The uploaded file needs to have been previously saved from the "
                       "<b>Save Settings</b> section.\n");
        // Looking for the words "custom track" in an error string is hokey, returning an enum
        // from cartLoadSettings would be better, but IMO that isn't worth a big refactoring.
        if (stringIn("custom track", dyLoadMessage->string))
            {
            dyStringPrintf(dyMessage, "If you would like to upload a custom track, please use the "
                           "<a href='%s?%s'>"
                           "Custom Tracks</a> tool.\n",
                           hgCustomName(), cartSidUrlString(cart));
            }
        dyStringAppend(dyMessage, "If you feel you have reached this "
                       "message in error, please contact the "
                       "<A HREF=\"mailto:genome-www@soe.ucsc.edu?subject=Session file upload failed&"
                       "body=Hello Genome Browser team,%0AMy session file failed to upload. "
                       "The error message was:%0A");
        dyStringAppend(dyMessage, cgiEncodeFull(dyLoadMessage->string));
        dyStringAppend(dyMessage, "%0ACan you help me upload the data?\">"
                       "UCSC Genome Browser team</A> for assistance.\n");
        }
    dyStringFree(&dyLoadMessage);
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

char *newName = trimSpaces(cartOptionalString(cart, hgsNewSessionName));
if (isNotEmpty(newName) && !sameString(sessionName, newName))
    {
    char *encNewName = cgiEncodeFull(newName);
    // In case the user has clicked to confirm that they want to overwrite an existing session,
    // delete the existing row before updating the row that will overwrite it.
    sqlSafef(query, sizeof(query), "delete from %s where userName = '%s' and sessionName = '%s';",
             namedSessionTable, encUserName, encNewName);
    sqlUpdate(conn, query);
    sqlSafef(query, sizeof(query),
	  "UPDATE %s set sessionName = '%s' WHERE userName = '%s' AND sessionName = '%s';",
	  namedSessionTable, encNewName, encUserName, encSessionName);
    sqlUpdate(conn, query);
    dyStringPrintf(dyMessage, "Changed session name from %s to <B>%s</B>.\n",
		   sessionName, newName);
    sessionName = newName;
    encSessionName = encNewName;
    renamePrefixedCartVar(hgsEditPrefix         , encOldSessionName, encNewName);
    renamePrefixedCartVar(hgsLoadPrefix         , encOldSessionName, encNewName);
    renamePrefixedCartVar(hgsDeletePrefix       , encOldSessionName, encNewName);
    renamePrefixedCartVar(hgsShowDownloadPrefix , encOldSessionName, encNewName);
    renamePrefixedCartVar(hgsMakeDownloadPrefix , encOldSessionName, encNewName);
    renamePrefixedCartVar(hgsDoDownloadPrefix   , encOldSessionName, encNewName);
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
if (shared)
    printShareMessage(dyMessage, encUserName, encSessionName, FALSE);
return dyStringCannibalize(&dyMessage);
}

static int getSharingLevel(struct sqlConnection *conn, char *encUserName, char *encSessionName)
/* Return the value of 'shared' from the namedSessionDb row for user & session;
 * errAbort if there is no such session. (0 = not shared, 1 = shared by link, 2 = public session) */
{
char query[2048];
sqlSafef(query, sizeof(query), "select shared from %s where userName='%s' and sessionName = '%s';",
         namedSessionTable, encUserName, encSessionName);
char buf[256];
char *sharedStr = sqlQuickQuery(conn, query, buf, sizeof buf);
if (sharedStr == NULL)
    errAbort("Unable to find session for userName='%s' and sessionName='%s'; no result from query '%s'",
             encUserName, encSessionName, query);
return atoi(sharedStr);
}

char *doReSaveSession(char *userName, char *actionVar)
/* Load a session (which may have old trash and customTrash references) and re-save it
 * so that customTrash tables will be moved to customData* databases and trash paths
 * will be replaced with userdata (hg.conf sessionDataDir) paths.
 * NOTE: this is not intended to be reachable by the UI; it is for a script to update
 * old sessions to use the new sessionData locations. */
{
if (userName == NULL)
    return "Unable to re-save session -- please log in and try again.";
struct sqlConnection *conn = hConnectCentral();
char *sessionName = trimSpaces(cartString(cart, hgsNewSessionName));
if (isEmpty(sessionName))
    return "Error: Unable to save a session without a name.  Please add one and try again.";

char *encUserName = cgiEncodeFull(userName);
char *encSessionName = cgiEncodeFull(sessionName);
int sharingLevel = getSharingLevel(conn, encUserName, encSessionName);
cartLoadUserSession(conn, userName, sessionName, cart, NULL, actionVar);
// Don't cartCopyLocalHubs because we're not going to make any track collection changes
hubConnectLoadHubs(cart);
// Some old sessions reference databases that are no longer present, and that triggers an errAbort
// when cartHideDefaultTracks calls hgTrackDb.  Don't let that stop the process of updating other
// stuff in the session.
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    cartHideDefaultTracks(cart);
errCatchEnd(errCatch);
if (errCatch->gotError)
    fprintf(stderr, "doReSaveSession: Error from cartHideDefaultTracks: '%s'; Continuing...",
            errCatch->message->string);
errCatchFree(&errCatch);
struct dyString *dyMessage = dyStringNew(1024);
dyStringPrintf(dyMessage,
               "Re-saved settings from user <B>%s</B>'s session <B>%s</B> "
               "that %s be shared with others.  %s %s",
	       userName, htmlEncode(sessionName), (sharingLevel ? "may" : "may not"),
	       getSessionLink(userName, encSessionName),
	       getSessionEmailLink(encUserName, encSessionName));
cartCheckForCustomTracks(cart, dyMessage);
int useCount = saveCartAsSession(conn, encUserName, encSessionName, sharingLevel);
if (useCount <= INITIAL_USE_COUNT)
    errAbort("Expected useCount of at least %d after re-saving session for "
             "userName='%s', sessionName='%s', but got %d",
             INITIAL_USE_COUNT+1, encUserName, encSessionName, useCount);
hDisconnectCentral(&conn);
return dyStringCannibalize(&dyMessage);
}

// ======================================

void prepBackGroundCall(char **pBackgroundProgress, char *cleanPrefix)
/* fix cart and save state */
{
*pBackgroundProgress = cloneString(cgiUsualString("backgroundProgress", NULL)); 
cartRemove(cart, "backgroundExec");
cartRemove(cart, "backgroundProgress");
cartRemovePrefix(cart, cleanPrefix);
cartSaveState(cart);  // in case it crashes
}

void launchForeAndBackGround(char *operation)
/* update cart, launch background and foreground */
{
char cmd[1024]; 
safef(cmd, sizeof cmd, "./hgSession backgroundExec=%s", operation);

// allow child to see variables loaded from CGI.
// because CGI settings have not been saved back to the cart yet
cartSaveState(cart);   

char *workUrl = NULL;
// automatically adds hgsid
// automatically adds backGroundProgress=%s url.progress for separate channel
// Have to pass the userName manually since background exec will not get cookie,
// but we are no longer using userName which was needed with saved-sessions.
startBackgroundWork(cmd, &workUrl);

htmlOpen("Background Status");

jsInlineF(
    "setTimeout(function(){location = 'hgSession?backgroundStatus=%s&hgsid=%s';},2000);\n", 
    cgiEncode(workUrl), cartSessionId(cart));
htmlClose();
fflush(stdout);
}

void passSubmittedBinaryAsTrashFile(struct hashEl *list)
/* fetch the binary file submitted in memory,
 * and save it to temp trash location,
 * saving the name in the cart. 
 * This is necessary to pass the file to the background process.*/
{
// List should have these two
// hgS_extractUpload_hub_9614_Anc11__binary
// hgS_extractUpload_hub_9614_Anc11__filename
// can have a third __filepath if crashes leaving in the cart.

char *binaryParam = NULL;
struct hashEl *hel = NULL;
for (hel = list; hel; hel = hel->next)
    {
    if (endsWith(hel->name, "__binary"))
	binaryParam = cloneString(hel->name);
    }

if (!binaryParam)
    {
    htmlOpen("No file selected");
    printf("Please choose a saved session custom tracks local backup archive file (.tar.gz) to upload");
    htmlClose();
    exit(0);
    } 

char *binaryValue = cartOptionalString(cart, binaryParam);

char *binInfo = cloneString(binaryValue);
char *words[2];
char *mem;
unsigned long size;
chopByWhite(binInfo, words, ArraySize(words));
mem = (char *)sqlUnsignedLong(words[0]);
size = sqlUnsignedLong(words[1]);

struct tempName tn;
trashDirFile(&tn, "backGround", cartSessionId(cart), ".bin");

writeGulp(tn.forCgi, mem, size); 

// add new cart var with trash path
// hgS_extractUpload_hub_9614_Anc11__filepath
char *varName = replaceChars(binaryParam, "__binary", "__filepath");
cartRemove(cart, varName);  // just in case
cartSetString(cart, varName, tn.forCgi);  // update the cart
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

char *backgroundStatus = cloneString(cartUsualString(cart, "backgroundStatus", NULL));

if (backgroundStatus)
    {
    // clear backgroundStatus from the cart
    cartRemove(cart, "backgroundStatus");
    /* Save cart variables. */
    cartSaveState(cart);
    getBackgroundStatus(backgroundStatus);
    exit(0);
    }

char *backgroundExec = cloneString(cgiUsualString("backgroundExec", NULL));


struct hashEl *showDownloadList = cartFindPrefix(cart, hgsShowDownloadPrefix);
struct hashEl *makeDownloadList = cartFindPrefix(cart, hgsMakeDownloadPrefix);
struct hashEl *doDownloadList = cartFindPrefix(cart, hgsDoDownloadPrefix);

if (showDownloadList)
    showDownloadSessionCtData(showDownloadList);
else if (makeDownloadList)
    {
    if (sameOk(backgroundExec,"makeDownloadSessionCtData"))
	{
	// only one, not a list.
	struct hashEl *hel = makeDownloadList;
	char *param1 = cloneString(hel->name);

	char *backgroundProgress = NULL; 
	prepBackGroundCall(&backgroundProgress, hgsMakeDownloadPrefix);

	makeDownloadSessionCtData(param1, backgroundProgress);

	exit(0);  // cannot return
	}
    else
	{

	launchForeAndBackGround("makeDownloadSessionCtData");

	exit(0);
	}

    }
else if (doDownloadList)
    doDownloadSessionCtData(doDownloadList);
else if (cartVarExists(cart, hgsDoMainPage) || cartVarExists(cart, hgsCancel))
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
else if (cartVarExists(cart, hgsDoReSaveSession))
    {
    char *message = doReSaveSession(userName, hgsDoReSaveSession);
    printf("\n%s\n\n", message);
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

