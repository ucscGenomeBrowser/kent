/* htmlCheck - Do a little reading and verification of html file. */
#include "common.h"
#include "errabort.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"
#include "net.h"

static char const rcsid[] = "$Id: htmlCheck.c,v 1.14 2004/02/29 18:08:57 kent Exp $";

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
  "   getHtml - print the html, but not the header to stdout\n"
  "   getForms - print the form structure to stdout\n"
  "   getVars - print the form variables to stdout\n"
  "   getLinks - print links\n"
  "   getTags - print out just the tags\n"
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

struct htmlAttribute
/* An html attribute - part of a set of name/values pairs in form. */
    {
    struct htmlAttribute *next;
    char *name;		/* Attribute name. */
    char *val;		/* Attribute value. */
    };

struct htmlTag
/* A html tag - includes attribute list and parent, but no text. */
    {
    struct htmlTag *next;
    char *name;	/* Tag name. */
    struct htmlAttribute *attributes;  /* Attribute list. */
    char *start;  /* Start of this tag. */
    char *end;	  /* End of tag (one past closing '>') */
    };

struct htmlFormVar
/* A variable within an html form - from input, button, etc. */
    {
    struct htmlFormVar *next;	/* Next in list. */
    char *name;			/* Variable name. */
    char *tagName;		/* Name of tag. */
    char *type;			/* Variable type. */
    char *curVal;		/* Current value if any. */
    struct slName *values;	/* List of available values.  Null if textBox. */
    struct slRef *tags;	        /* List of references associated tags. */
    };

struct htmlForm
/* A form within an html page. */
    {
    struct htmlForm *next;	/* Next form in list. */
    char *name;			/* Name (or "" if not defined). */
    char *action;		/* Action attribute value. */
    char *method;		/* Method attribute value.  Defaults to "GET" */
    struct htmlTag *startTag;	/* Tag that holds <FORM> */
    struct htmlTag *endTag;	/* Tag one past </FORM> */
    struct htmlFormVar *vars; /* List of form variables. */
    };

struct htmlPage
/* A complete html page parsed out. */
    {
    struct htmlPage *next;
    struct httpStatus *status;		/* Version and status */
    char *fullText;			/* Full unparsed text including headers. */
    struct hash *header;		/* Hash of header lines (cookies, etc.) */
    char *htmlText;			/* Text unparsed after header. */
    struct htmlTag *tags;		/* List of tags in this page. */
    struct htmlForm *forms;		/* List of all forms. */
    };

static int findLineNumber(char *start, char *pos)
/* Figure out line number of given position relative to start. */
{
char *s;
int line = 1;
for (s = start; s <= pos; ++s)
    {
    if (s[0] == '\n')
       ++line;
    }
return line;
}

void tagVaWarn(struct htmlPage *page, struct htmlTag *tag, char *format, 
	va_list args)
/* Print warning message and some context of tag. */
{
char context[80];
strncpy(context, tag->start, sizeof(context));
context[sizeof(context)-1] = 0;
warn("Error near line %d: %s", findLineNumber(page->htmlText, tag->start), 
	context);
vaWarn(format, args);
}

void tagWarn(struct htmlPage *page, struct htmlTag *tag, char *format, ...)
/* Print warning message and some context of tag. */
{
va_list args;
va_start(args, format);
tagVaWarn(page, tag, format, args);
va_end(args);
}

void tagAbort(struct htmlPage *page, struct htmlTag *tag, char *format, ...)
/* Print abort message and some context of tag. */
{
va_list args;
va_start(args, format);
tagVaWarn(page, tag, format, args);
va_end(args);
noWarnAbort();
}

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

struct httpStatus *httpStatusParse(char **pText)
/* Read in status from first line.  Update pText to point to next line. */
{
char *line = nextCrLfLine(pText);
char *word;
struct httpStatus *status;
if (line == NULL)
    {
    warn("Empty http output.");
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

struct hash *htmlHeaderRead(char **pHtml)
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


char *tagAttributeVal(struct htmlPage *page, struct htmlTag *tag, 
	char *name, char *defaultVal)
/* Return value of named attribute, or defaultVal if attribute doesn't exist. */
{
struct htmlAttribute *att;
for (att = tag->attributes; att != NULL; att = att->next)
    {
    if (sameWord(att->name, name))
        return att->val;
    }
return defaultVal;
}

char *tagAttributeNeeded(struct htmlPage *page, struct htmlTag *tag, char *name)
/* Return named tag attribute.  Complain and return "n/a" if it
 * doesn't exist. */
{
char *val = tagAttributeVal(page, tag, name, NULL);
if (val == NULL)
    {
    tagWarn(page, tag, "Missing %s attribute", name);
    val = "n/a";
    }
return val;
}

struct htmlTag *htmlTagScan(char *html)
/* Scan HTML for tags and return a list of them. */
{
char *dupe = cloneString(html);
char *s = dupe, c, *e, *tagName;
struct slName *list = NULL, *link;
struct htmlTag *tagList, *tag;
struct htmlAttribute *att;
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
		e = s;
		for (;;)
		    {
		    c = *e;
		    if (c == '=')
		        break;
		    else if (c == '>')
		        break;
		    else if (c == 0)
		        break;
		    e += 1;
		    }
		if (c == 0)
		    {
		    warn("End of file in tag");
		    break;
		    }
		name = s;
		*e++ = 0;
		eraseTrailingSpaces(name);
		if (c == '>')
		    {
		    val = "";
		    gotEnd = TRUE;
		    tag->end = html + (e - dupe);
		    }
		else
		    {
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

static struct htmlFormVar *findOrMakeVar(struct htmlPage *page, char *name, 
	struct hash *hash, struct htmlTag *tag, struct htmlFormVar **pVarList)
/* Find variable of existing name if it exists,  otherwise
 * make a new one and add to hash and list.  Add reference
 * to this tag to var. */
{
struct htmlFormVar *var = hashFindVal(hash, name);
if (var == NULL)
    {
    AllocVar(var);
    var->name = name;
    var->tagName = tag->name;
    hashAdd(hash, name, var);
    slAddHead(pVarList, var);
    }
else
    {
    if (!sameWord(var->tagName, tag->name))
        {
	tagWarn(page, tag, "Mixing FORM variable tag types %s and %s", 
		var->tagName, tag->name);
	var->tagName = tag->name;
	}
    }
refAdd(&var->tags, tag);
return var;
}

struct htmlFormVar *formParseVars(struct htmlPage *page, struct htmlForm *form)
/* Return a list of variables parsed out of form.  
 * A form variable is something that may appear in the name
 * side of the name=value pairs that serves as input to a CGI
 * script.  The variables may be constructed from buttons, 
 * INPUT tags, OPTION lists, or TEXTAREAs. */
{
struct htmlTag *tag;
struct htmlFormVar *varList = NULL, *var;
struct hash *hash = newHash(0);
for (tag = form->startTag->next; tag != form->endTag; tag = tag->next)
    {
    if (sameWord(tag->name, "INPUT"))
        {
	char *type = tagAttributeNeeded(page, tag, "TYPE");
	char *varName = tagAttributeNeeded(page, tag, "NAME");
	char *value = tagAttributeVal(page, tag, "VALUE", NULL);
	var = findOrMakeVar(page, varName, hash, tag, &varList); 
	if (var->type != NULL && !sameWord(var->type, type))
	    tagWarn(page, tag, "Mixing input types %s and %s", var->type, type);
	var->type = type;
	if (sameWord(type, "TEXT") || sameWord(type, "PASSWORD") 
		|| sameWord(type, "FILE") || sameWord(type, "HIDDEN"))
	    {
	    var->curVal = value;
	    }
	else if (sameWord(type, "CHECKBOX") || sameWord(type, "RADIO"))
	    {
	    if (tagAttributeVal(page, tag, "CHECKED", NULL) != NULL)
	        var->curVal = value;
	    }
	else if ( sameWord(type, "RESET") || sameWord(type, "BUTTON") ||
		sameWord(type, "SUBMIT") || sameWord(type, "IMAGE") ||
		sameWord(type, "n/a"))
	    {
	    /* Do nothing. */
	    }
	else
	    {
	    tagWarn(page, tag, "Unrecognized INPUT TYPE %s", type);
	    }
	}
    else if (sameWord(tag->name, "SELECT"))
        {
	char *varName = tagAttributeNeeded(page, tag, "NAME");
	struct htmlTag *subTag;
	var = findOrMakeVar(page, varName, hash, tag, &varList); 
	for (subTag = tag->next; subTag != form->endTag; subTag = subTag->next)
	    {
	    if (sameWord(subTag->name, "/SELECT"))
	        break;
	    else if (sameWord(subTag->name, "OPTION"))
	        {
		if (tagAttributeVal(page, subTag, "SELECTED", NULL) != NULL)
		    {
		    char *selVal = tagAttributeVal(page, subTag, "VALUE", NULL);
		    if (selVal == NULL)
		        {
			char *e = strchr(subTag->end, '<');
			if (e != NULL)
			    selVal = cloneStringZ(subTag->end, e - subTag->end);
			}
		    if (selVal != NULL)
			var->curVal = selVal;
		    }
		}
	    }
	}
    else if (sameWord(tag->name, "TEXTAREA"))
        {
	char *varName = tagAttributeNeeded(page, tag, "NAME");
	char *e = strchr(tag->end, '<');
	var = findOrMakeVar(page, varName, hash, tag, &varList); 
	if (e != NULL)
	    var->curVal = cloneStringZ(tag->end, e - tag->end);
	}
    }
slReverse(&varList);
for (var = varList; var != NULL; var = var->next)
    slReverse(&var->tags);
return varList;
}

struct htmlForm *htmlParseForms(struct htmlPage *page,
	struct htmlTag *startTag, struct htmlTag *endTag)
/* Parse out list of forms from tag stream. */
{
struct htmlForm *formList = NULL, *form = NULL;
struct htmlTag *tag;
for (tag = startTag; tag != endTag; tag = tag->next)
    {
    if (sameWord(tag->name, "FORM"))
        {
	if (form != NULL)
	    tagWarn(page, tag, "FORM inside of FORM");
	AllocVar(form);
	form->startTag = tag;
	slAddHead(&formList, form);
	form->name = tagAttributeVal(page, tag, "name", "n/a");
	form->action = tagAttributeNeeded(page, tag, "action");
	form->method = tagAttributeVal(page, tag, "method", "GET");
	}
    else if (sameWord(tag->name, "/FORM"))
        {
	if (form == NULL)
	    tagWarn(page, tag, "/FORM outside of FORM");
	else
	    {
	    form->endTag = tag->next;
	    form = NULL;
	    }
	}
    }
slReverse(&formList);
for (form = formList; form != NULL; form = form->next)
    {
    form->vars = formParseVars(page, form);
    }
return formList;
}

struct htmlPage *htmlPageParse(char *fullText)
/* Parse out page and return. */
{
struct htmlPage *page;
char *dupe = cloneString(fullText);
char *s = dupe;
struct httpStatus *status = httpStatusParse(&s);

if (status == NULL)
    return NULL;
AllocVar(page);
page->fullText = fullText;
page->status = status;
page->header = htmlHeaderRead(&s);
page->htmlText = fullText + (s - dupe);
page->tags = htmlTagScan(s);
page->forms = htmlParseForms(page, page->tags, NULL);
return page;
}

struct htmlPage *htmlPageParseOk(char *fullText)
/* Parse out page and return only if status ok. */
{
struct htmlPage *page = htmlPageParse(fullText);
if (page == NULL)
   noWarnAbort();
if (page->status->status != 200)
   errAbort("Page returned with status code %d", page->status->status);
return page;
}

struct slName *htmlPageScanAttribute(struct htmlPage *page, 
	char *tagName, char *attribute)
/* Scan page for values of particular attribute in particular tag.
 * if tag is NULL then scans in all tags. */
{
struct htmlTag *tag;
struct htmlAttribute *att;
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

struct slName *htmlPageLinks(struct htmlPage *page)
/* Scan through tags list and pull out HREF attributes. */
{
return htmlPageScanAttribute(page, NULL, "HREF");
}

void checkOk(char *fullText)
/* Parse out first line and check it's ok. */
{
struct httpStatus *status = httpStatusParse(&fullText);
if (status == NULL)
    noWarnAbort();
if (status->status != 200)
    errAbort("Status code %d", status->status);
}

void getHeader(char *html)
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

void getLinks(struct htmlPage *page)
/* Print out all links. */
{
struct slName *link, *linkList = htmlPageLinks(page);
for (link = linkList; link != NULL; link = link->next)
    {
    printf("%s\n", link->name);
    }
}

static char *naForNull(char *s)
/* Return 'n/a' if s is NULL, otherwise s. */
{
if (s == NULL) 
   s = "n/a";
return s;
}

void getForms(struct htmlPage *page)
/* Print out all forms. */
{
struct htmlForm *form;
struct htmlFormVar *var;
for (form = page->forms; form != NULL; form = form->next)
    {
    printf("%s\t%s\t%s\n", form->name, form->method, form->action);
    for (var = form->vars; var != NULL; var = var->next)
        {
	struct htmlTag *tag = var->tags->val;
	printf("\t%s\t%s\t%s\t%s\n", var->name, var->tagName, 
		naForNull(var->type), 
		naForNull(var->curVal));
	}
    }
}

void getVars(struct htmlPage *page)
/* Print out all forms. */
{
struct htmlForm *form;
struct htmlFormVar *var;
for (form = page->forms; form != NULL; form = form->next)
    {
    for (var = form->vars; var != NULL; var = var->next)
        {
	struct htmlTag *tag = var->tags->val;
	printf("%s\t%s\t%s\t%s\n", var->name, var->tagName, 
		naForNull(var->type), 
		naForNull(var->curVal));
	}
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

struct htmlRow
/* Data on a row */
    {
    struct htmlRow *next;
    int tdCount;
    int inTd;
    };

struct htmlTable 
/* Data on a table. */
    {
    struct htmlTable *next;
    struct htmlRow *row;
    int rowCount;
    };

void validateTables(struct htmlPage *page, 
	struct htmlTag *startTag, struct htmlTag *endTag)
/* Validate <TABLE><TR><TD> are all properly nested, and that there
 * are no empty rows. */
{
struct htmlTable *tableStack = NULL, *table;
struct htmlRow *row;
struct htmlTag *tag;

for (tag = startTag; tag != endTag; tag = tag->next)
    {
    if (sameWord(tag->name, "TABLE"))
        {
	if (tableStack != NULL)
	    {
	    if (tableStack->row == NULL || !tableStack->row->inTd)
	    tagAbort(page, tag, "TABLE inside of another table, but not inside of <TR><TD>\n");
	    }
	AllocVar(table);
	slAddHead(&tableStack, table);
	}
    else if (sameWord(tag->name, "/TABLE"))
        {
	if ((table = tableStack) == NULL)
	    tagAbort(page, tag, "Extra </TABLE> tag");
	if (table->rowCount == 0)
	    tagAbort(page, tag, "<TABLE> with no <TR>'s");
	if (table->row != NULL)
	    tagAbort(page, tag, "</TABLE> inside of a row");
	tableStack = table->next;
	freez(&table);
	}
    else if (sameWord(tag->name, "TR"))
        {
	if ((table = tableStack) == NULL)
	    tagAbort(page, tag, "<TR> outside of TABLE");
	if (table->row != NULL)
	    tagAbort(page, tag, "<TR>...<TR> with no </TR> in between");
	AllocVar(table->row);
	table->rowCount += 1;
	}
    else if (sameWord(tag->name, "/TR"))
        {
	if ((table = tableStack) == NULL)
	    tagAbort(page, tag, "</TR> outside of TABLE");
	if (table->row == NULL)
	    tagAbort(page, tag, "</TR> with no <TR>");
	if (table->row->inTd)
	    {
	    tagAbort(page, tag, "</TR> while <TD> is open");
	    }
	if (table->row->tdCount == 0)
	    tagAbort(page, tag, "Empty row in <TABLE>");
	freez(&table->row);
	}
    else if (sameWord(tag->name, "TD") || sameWord(tag->name, "TH"))
        {
	if ((table = tableStack) == NULL)
	    tagAbort(page, tag, "<%s> outside of <TABLE>", tag->name);
	if ((row = table->row) == NULL)
	    tagAbort(page, tag, "<%s> outside of <TR>", tag->name);
	if (row->inTd)
	    tagAbort(page, tag, "<%s>...<%s> with no </%s> in between", 
	    	tag->name, tag->name, tag->name);
	row->inTd = TRUE;
	row->tdCount += 1;
	}
    else if (sameWord(tag->name, "/TD") || sameWord(tag->name, "/TH"))
        {
	if ((table = tableStack) == NULL)
	    tagAbort(page, tag, "<%s> outside of <TABLE>", tag->name);
	if ((row = table->row) == NULL)
	    tagAbort(page, tag, "<%s> outside of <TR>", tag->name);
	if (!row->inTd)
	    tagAbort(page, tag, "<%s> with no <%s>", tag->name, tag->name+1);
	row->inTd = FALSE;
	}
    }
if (tableStack != NULL)
    tagAbort(page, tag, "Missing </TABLE>");
}

void checkTagIsInside(struct htmlPage *page, char *outsiders, char *insiders,  
	struct htmlTag *startTag, struct htmlTag *endTag)
/* Check that insiders are all bracketed by outsiders. */
{
char *outDupe = cloneString(outsiders);
char *inDupe = cloneString(insiders);
char *line, *word;
int depth = 0;
struct htmlTag *tag;
struct hash *outOpen = newHash(8);
struct hash *outClose = newHash(8);
struct hash *inHash = newHash(8);
char buf[256];

/* Create hashes of all insiders */
line = inDupe;
while ((word = nextWord(&line)) != NULL)
    {
    touppers(word);
    hashAdd(inHash, word, NULL);
    }

/* Create hash of open and close outsiders. */
line = outDupe;
while ((word = nextWord(&line)) != NULL)
    {
    touppers(word);
    hashAdd(outOpen, word, NULL);
    safef(buf, sizeof(buf), "/%s", word);
    hashAdd(outClose, buf, NULL);
    }

/* Stream through tags making sure that insiders are
 * at least one deep inside of outsiders. */
for (tag = startTag; tag != NULL; tag = tag->next)
    {
    char *type = tag->name;
    if (hashLookup(outOpen, type ))
        ++depth;
    else if (hashLookup(outClose, type))
        --depth;
    else if (hashLookup(inHash, type))
        {
	if (depth <= 0)
	    tagAbort(page, tag, "%s outside of any of %s", type, outsiders);
	}
    }
freeHash(&inHash);
freeHash(&outOpen);
freeHash(&outClose);
freeMem(outDupe);
freeMem(inDupe);
}

void checkNest(struct htmlPage *page,
	char *type, struct htmlTag *startTag, struct htmlTag *endTag)
/* Check that <type> and </type> tags are properly nested. */
{
struct htmlTag *tag;
int depth = 0;
char endType[256];
safef(endType, sizeof(endType), "/%s", type);
for (tag = startTag; tag != endTag; tag = tag->next)
    {
    if (sameWord(tag->name, type))
	++depth;
    else if (sameWord(tag->name, endType))
        {
	--depth;
	if (depth < 0)
	   tagAbort(page, tag, "<%s> without preceding <%s>", endType, type);
	}
    }
if (depth != 0)
    errAbort("Missing <%s> tag", endType);
}

void validateNestingTags(struct htmlPage *page,
	struct htmlTag *startTag, struct htmlTag *endTag,
	char *nesters[], int nesterCount)
/* Validate many tags that do need to nest. */
{
int i;
for (i=0; i<nesterCount; ++i)
    checkNest(page, nesters[i], startTag, endTag);
}

static char *bodyNesters[] = 
/* Nesting tags that appear in body. */
{
    "ADDRESS", "DIV", "H1", "H2", "H3", "H4", "H5", "H6",
    "ACRONYM", "BLOCKQUOTE", "CITE", "CODE", "DEL", "DFN"
    "DIR", "DL", "MENU", "OL", "UL", "CAPTION", "TABLE", 
    "A", "MAP", "OBJECT", "FORM"
};

static char *headNesters[] =
/* Nesting tags that appear in header. */
{
    "TITLE",
};

struct htmlTag *validateBody(struct htmlPage *page, struct htmlTag *startTag)
/* Go through tags from current position (just past <BODY>)
 * up to and including </BODY> and check some things. */
{
struct htmlTag *tag, *endTag = NULL;

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
validateTables(page, startTag, endTag);
checkTagIsInside(page, "DIR MENU OL UL", "LI", startTag, endTag);
checkTagIsInside(page, "DL", "DD DT", startTag, endTag);
checkTagIsInside(page, "COLGROUP TABLE", "COL", startTag, endTag);
checkTagIsInside(page, "MAP", "AREA", startTag, endTag);
checkTagIsInside(page, "FORM", 
	"INPUT BUTTON /BUTTON OPTION SELECT /SELECT TEXTAREA /TEXTAREA"
	"FIELDSET /FIELDSET"
	, 
	startTag, endTag);
validateNestingTags(page, startTag, endTag, bodyNesters, ArraySize(bodyNesters));
return endTag->next;
}

char *urlOkChars()
/* Return array character indexed array that has
 * 1 for characters that are ok in URLs and 0
 * elsewhere. */
{
char *okChars;
int c;
AllocArray(okChars, 256);
for (c=0; c<256; ++c)
    if (isalnum(c))
        okChars[c] = 1;
/* This list is a little more inclusive than W3's. */
okChars['='] = 1;
okChars['-'] = 1;
okChars['.'] = 1;
okChars[';'] = 1;
okChars['_'] = 1;
okChars['&'] = 1;
okChars['+'] = 1;
return okChars;
}

void validateCgiUrl(char *url)
/* Make sure URL follows basic CGI encoding rules. */
{
if (startsWith("http:", url) || startsWith("https:", url))
    {
    static char *okChars = NULL;
    UBYTE c, *s;
    if (okChars == NULL)
	okChars = urlOkChars();
    url = strchr(url, '?');
    if (url != NULL)
	{
	s = (UBYTE*)url+1;
	while ((c = *s++) != 0)
	    {
	    if (!okChars[c])
		{
		errAbort("Character %c not allowed in URL %s", c, url);
		}
	    }
	}
    }
}

void validateCgiUrls(struct htmlPage *page)
/* Make sure URLs in page follow basic CGI encoding rules. */
{
struct htmlForm *form;
struct slName *linkList = htmlPageLinks(page), *link;

for (form = page->forms; form != NULL; form = form->next)
    validateCgiUrl(form->action);
for (link = linkList; link != NULL; link = link->next)
    validateCgiUrl(link->name);
slFreeList(&linkList);
}

void validate(struct htmlPage *page)
/* Do some basic validations. */
{
struct htmlTag *tag;
boolean gotTitle = FALSE;

/* To simplify things upper case all tag names. */
for (tag = page->tags; tag != NULL; tag = tag->next)
    touppers(tag->name);

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
if (!gotTitle)
    warn("No title in <HEAD>");
validateNestingTags(page, page->tags, tag, headNesters, ArraySize(headNesters));
tag = tag->next;
if (tag == NULL || !sameWord(tag->name, "BODY"))
    errAbort("<BODY> tag does not follow <HTML> tag");
tag = validateBody(page, tag->next);
if (tag == NULL || !sameWord(tag->name, "/HTML"))
    errAbort("Missing </HTML>");
validateCgiUrls(page);
verbose(1, "ok\n");
}

void htmlCheck(char *command, char *url)
/* Read url. Switch on command and dispatch to appropriate routine. */
{
struct dyString *dy = netSlurpUrl(url);
char *fullText = dy->string;
if (sameString(command, "getAll"))
    mustWrite(stdout, dy->string, dy->stringSize);
else if (sameString(command, "ok"))
    checkOk(fullText);
else if (sameString(command, "getHeader"))
    getHeader(fullText);
else /* Do everything that requires full parsing. */
    {
    struct htmlPage *page = htmlPageParseOk(fullText);
    if (sameString(command, "getHtml"))
        fputs(page->htmlText, stdout);
    else if (sameString(command, "getLinks"))
	getLinks(page);
    else if (sameString(command, "getForms"))
        getForms(page);
    else if (sameString(command, "getVars"))
        getVars(page);
    else if (sameString(command, "getTags"))
	getTags(page);
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
