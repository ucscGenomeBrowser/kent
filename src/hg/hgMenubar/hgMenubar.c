#include "common.h"
#include "dystring.h"
#include "filePath.h"
#include "linefile.h"

#define CGI_NAME "cgi-bin/hgMenubar"

struct dyString *navBarFilePath(char *cgiPath, char *docRoot)
{
struct dyString *navPath = dyStringCreate("%s", docRoot);
// Memory leak - replaceChars returns memory that is never freed
char *incPath = replaceChars(cgiPath, "/cgi-bin/hgMenubar", "/inc/globalNavBar.inc");

dyStringAppend(navPath, incPath);
return navPath;
}


void printMenuBar(char *cgiPath, char *docRoot, char *pagePath)
{
struct dyString *navBarLoc = navBarFilePath(cgiPath, docRoot);
struct lineFile *menuFile = lineFileOpen(dyStringContents(navBarLoc), TRUE);
char* oldLine = NULL;
int lineSize = 0;

char *cgiContainerPath = replaceChars(cgiPath, CGI_NAME, "");
char *newPath = makeRelativePath(pagePath, cgiContainerPath);

struct dyString *oldHref = dyStringCreate("href=\"../");
struct dyString *newHref = dyStringCreate("href=\"%s", newPath);

printf ("Content: text/html\r\n\r\n");

while (lineFileNext(menuFile, &oldLine, &lineSize))
    {
    // Not quite as robust as perl search and replace - no variable whitespace handling
    // Also lots of memory leakage - every line is reallocated and forgotten
    char *newLine = replaceChars(oldLine, dyStringContents(oldHref), dyStringContents(newHref));
    printf("%s\n", newLine);
    }

lineFileClose(&menuFile);
}


void parseEnvOrDie (char **cgiPath, char** docRoot, char** pagePath)
{
*cgiPath = getenv("SCRIPT_NAME");
*docRoot = getenv("DOCUMENT_ROOT");
*pagePath = getenv("DOCUMENT_URI");
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
printMenuBar(cgiPath, docRoot, pagePath);
return 0;
}
