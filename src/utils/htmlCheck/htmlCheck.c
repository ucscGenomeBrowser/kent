/* htmlCheck - Do a little reading and verification of html file. */
#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"
#include "net.h"

static char const rcsid[] = "$Id: htmlCheck.c,v 1.6 2004/02/28 08:58:34 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "htmlCheck - Do a little reading and verification of html file\n"
  "usage:\n"
  "   htmlCheck how url\n"
  "where how is:\n"
  "   ok - just check for 200 return.  Print error message and exit -1 if no 200\n"
  "   printAll - read the url (header and html) and print to stdout\n"
  "   printHeader - read the header and print to stdout\n"
  "   printHtml - print the html, but not the header to stdout\n"
  "   printLinks - print links\n"
  "   printTags - print out just the tags\n"
  "   validate - do some basic validations including TABLE/TR/TD nesting\n"
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

struct httpAttribute
/* An http attribute - part of a set of name/values pairs in form. */
    {
    struct httpAttribute *next;
    char *name;		/* Attribute name. */
    char *val;		/* Attribute value. */
    };

struct httpTag
/* A http tag - includes attribute list and parent, but no text. */
    {
    struct httpTag *next;
    char *name;	/* Tag name. */
    struct httpAttribute *attributes;  /* Attribute list. */
    char *start;  /* Start of this tag. */
    char *end;	  /* End of tag (one past closing '>') */
    };

struct httpPage
/* A complete http page parsed out. */
    {
    struct httpPage *next;
    struct httpStatus *status;		/* Version and status */
    char *fullText;			/* Full unparsed text including headers. */
    char *htmlText;			/* Text unparsed after header. */
    struct hash *header;		/* Hash of header lines (cookies, etc.) */
    struct httpTag *tags;		/* List of tags in this page. */
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

struct httpTag *httpTagScan(char *html)
/* Scan HTML for tags and return a list of them. */
{
char *dupe = cloneString(html);
char *s = dupe, c, *e, *tagName;
struct slName *list = NULL, *link;
struct httpTag *tagList, *tag;
struct httpAttribute *att;
int pos;

for (;;)
    {
    c = *s++;
    if (c == 0)
        break;
    if (c == '<')
        {
	if (*s == '!')	/* HTML comment. */
	    {
	    s += 1;
	    if (s[0] == '-' && s[1] == '-')
	        s = stringIn("-->", s);
	    else
		s = strchr(s, '>');
	    if (s == NULL)
		{
	        warn("End of file in comment");
		break;
		}
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
	    
	    /* Allocate tag, fill in name, and stick it on list. */
	    AllocVar(tag);
	    tag->name = cloneString(tagName);
	    slAddHead(&tagList, tag);
	    pos = tagName - dupe - 1;
	    tag->start = html+pos;

	    /* If already got end tag (or EOF) stop processing tag. */
	    if (c == '>' || c == 0)
		{
		tag->end = html + (e - dupe);
	        continue;
		}

	    /* Process name/value pairs until get end tag. */
	    for (;;)
		{
		char *name, *val;
		boolean gotEnd = FALSE;

		/* Check for end tag. */
		s = skipLeadingSpaces(s);
		if (s[0] == '>' || s[0] == 0)
		    {
		    tag->end = html + (s - dupe);
		    if (s[0] == '>')
			tag->end += 1;
		    break;
		    }

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
			    tag->end = html + (e - dupe);
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
		AllocVar(att);
		att->name = cloneString(name);
		att->val = cloneString(val);
		slAddTail(&tag->attributes, att);
		s = e;
		if (gotEnd)
		    break;
		}
	    }
	}
    }
freeMem(dupe);
slReverse(&tagList);
return tagList;
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
page->fullText = html;
page->status = status;
page->header = httpHeaderRead(&s);
page->htmlText = html + (s - dupe);
page->tags = httpTagScan(s);
freeMem(dupe);
return page;
}

struct httpPage *httpPageParseOk(char *html)
/* Parse out page and return only if status ok. */
{
struct httpPage *page = httpPageParse(html);
if (page == NULL)
   noWarnAbort();
if (page->status->status != 200)
   errAbort("Page returned with status code %d", page->status->status);
return page;
}

struct slName *httpPageScanAttribute(struct httpPage *page, 
	char *tagName, char *attribute)
/* Scan page for values of particular attribute in particular tag.
 * if tag is NULL then scans in all tags. */
{
struct httpTag *tag;
struct httpAttribute *att;
struct slName *list = NULL, *el;

for (tag = page->tags; tag != NULL; tag = tag->next)
    {
    if (tagName == NULL || sameWord(tagName, tag->name))
        {
	for (att = tag->attributes; att != NULL; att = att->next)
	    {
	    if (sameWord(attribute, att->name))
	        {
		el = slNameNew(att->val);
		slAddHead(&list, el);
		}
	    }
	}
    }
slReverse(&list);
return list;
}

struct slName *httpPageLinks(struct httpPage *page)
/* Scan through tags list and pull out HREF attributes. */
{
return httpPageScanAttribute(page, NULL, "HREF");
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

void printLinks(struct httpPage *page)
/* Print out all links. */
{
struct slName *link, *linkList = httpPageLinks(page);
for (link = linkList; link != NULL; link = link->next)
    {
    printf("%s\n", link->name);
    }
}

void printTags(struct httpPage *page)
/* Print out all tags. */
{
struct httpTag *tag;
struct httpAttribute *att;
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

struct httpRow
/* Data on a row */
    {
    struct httpRow *next;
    int tdCount;
    int inTd;
    };

struct httpTable 
/* Data on a table. */
    {
    struct httpTable *next;
    struct httpRow *row;
    int rowCount;
    };

void tagAbort(struct httpTag *tag, char *format, ...)
/* Print abort message and some context of tag. */
{
char context[80];
va_list args;
va_start(args, format);
strncpy(context, tag->start, sizeof(context));
context[sizeof(context)-1] = 0;
warn("Error: %s", context);
vaErrAbort(format, args);
va_end(args);
}

void httpValidateTables(struct httpTag *startTag, struct httpTag *endTag)
/* Validate <TABLE><TR><TD> are all properly nested, and that there
 * are no empty rows. */
{
struct httpTable *tableStack = NULL, *table;
struct httpRow *row;
struct httpTag *tag;

for (tag = startTag; tag != endTag; tag = tag->next)
    {
    if (sameWord(tag->name, "TABLE"))
        {
	if (tableStack != NULL)
	    {
	    if (tableStack->row == NULL || !tableStack->row->inTd)
	    tagAbort(tag, "TABLE inside of another table, but not inside of <TR><TD>\n");
	    }
	AllocVar(table);
	slAddHead(&tableStack, table);
	}
    else if (sameWord(tag->name, "/TABLE"))
        {
	if ((table = tableStack) == NULL)
	    tagAbort(tag, "Extra </TABLE> tag");
	if (table->rowCount == 0)
	    tagAbort(tag, "<TABLE> with no <TR>'s");
	if (table->row != NULL)
	    tagAbort(tag, "</TABLE> inside of a row");
	tableStack = table->next;
	freez(&table);
	}
    else if (sameWord(tag->name, "TR"))
        {
	if ((table = tableStack) == NULL)
	    tagAbort(tag, "<TR> outside of TABLE");
	if (table->row != NULL)
	    tagAbort(tag, "<TR>...<TR> with no </TR> in between");
	AllocVar(table->row);
	table->rowCount += 1;
	}
    else if (sameWord(tag->name, "/TR"))
        {
	if ((table = tableStack) == NULL)
	    tagAbort(tag, "</TR> outside of TABLE");
	if (table->row == NULL)
	    tagAbort(tag, "</TR> with no <TR>");
	if (table->row->inTd)
	    tagAbort(tag, "</TR> while <TD> is open");
	if (table->row->tdCount == 0)
	    tagAbort(tag, "Empty row in <TABLE>");
	freez(&table->row);
	}
    else if (sameWord(tag->name, "TD") || sameWord(tag->name, "TH"))
        {
	if ((table = tableStack) == NULL)
	    tagAbort(tag, "<TD> outside of <TABLE>");
	if ((row = table->row) == NULL)
	    tagAbort(tag, "<TD> outside of <TR>");
	if (row->inTd)
	    tagAbort(tag, "<TD>...<TD> with no </TD> in between");
	row->inTd = TRUE;
	row->tdCount += 1;
	}
    else if (sameWord(tag->name, "/TD") || sameWord(tag->name, "/TH"))
        {
	if ((table = tableStack) == NULL)
	    tagAbort(tag, "</TD> outside of <TABLE>");
	if ((row = table->row) == NULL)
	    tagAbort(tag, "</TD> outside of <TR>");
	if (!row->inTd)
	    tagAbort(tag, "</TD> with no <TD>");
	row->inTd = FALSE;
	}
    }
if (tableStack != NULL)
    tagAbort(tag, "Missing </TABLE>");
}

struct httpTag *validateBody(struct httpTag *startTag)
/* Go through tags from current position (just past <BODY>)
 * up to and including </BODY> and check some things. */
{
struct httpTag *tag, *endTag = NULL;

/* First search for end tag. */
for (tag = startTag; tag != NULL; tag = tag->next)
    {
    if (sameWord(tag->name, "/BODY"))
        {
	endTag = tag;
	break;
	}
    }
if (endTag == NULL)
    errAbort("Missing </BODY>");
httpValidateTables(startTag, endTag);
return endTag->next;
}

void validate(struct httpPage *page)
/* Do some basic validations. */
{
struct httpTag *tag;
boolean gotTitle;

/* Validate header, and make a suggestion or two */
if ((tag = page->tags) == NULL)
    errAbort("No tags");
if (!sameWord(tag->name, "HTML"))
    errAbort("Doesn't start with <HTML> tag");
tag = tag->next;
if (tag == NULL || !sameWord(tag->name, "HEAD"))
    errAbort("<HEAD> tag does not follow <HTML> tag");
for (;;)
    {
    tag = tag->next;
    if (tag == NULL)
        errAbort("Missing </HEAD>");
    if (sameWord(tag->name, "TITLE"))
        gotTitle = TRUE;
    if (sameWord(tag->name, "/HEAD"))
        break;
    }
tag = tag->next;
if (tag == NULL || !sameWord(tag->name, "BODY"))
    errAbort("<BODY> tag does not follow <HTML> tag");
tag = validateBody(tag->next);
if (tag == NULL || !sameWord(tag->name, "/HTML"))
    errAbort("Missing </HTML>");
verbose(1, "ok\n");
}

void htmlCheck(char *command, char *url)
/* Read url. Switch on command and dispatch to appropriate routine. */
{
struct dyString *dy = netSlurpUrl(url);
char *fullText = dy->string;
if (sameString(command, "printAll"))
    mustWrite(stdout, dy->string, dy->stringSize);
else if (sameString(command, "ok"))
    checkOk(fullText);
else if (sameString(command, "printHeader"))
    printHeader(fullText);
else /* Do everything that requires full parsing. */
    {
    struct httpPage *page = httpPageParseOk(fullText);
    if (sameString(command, "printHtml"))
        fputs(page->htmlText, stdout);
    else if (sameString(command, "printLinks"))
	printLinks(page);
    else if (sameString(command, "printTags"))
	printTags(page);
    else if (sameString(command, "validate"))
	validate(page);	
    else
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
