/*
 * This CGI is used by static html pages to show a menu bar.
 * On an Apache with activated SSI, a html statement like
 * <!--#include virtual="../cgi-bin/hgMenubar"-->
 * will include the menu bar into a static page.
 */
#include "common.h"
#include "cheapcgi.h"
#include "dystring.h"
#include "filePath.h"
#include "linefile.h"
#include "jsHelper.h"
#include "wikiLink.h"
#include "portable.h"

#define CGI_NAME "cgi-bin/hgMenubar"
#define NAVBAR_INC_PATH "/inc/globalNavBar.inc"
#define NAVBAR_INC_DIR "/inc/"    /* the only directory an incFile value may name, refs #38052 */
#define OLD_HREF "href=\"../"

char* errMessage;

static char *pageReturnUrl(char *pagePath)
/* Return the CGI-encoded URL of the static page this menu bar is included into, to hand to
 * hgLogin as its returnto so login and logout come back to that page (refs #38192).  NULL if
 * we cannot build one hgLogin would accept, and then the links keep their old hgSession
 * target.  pagePath is the path part, from REDIRECT_URL or DOCUMENT_URI. */
{
if (isEmpty(pagePath))
    return NULL;
struct dyString *dy = dyStringNew(256);
dyStringPrintf(dy, "http%s://%s%s", cgiAppendSForHttps(), cgiServerNamePort(), pagePath);
char *encoded = wikiLinkEncodePageReturnUrl(dy->string);
dyStringFree(&dy);
return encoded;
}

char *loginLinkHtml(char *pagePath)
/* Return HTML <li> for the top-right Login menu item, or "" if no login system is configured.
 * Static-page variant: all of the URLs come from wikiLink and are absolute, so the caller's
 * OLD_HREF substitution leaves them alone.  topLinks.js turns the logged-in item into a
 * dialog. */
{
if (!(loginSystemEnabled() || wikiLinkEnabled()))
    return cloneString("");
struct dyString *dy = dyStringNew(512);
char *userName = wikiLinkUserName();
// There is no hgsid on a static page, so the return URL carries none either
char *retEnc = pageReturnUrl(pagePath);
if (userName == NULL)
    {
    // Link straight to the login page (absolute URL from wikiLink), not through hgSession.
    char *loginUrl = retEnc ? wikiLinkUserLoginUrlReturning("", retEnc)
                            : wikiLinkUserLoginUrl("");
    dyStringPrintf(dy, "<a class='topRightLink' href=\"%s\" id='loginLink' "
        "title='Log in to save and share sessions'>Login</a>", loginUrl);
    }
else
    {
    char *logoutUrl = retEnc ? wikiLinkUserLogoutUrlReturning("", retEnc)
                             : wikiLinkUserLogoutUrl("");
    char *changePwUrl = retEnc ? wikiLinkChangePasswordUrlReturning("", retEnc)
                               : wikiLinkChangePasswordUrl("");
    char *changeEmailUrl = retEnc ? wikiLinkChangeEmailUrlReturning("", retEnc)
                                  : wikiLinkChangeEmailUrl("");
    char *changeRecovEmailUrl = retEnc ? wikiLinkChangeRecovEmailUrlReturning("", retEnc)
                                       : wikiLinkChangeRecovEmailUrl("");
    dyStringPrintf(dy, "<a class='topRightLink' href='#' id='loginLink' "
        "title='Account info and sign out' "
        "data-username=\"%s\" data-logouturl=\"%s\" data-changepwurl=\"%s\" "
        "data-changeemailurl=\"%s\" data-changerecovemailurl=\"%s\">%s</a>",
        userName, logoutUrl, changePwUrl ? changePwUrl : "",
        changeEmailUrl ? changeEmailUrl : "",
        changeRecovEmailUrl ? changeRecovEmailUrl : "", userName);
    }
freez(&retEnc);
return dyStringCannibalize(&dy);
}

char *incFilePath(char *cgiPath, char *filePath, char *docRoot)
/* Replace CGI_NAME in cgiPath with docRoot/filePath.  filePath must begin with "/" eg "/inc/..." */
{
char *incPath = replaceChars(cgiPath, "/"CGI_NAME, filePath);
return catTwoStrings(docRoot, incPath);
}

void printIncludes(char* baseDir, char *docRoot)
{
// Cache-buster for the menu-bar CSS/JS: append ?v=<file mtime> so browsers refetch these when
// they change instead of serving a stale cached copy (the CGIs get this from
// webTimeStampedLinkToResource, but that emits its own "../"-relative URL which is wrong for the
// arbitrary-depth static pages this menu bar is included into, so we reuse just its mtime idea).
// fileExists guards fileModTime, which would otherwise abort the menu bar on every static page if
// a resource were missing.
char jsPath[PATH_LEN], cssPath[PATH_LEN];
safef(jsPath, sizeof jsPath, "%s/js/topLinks.js", docRoot);
safef(cssPath, sizeof cssPath, "%s/style/nice_menu.css", docRoot);
long jsVer = fileExists(jsPath) ? (long)fileModTime(jsPath) : 0;
long cssVer = fileExists(cssPath) ? (long)fileModTime(cssPath) : 0;

printf ("<noscript><div class='noscript'><div class='noscript-inner'><p><b>JavaScript is disabled in your web browser</b></p><p>You must have JavaScript enabled in your web browser to use the Genome Browser</p></div></div></noscript>\n");
printf ("<script type='text/javascript' SRC='%sjs/jquery.js'></script>\n", baseDir);
printf ("<script type='text/javascript' SRC='%sjs/jquery.plugins.js'></script>\n", baseDir);
printf("<script type='text/javascript' SRC='%s/js/utils.js'></script>\n", baseDir);
printf("<script type='text/javascript' SRC='%s/js/topLinks.js?v=%ld'></script>\n", baseDir, jsVer);
printf ("<LINK rel='STYLESHEET' href='%sstyle/nice_menu.css?v=%ld' TYPE='text/css'>\n", baseDir,
    cssVer);
}

void printMenuBar(char *cgiPath, char *docRoot, char *pagePath, char *filePath)
{
char *navBarLoc = incFilePath(cgiPath, filePath, docRoot);
struct lineFile *menuFile = lineFileOpen(navBarLoc, TRUE);
char* oldLine = NULL;
int lineSize = 0;

char *cgiContainerPath = replaceChars(cgiPath, CGI_NAME, "");
char *newPath = makeRelativePath(pagePath, cgiContainerPath);

char *newHref = catTwoStrings("href=\"", newPath);

printf ("Content-type: text/html\r\n\r\n");

if (sameString(filePath, NAVBAR_INC_PATH))
    printIncludes(newPath, docRoot);

while (lineFileNext(menuFile, &oldLine, &lineSize))
    {
    // Not quite as robust as perl search and replace - no variable whitespace handling
    // Also lots of memory leakage - every line is reallocated and forgotten
    char *line = oldLine;
    // Fill the top-right link placeholders.  Login shows the user or a link to hgSession;
    // the Share-a-link button is browser-only, so it is dropped on static pages.
    if (stringIn("<!-- LOGIN_LINK -->", line))
        line = replaceChars(line, "<!-- LOGIN_LINK -->", loginLinkHtml(pagePath));
    if (stringIn("<!-- SHARE_LINK -->", line))
        line = replaceChars(line, "<!-- SHARE_LINK -->", "");
    char *newLine = replaceChars(line, OLD_HREF, newHref);
    printf("%s\n", newLine);
    }

lineFileClose(&menuFile);
// links to hgTracks need to use the web browser width and set the hgTracks image
// size in pixels correctly to match the hgGateway "GO" button
jsInline("$(\"#tools1 ul li a\").each( function (a) {\n"
"    if (this.href && this.href.indexOf(\"hgTracks\") !== -1) {\n"
"        var obj = this;\n"
"        obj.onclick = function(e) {\n"
"            var pix = calculateHgTracksWidth();\n"
"            e.currentTarget.href += \"&pix=\" + pix;\n"
"        }\n"
"    }\n"
"});\n");
// if the user has previously searched for assemblies, add them to the "Genomes" menu heading,
// above the "other" assemblies link
jsInline("addRecentGenomesToMenuBar();\n");
jsInlineFinish();
}


void parseEnvOrDie (char **cgiPath, char** docRoot, char** pagePath)
{
*cgiPath = getenv("SCRIPT_NAME");
*docRoot = getenv("DOCUMENT_ROOT");
*pagePath = getenv("REDIRECT_URL");
if (*pagePath == NULL)
    *pagePath = getenv("DOCUMENT_URI");
if (*pagePath == NULL)
    {
    *pagePath = cloneString("/inc/");
    errMessage = "Error: hgMenubar was run without the REDIRECT_URL or DOCUMENT_URI variable set. Looks like it wasn't run from an SSI statement. Defaulting to the 'inc/' directory, avoids errors in the Apache error log.";
    }

if ( (*cgiPath == NULL) || (*docRoot == NULL) || (*pagePath == NULL) )
    {
    fprintf (stderr, "Error: bad invocation of menubar\n");
    exit (1);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *cgiPath, *docRoot, *pagePath;
parseEnvOrDie(&cgiPath, &docRoot, &pagePath);
cgiSpoof(&argc, argv);
char *incFile = cgiUsualString("incFile", NAVBAR_INC_PATH);
/* SECURITY (refs #38052): incFile names a server-side include that we open and print
 * line by line, so a request must not be able to point it wherever it likes.  The path
 * is built as docRoot + incFile with no traversal stripping, so a value like
 * "/../../../../etc/passwd" would echo any file the server can read.  Require the known
 * include directory and no "..".  Fall back to the normal menu bar on anything else
 * rather than aborting, because this CGI is included into every static page and an
 * abort would break the page instead of just ignoring a bad parameter. */
if (!startsWith(NAVBAR_INC_DIR, incFile) || stringIn("..", incFile) != NULL)
    {
    fprintf(stderr, "hgMenubar: ignoring unexpected incFile value [%s]\n", incFile);
    incFile = NAVBAR_INC_PATH;
    }
printMenuBar(cgiPath, docRoot, pagePath, incFile);
if (errMessage)
    puts(errMessage);
return 0;
}
