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

#define CGI_NAME "cgi-bin/hgMenubar"
#define NAVBAR_INC_PATH "/inc/globalNavBar.inc"
#define OLD_HREF "href=\"../"

char* errMessage;

char *incFilePath(char *cgiPath, char *filePath, char *docRoot)
/* Replace CGI_NAME in cgiPath with docRoot/filePath.  filePath must begin with "/" eg "/inc/..." */
{
char *incPath = replaceChars(cgiPath, "/"CGI_NAME, filePath);
return catTwoStrings(docRoot, incPath);
}

void printIncludes(char* baseDir)
{
printf ("<noscript><div class='noscript'><div class='noscript-inner'><p><b>JavaScript is disabled in your web browser</b></p><p>You must have JavaScript enabled in your web browser to use the Genome Browser</p></div></div></noscript>\n");
printf ("<script type='text/javascript' SRC='%sjs/jquery.js'></script>\n", baseDir);
printf ("<script type='text/javascript' SRC='%sjs/jquery.plugins.js'></script>\n", baseDir);
printf("<script type='text/javascript' SRC='%s/js/utils.js'></script>\n", baseDir);
printf ("<LINK rel='STYLESHEET' href='%sstyle/nice_menu.css' TYPE='text/css'>\n", baseDir);
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
    printIncludes(newPath);

while (lineFileNext(menuFile, &oldLine, &lineSize))
    {
    // Not quite as robust as perl search and replace - no variable whitespace handling
    // Also lots of memory leakage - every line is reallocated and forgotten
    char *newLine = replaceChars(oldLine, OLD_HREF, newHref);
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
printMenuBar(cgiPath, docRoot, pagePath, incFile);
if (errMessage)
    puts(errMessage);
return 0;
}
