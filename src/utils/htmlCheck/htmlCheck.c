/* htmlCheck - Do a little reading and verification of html file. */
#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"
#include "net.h"

static char const rcsid[] = "$Id: htmlCheck.c,v 1.1 2004/02/26 08:30:37 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "htmlCheck - Do a little reading and verification of html file\n"
  "usage:\n"
  "   htmlCheck how url\n"
  "where how is:\n"
  "   ok - just check for 200 return.  Print error message and exit -1 if no 200\n"
  "   read - read the url and print to stdout\n"
  "   header - read the header and print to stdout\n"
  "   printLinks - print links\n"
  "   (validate - check that html is good)\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct httpStatus
/* HTTP version and status code. */
    {
    char *version;
    int status;
    };

struct httpPage
/* A complete http page parsed out. */
    {
    struct httpPage *next;
    struct httpStatus *status;		/* Version and status */
    struct hash *header;		/* Hash of header lines (cookies, etc.) */
    struct slName *links;		/* List of all links. */
    };

static char *nextCrLfLine(char **pS)
/* Return zero-terminated line and advance *pS to start of
 * next line.  Return NULL at end of file.  Warn if there is
 * no <CR>. */
{
char *s = *pS, *e;
if (s == NULL || s[0] == 0)
    return NULL;
e = strchr(s, '\n');
if (e == NULL)
    verbose(1, "End of file in header\n");
else 
    {
    *e = 0;
    if (e == s || e[-1] != '\r')
	verbose(1, "Missing <CR> in header line\n");
    else
       e[-1] = 0;
    e += 1;
    }
*pS = e;
return s;
}

struct httpStatus *httpStatusParse(char **pHtml)
/* Read in status from first line.  Update pHtml to point to next line. */
{
char *line = nextCrLfLine(pHtml);
char *word;
struct httpStatus *status;
if (line == NULL)
    {
    warn("Empty html file.");
    return NULL;
    }
AllocVar(status);
status->version = nextWord(&line);
word = nextWord(&line);
if (word == NULL)
    {
    warn("Short status line.");
    return NULL;
    }
if (!isdigit(word[0]))
    {
    warn("Not a number in status field");
    return NULL;
    }
status->status = atoi(word);
return status;
}

struct hash *httpHeaderRead(char **pHtml)
/* Read in from second line through first blank line and
 * save in hash.  These lines are in the form name: value. */
{
struct hash *hash = hashNew(6);
for (;;)
    {
    char *line = nextCrLfLine(pHtml);
    char *word;
    if (line == NULL)
	{
        warn("End of file in header");
	break;
	}
    word = nextWord(&line);
    if (word == NULL)
        break;
    line = skipLeadingSpaces(line);
    hashAdd(hash, word, cloneString(line));
    }
return hash;
}

struct slName *httpScanAttribute(char *html, char *tag, char *attribute)
/* Scan HTML for values of particular attribute in particular tag.
 * if tag is NULL then scans in all tags. */
{
char *dupe = cloneString(html);
char *s = dupe, c, *e, *tagName;
struct slName *list = NULL, *link;
for (;;)
    {
    c = *s++;
    if (c == 0)
        break;
    if (c == '<')
        {
	if (*s == '!')	/* HTML comment. */
	    {
	    s = strchr(s, '>');
	    }
	else
	    {
	    /* Grab first word into tagName. */
	    e = s;
	    for (;;)
	        {
		c = *e;
		if (c == '>' || c == 0 || isspace(c))
		    break;
		e += 1;
		}
	    if (c != 0)
	       *e++ = 0;
	    tagName = s;
	    s = e;

	    /* If already got end tag (or EOF) stop processing tag. */
	    if (c == '>' || c == 0)
	        continue;

	    /* Process name/value pairs until get end tag. */
	    for (;;)
		{
		char *name, *val;
		boolean gotEnd = FALSE;

		/* Check for end tag. */
		s = skipLeadingSpaces(s);
		if (s[0] == '>' || s[0] == 0)
		    break;

		/* Get name - everything up to equals. */
		e = strchr(s, '=');
		if (e == NULL)
		    {
		    warn("missing '=' in attributes list");
		    break;
		    }
		name = s;
		*e++ = 0;
		eraseTrailingSpaces(name);
		val = e = skipLeadingSpaces(e);
		if (e[0] == '"')
		    {
		    if (!parseQuotedString(val, val, &e))
			break;
		    }
		else
		    {
		    for (;;)
			{
			c = *e;
			if (c == '>')
			    {
			    gotEnd = TRUE;
			    *e++ = 0;
			    break;
			    }
			else if (isspace(c))
			    {
			    *e++ = 0;
			    break;
			    }
			else if (c == 0)
			    break;
			++e;
			}
		    }
		if (tag == NULL || sameWord(tag, tagName))
		    {
		    if (sameWord(attribute, name))
			{
			link = slNameNew(val);
			slAddHead(&list, link);
			}
		    }
		s = e;
		if (gotEnd)
		    break;
		}
	    }
	}
    }
freeMem(dupe);
slReverse(&list);
return list;
}

struct slName *httpLinkScan(char *html)
/* Scan page for links. (HREFs in tags.) */
{
return httpScanAttribute(html, NULL, "HREF");
}

struct httpPage *httpPageParse(char *html)
/* Parse out page and return. */
{
struct httpPage *page;
char *dupe = cloneString(html);
char *s = dupe;
struct httpStatus *status = httpStatusParse(&s);

if (status == NULL)
    return NULL;
AllocVar(page);
page->status = status;
page->header = httpHeaderRead(&s);
page->links = httpLinkScan(s);
freeMem(dupe);
return page;
}

void checkOk(char *html)
/* Parse out first line and check it's ok. */
{
struct httpStatus *status = httpStatusParse(&html);
if (status == NULL)
    noWarnAbort();
if (status->status != 200)
    errAbort("Status code %d", status->status);
}

void printHeader(char *html)
/* Parse out and print header. */
{
char *line;
while ((line = nextCrLfLine(&html)) != NULL)
    {
    if (line == NULL || line[0] == 0)
	break;
    printf("%s\r\n", line);
    }
}

void printLinks(char *html)
/* Print out all links. */
{
struct httpPage *page = httpPageParse(html);
struct slName *link;
for (link = page->links; link != NULL; link = link->next)
    {
    printf("%s\n", link->name);
    }
}

void htmlCheck(char *command, char *url)
/* Read url. Switch on command and dispatch to appropriate routine. */
{
struct dyString *htmlText = netSlurpUrl(url);
if (sameString(command, "read"))
    mustWrite(stdout, htmlText->string, htmlText->stringSize);
else if (sameString(command, "ok"))
    {
    checkOk(htmlText->string);
    }
else if (sameString(command, "header"))
    {
    printHeader(htmlText->string);
    }
else if (sameString(command, "printLinks"))
    {
    printLinks(htmlText->string);
    }
else
    {
    errAbort("Unrecognized command %s", command);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *commmand, *url;
if (argc != 3)
    usage();
htmlCheck(argv[1], argv[2]);
return 0;
}
