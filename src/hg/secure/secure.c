/* hgGateway - Human Genome Browser Gateway. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "web.h"
#include "cart.h"
#include "hdb.h"
#include "dbDb.h"
#include "web.h"
#include "hui.h"
#include "hgFind.h"
#include "hui.h"
#include "linefile.h"

static char const rcsid[] = "$Id: secure.c,v 1.7 2008/09/03 19:21:19 markd Exp $";


/* GLOBAL DECLARATIONS */
char *keyFile = "/usr/local/apache/htdocs/license/secure/.keys";
char *secureDir = "/usr/local/apache/htdocs/license/secure";
struct cart *cart = NULL;
struct hash *oldVars = NULL;
/* END GLOBAL DECLARATIONS */

boolean isValidKey(char *key)
/*
  Verify if a key matches our database key
*/
{
struct lineFile *file = NULL;
char *line = NULL;
int len = 0;

if (NULL == key || strlen(key) == 0)
    {
    return FALSE;
    }

file = lineFileOpen(keyFile, TRUE);
while (lineFileNext(file, &line, &len))
{
if (0 == strcmp(key, line))
    {
    return TRUE;
    }
}

return FALSE;
}

void printFile(char *filename)
/*
  Fucntion that prints the contents of this file to stdout.
*/
{
char path[256];
struct lineFile *lf = NULL;
char *line = NULL;
int len = 0;

/* Protect against ../.. URL hacking */
if(strstr(filename, "../"))
{
printf("INVALID FILE PATH %s", filename);
return;
}

snprintf(path, 256, "%s/%s", secureDir, filename);
lf = lineFileOpen(path, TRUE);
while (lineFileNext(lf, &line, &len))
    {
    printf("%s\n", line);
    }
}

void printIndexFile()
/*
  Prints the index.html file to stdout
*/
{
printFile("index.html");
}

void doMiddle(struct cart *theCart)
/* Set up pretty web display and save cart in global. */
{
char *key = NULL;
char *file = NULL;

cart = theCart;

key = cartOptionalString(cart, "key");
if (isValidKey(key))
    {
    file = cartOptionalString(cart, "file");
    if(NULL == file)
        {
        printIndexFile();
        }
    else
        {
        printFile(file);
        }
    }
else
    {
    cartWebStart(theCart, database, "SECURE CGI \n");
    cartRemove(cart, "key");
    printf("INVALID KEY\n");
    cartWebEnd();
    }
}

char *excludeVars[] = {"SUBMIT", "submit", "FILE", "file"};

int main(int argc, char *argv[])
/* Process command line. */
{
oldVars = hashNew(8);
cgiSpoof(&argc, argv);

cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
