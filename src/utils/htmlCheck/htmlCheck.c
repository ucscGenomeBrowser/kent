/* htmlCheck - Do a little reading and verification of html file. */

#include "common.h"
#include "errabort.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"
#include "filePath.h"
#include "net.h"
#include "htmlPage.h"

static char const rcsid[] = "$Id: htmlCheck.c,v 1.31 2008/08/05 17:54:00 kuhn Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "htmlCheck - Do a little reading and verification of html file\n"
  "usage:\n"
  "   htmlCheck how url\n"
  "where how is:\n"
  "   ok - just check for 200 return.  Print error message and exit -1 if no 200\n"
  "   getAll - read the url (header and html) and print to stdout\n"
  "   getHeader - read the header and print to stdout\n"
  "   getCookies - print list of cookies\n"
  "   getHtml - print the html, but not the header to stdout\n"
  "   getForms - print the form structure to stdout\n"
  "   getVars - print the form variables to stdout\n"
  "   getLinks - print links\n"
  "   getTags - print out just the tags\n"
  "   checkLinks - check links in page\n"
  "   checkLinks2 - check links in page and all subpages in same host\n"
  "             (Just one level of recursion)\n"
  "   checkLocalLinks - check local links in page\n"
  "   checkLocalLinks2 - check local links in page and connected local pages\n"
  "             (Just one level of recursion)\n"
  "   submit - submit first form in page if any using 'GET' method\n"
  "   validate - do some basic validations including TABLE/TR/TD nesting\n"
  "options:\n"
  "   cookies=cookie.txt - Cookies is a two column file\n"
  "           containing <cookieName><space><value><newLine>\n"
  "note: url will need to be in quotes if it contains an ampersand."
  );
}

static struct optionSpec options[] = {
   {"cookies", OPTION_STRING},
   {NULL, 0},
};

void checkOk(char *fullText)
/* Parse out first line and check it's ok. */
{
struct htmlStatus *status = htmlStatusParse(&fullText);
if (status == NULL)
    noWarnAbort();
if (status->status != 200)
    errAbort("Status code %d", status->status);
}

void getHeader(char *html)
/* Parse out and print header. */
{
char *line;
while ((line = htmlNextCrLfLine(&html)) != NULL)
    {
    if (line == NULL || line[0] == 0)
	break;
    printf("%s\r\n", line);
    }
}

void getLinks(struct htmlPage *page)
/* Print out all links. */
{
struct slName *link, *linkList = htmlPageLinks(page);
for (link = linkList; link != NULL; link = link->next)
    {
    printf("%s\n", link->name);
    }
}

void htmlPrintForms(struct htmlPage *page, FILE *f)
/* Print out all forms. */
{
struct htmlForm *form;
for (form = page->forms; form != NULL; form = form->next)
    htmlFormPrint(form, f);
}

void getVars(struct htmlPage *page)
/* Print out all forms. */
{
struct htmlForm *form;
struct htmlFormVar *var;
for (form = page->forms; form != NULL; form = form->next)
    {
    for (var = form->vars; var != NULL; var = var->next)
	htmlFormVarPrint(var, stdout, "");
    }
}

void getTags(struct htmlPage *page)
/* Print out all tags. */
{
struct htmlTag *tag;
struct htmlAttribute *att;
for (tag = page->tags; tag != NULL; tag = tag->next)
    {
    printf("%s", tag->name);
    for (att = tag->attributes; att != NULL; att = att->next)
        {
	printf(" %s=", att->name);
	if (hasWhiteSpace(att->val))
	    printf("\"%s\"", att->val);
	else
	    printf("%s", att->val);
	}
    printf("\n");
    }
}

void getCookies(struct htmlPage *page)
/* Print out all cookies. */
{
struct htmlCookie *cookie;
for (cookie = page->cookies; cookie != NULL; cookie = cookie->next)
    {
    printf("%s\t%s\t%s\t%s\t%s\t%s\n",
    	cookie->name, cookie->value, naForNull(cookie->domain), 
	naForNull(cookie->path), naForNull(cookie->expires),
	(cookie->secure ? "SECURE" : "UNSECURE"));
    }
}

void quickSubmit(struct htmlPage *page)
/* Just press submit on first form. */
{
struct htmlPage *newPage;
if (page->forms == NULL)
    errAbort("No forms on %s", page->url);
newPage = htmlPageFromForm(page, page->forms, "submit", "Submit");
htmlPageValidateOrAbort(newPage);
}


char *skipOverProtocol(char *s)
/* Skip over http:// or ftp:// or https:// */
{
char *p;
if ((p = stringIn("://", s)) != NULL)
    return p+3;
else
    return s;
}

int hostNameSize(char *s)
/* Return size of host name (not including last slash) */
{
char *e = strchr(s, '/');
if (e == NULL)
    return strlen(s);
else
    return e - s;
}

boolean sameHost(char *a, char *b)
/* Given URLs a and b, return TRUE if they refer to same host. */
{
int aSize, bSize;
a = skipOverProtocol(a);
b = skipOverProtocol(b);
aSize = hostNameSize(a);
bSize = hostNameSize(b);
if (aSize != bSize)
    return FALSE;
return (memcmp(a, b, aSize) == 0);
}

static struct htmlTag *findNamedAnchor(struct htmlPage *page, char *name)
/* Find anchor of given name. */
{
struct htmlTag *tag;
for (tag = page->tags; tag != NULL; tag = tag->next)
    {
    if (sameWord(tag->name, "A"))
        {
	char *anchorName = htmlTagAttributeVal(page, tag, "name", NULL);
	if (anchorName != NULL && sameWord(anchorName, name))
	    return tag;
	}
    }
return NULL;
}

static jmp_buf recoverJumpBuf;

static void recoverAbort()
/* semiAbort */
{
longjmp(recoverJumpBuf, -1);
}

char *slurpUrl(char *url)
/* Grab url.  If there's a problem report error and return NULL */
{
int status;
char *retVal = NULL;
pushAbortHandler(recoverAbort);
status = setjmp(recoverJumpBuf);
if (status == 0)    /* Always true except after long jump. */
    {
    struct dyString *dy = netSlurpUrl(url);
    retVal = dyStringCannibalize(&dy);
    }
popAbortHandler();
return retVal;
}

void checkRecursiveLinks(struct hash *uniqHash, struct htmlPage *page, 
	int depth, boolean justLocal)
/* Check links recursively up to depth. */
{
struct slName *linkList = htmlPageLinks(page), *link;
for (link = linkList; link != NULL; link = link->next)
    {
    if (link->name[0] == '#')
        {
	if (findNamedAnchor(page, link->name+1) == NULL)
	    {
	    warn("%s%s doesn't exist", page->url, link->name);
	    }
	}
    else
	{
	char *url = htmlExpandUrl(page->url, link->name);
	if (url != NULL)
	    {
	    boolean isLocal = sameHost(page->url, url);
	    if (isLocal || !justLocal)
		{
		if (!hashLookup(uniqHash, url))
		    {
		    struct hash *headerHash = newHash(8);
		    int status = netUrlHead(url, headerHash);
		    hashAdd(uniqHash, url, NULL);
		    if (status != 200 && status != 302 && status != 301)
			warn("%d from %s", status, url);
		    else
			{
			if (depth > 1 && isLocal)
			    {
			    char *contentType = hashFindVal(headerHash, "Content-Type:");
			    if (contentType != NULL && startsWith("text/html", contentType))
				{
				char *fullText = slurpUrl(url);
				if (fullText != NULL)
				    {
				    struct htmlPage *newPage = 
				       htmlPageParse(url, fullText);
				    if (newPage != NULL && newPage->status->status==200)
					{
					fullText = NULL;
					printf("Recursing into %s\n", url);
					checkRecursiveLinks(uniqHash, newPage, 
					    depth-1, justLocal);
					htmlPageFree(&newPage);
					}
				    freez(&fullText);
				    }
				}
			    }
			}
		    hashFree(&headerHash);
		    }
		}
	    freez(&url);
	    }
	}
    }
slFreeList(&linkList);
}

void checkLinks(struct htmlPage *page, int depth, boolean justLocal)
/* Check links (just one level deep. */
{
struct hash *uniqHash = hashNew(0);
hashAdd(uniqHash, page->url, NULL);
checkRecursiveLinks(uniqHash, page, depth, justLocal);
hashFree(&uniqHash);
}

struct htmlCookie *readCookies(char *fileName)
/* Read cookies from file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct htmlCookie *list = NULL, *cookie;
char *line, *word;
while (lineFileNextReal(lf, &line))
    {
    word = nextWord(&line);
    line = skipLeadingSpaces(line);
    if (line == NULL)
        errAbort("Missing cookie value line %d of %s", lf->lineIx, lf->fileName);
    AllocVar(cookie);
    cookie->name = cloneString(word);
    cookie->value = cloneString(line);
    slAddHead(&list, cookie);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

void htmlCheck(char *command, char *url, char *cookieFile)
/* Read url. Switch on command and dispatch to appropriate routine. */
{
char *fullText;
struct htmlCookie *cookies = NULL;

if (cookieFile != NULL)
    cookies = readCookies(cookieFile);
    fullText = htmlSlurpWithCookies(url, cookies);
if (sameString(command, "getAll"))
    mustWrite(stdout, fullText, strlen(fullText));
else if (sameString(command, "ok"))
    checkOk(fullText);
else if (sameString(command, "getHeader"))
    getHeader(fullText);
else /* Do everything that requires full parsing. */
    {
    struct htmlPage *page = htmlPageParseOk(url, fullText);
    if (sameString(command, "getHtml"))
        fputs(page->htmlText, stdout);
    else if (sameString(command, "getLinks"))
	getLinks(page);
    else if (sameString(command, "getForms"))
        htmlPrintForms(page, stdout);
    else if (sameString(command, "getVars"))
        getVars(page);
    else if (sameString(command, "getTags"))
	getTags(page);
    else if (sameString(command, "getCookies"))
        getCookies(page);
    else if (sameString(command, "submit"))
        quickSubmit(page);
    else if (sameString(command, "validate"))
	{
	htmlPageValidateOrAbort(page);	
	verbose(1, "ok\n");
	}
    else if (sameString(command, "checkLinks"))
        checkLinks(page, 1, FALSE);
    else if (sameString(command, "checkLinks2"))
        checkLinks(page, 2, FALSE);
    else if (sameString(command, "checkLocalLinks"))
        checkLinks(page, 1, TRUE);
    else if (sameString(command, "checkLocalLinks2"))
        checkLinks(page, 2, TRUE);
    else
	errAbort("Unrecognized command %s", command);
    htmlPageFree(&page);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(200000000);
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
htmlCheck(argv[1], argv[2], optionVal("cookies",NULL));
carefulCheckHeap();
return 0;
}
