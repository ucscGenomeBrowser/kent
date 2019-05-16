/* htmlPage - stuff to read, parse, and submit  htmlPages and forms. 
 *
 * typical usage is:
 *   struct htmlPage *page = htmlPageGet(url);
 *   htmlPageValidateOrAbort(page);
 *   var = htmlPageGetVar(page, page->forms, "org");
 *   if (var != NULL)
 *      printf("Organism = %s\n", var->curVal);
 *   htmlPageSetVar(page, page->forms, "org", "Human");
 *   newPage = htmlPageFromForm(page, page->forms, "submit", "Go");
 */

#ifndef HTMLPAGE_H
#define HTMLPAGE_H

#ifndef DYSTRING_H
#include "dystring.h"
#endif

struct htmlStatus
/* HTTP version and status code. */
    {
    struct htmlStatus *next;	/* Next in list. */
    char *version;		/* Usually something like HTTP/1.1 */
    int status;			/* HTTP status code.  200 is good. */
    };

struct htmlCookie
/* A cookie - stored by browser usually.  We need to echo it
 * back when we post forms. */
    {
    struct htmlCookie *next;	/* Next in list. */
    char *name;			/* Cookie name. */
    char *value;		/* Cookie value. */
    char *domain;		/* The set of web domains this applies to. */
    char *path;			/* Cookie applies below this path I guess. */
    char *expires;		/* Expiration date. */
    boolean secure;		/* Is it a secure cookie? */
    };

struct htmlAttribute
/* An html attribute - part of a set of name/values pairs in a tag. */
    {
    struct htmlAttribute *next;
    char *name;		/* Attribute name. */
    char *val;		/* Attribute value. */
    };

struct htmlTag
/* A html tag - includes attribute list and tag name parsed out, pointers to unparsed text. */
    {
    struct htmlTag *next;
    char *name;	/* Tag name. */
    struct htmlAttribute *attributes;  /* Attribute list. */
    char *start;  /* Start of this tag.  Not allocated here.*/
    char *end;	  /* End of tag (one past closing '>')  Not allocated here.*/
    };

struct htmlFormVar
/* A variable within an html form - from input, button, etc. */
    {
    struct htmlFormVar *next;	/* Next in list. */
    char *name;			/* Variable name.  Not allocated here.*/
    char *tagName;		/* Name of tag.  Not allocated here. */
    char *type;			/* Variable type. Not allocated here. */
    char *curVal;		/* Current value if any.  Allocated here. */
    struct slName *values;	/* List of available values.  Null if textBox. */
    struct slRef *tags;	        /* List of references associated tags. */
    };

struct htmlForm
/* A form within an html page. */
    {
    struct htmlForm *next;	/* Next form in list. */
    char *name;			/* Name (n/a if not defined).  Not allocated here. */
    char *action;		/* Action attribute value.  Not allocated here. */
    char *method;		/* Defaults to "GET". Not allocated here.  */
    struct htmlTag *startTag;	/* Tag that holds <FORM>. Not allocated here.  */
    struct htmlTag *endTag;	/* Tag one past </FORM> . Not allocated here. */
    struct htmlFormVar *vars; /* List of form variables. */
    };

struct htmlPage
/* A complete html page parsed out. */
    {
    struct htmlPage *next;
    char *url;				/* Url that produced this page. */
    struct htmlStatus *status;		/* Version and status. */
    struct hash *header;		/* Hash of header lines (cookies, etc.) */
    struct htmlCookie *cookies;		/* Associated cookies if any. */
    char *fullText;			/* Full unparsed text including headers. */
    char *htmlText;			/* Text unparsed after header.  Same mem as fullText. */
    struct htmlTag *tags;		/* List of tags in this page. */
    struct htmlForm *forms;		/* List of all forms. */
    };

void htmlStatusFree(struct htmlStatus **pStatus);
/* Free up resources associated with status */

void htmlStatusFreeList(struct htmlStatus **pList);
/* Free a list of dynamically allocated htmlStatus's */

struct htmlStatus *htmlStatusParse(char **pText);
/* Read in status from first line.  Update pText to point to next line. 
 * Note unlike many routines here, this does not insert zeros into text. */

void htmlCookieFree(struct htmlCookie **pCookie);
/* Free memory associated with cookie. */

void htmlCookieFreeList(struct htmlCookie **pList);
/* Free a list of dynamically allocated htmlCookie's */

struct htmlCookie *htmlCookieFileRead(char *fileName);
/* Read cookies from a line oriented file.  First word in line
 * is the cookie name, the rest of the line the cookie value. */

void htmlAttributeFree(struct htmlAttribute **pAttribute);
/* Free up resources associated with attribute. */

void htmlAttributeFreeList(struct htmlAttribute **pList);
/* Free a list of dynamically allocated htmlAttribute's */

char *htmlTagAttributeVal(struct htmlPage *page, struct htmlTag *tag, 
	char *name, char *defaultVal);
/* Return value of named attribute, or defaultVal if attribute doesn't exist. */

char *htmlTagAttributeNeeded(struct htmlPage *page, struct htmlTag *tag, char *name);
/* Return named tag attribute.  Complain and return "n/a" if it
 * doesn't exist. */

void htmlTagFree(struct htmlTag **pTag);
/* Free up resources associated with tag. */

void htmlTagFreeList(struct htmlTag **pList);
/* Free a list of dynamically allocated htmlTag's */

void htmlFormVarFree(struct htmlFormVar **pVar);
/* Free up resources associated with form variable. */

void htmlFormVarFreeList(struct htmlFormVar **pList);
/* Free a list of dynamically allocated htmlFormVar's */

void htmlFormVarPrint(struct htmlFormVar *var, FILE *f, char *prefix);
/* Print out variable to file, prepending prefix. */

void htmlFormFree(struct htmlForm **pForm);
/* Free up resources associated with form variable. */

void htmlFormFreeList(struct htmlForm **pList);
/* Free a list of dynamically allocated htmlForm's */

void htmlFormPrint(struct htmlForm *form, FILE *f);
/* Print out form structure. */

char *htmlFormCgiVars(struct htmlPage *page, struct htmlForm *form, 
	char *buttonName, char *buttonVal, struct dyString *dyHeader);
/* Return cgi vars in name=val format from use having pressed
 * submit button of given name and value. */

struct htmlForm *htmlFormGet(struct htmlPage *page, char *name);
/* Get named form. */

struct htmlFormVar *htmlFormVarGet(struct htmlForm *form, char *name);
/* Get named variable. */

void htmlFormVarSet(struct htmlForm *form, char *name, char *val);
/* Set variable to given value.  */

struct htmlFormVar *htmlPageGetVar(struct htmlPage *page, struct htmlForm *form, char *name);
/* Get named variable.  If form is NULL, first form in page is used. */

void htmlPageSetVar(struct htmlPage *page, struct htmlForm *form, char *name, char *val);
/* Set variable to given value.  If form is NULL, first form in page is used. */

void htmlPageFree(struct htmlPage **pPage);
/* Free up resources associated with htmlPage. */

void htmlPageFreeList(struct htmlPage **pList);
/* Free a list of dynamically allocated htmlPage's */

char *expandUrlOnBase(char *base, char *url);
/* Figure out first character past host name. Load up
 * return string with protocol (if any) and host name. 
 * It is assumed that url is relative to base and does not contain a protocol.*/

char *htmlExpandUrl(char *base, char *url);
/* Expand URL that is relative to base to stand on it's own. 
 * Return NULL if it's not http or https. */

char *htmlNextCrLfLine(char **pS);
/* Return zero-terminated line and advance *pS to start of
 * next line.  Return NULL at end of file.  Warn if there is
 * no <CR>. */

struct slName *htmlPageScanAttribute(struct htmlPage *page, 
	char *tagName, char *attribute);
/* Scan page for values of particular attribute in particular tag.
 * if tag is NULL then scans in all tags. */

struct slName *htmlPageLinks(struct htmlPage *page);
/* Scan through tags list and pull out HREF attributes. */

struct slName *htmlPageSrcLinks(struct htmlPage *page);
/* Scan through tags list and pull out SRC attributes. */

void htmlPageFormOrAbort(struct htmlPage *page);
/* Aborts if no FORM found */

void htmlPageValidateOrAbort(struct htmlPage *page);
/* Do some basic validations.  Aborts if there is a problem. */

void htmlPageStrictTagNestCheck(struct htmlPage *page);
/* Do strict tag nesting check.  Aborts if there is a problem. */

char *htmlSlurpWithCookies(char *url, struct htmlCookie *cookies);
/* Send get message to url with cookies, and return full response as
 * a dyString.  This is not parsed or validated, and includes http
 * header lines.  Typically you'd pass this to htmlPageParse() to
 * get an actual page. */

struct htmlPage *htmlPageParse(char *url, char *fullText);
/* Parse out page and return.  Warn and return NULL if problem. */

struct htmlPage *htmlPageParseOk(char *url, char *fullText);
/* Parse out page and return only if status ok. */

struct htmlPage *htmlPageParseNoHead(char *url, char *htmlText);
/* Parse out page in memory (past http header if any) and return. */

struct htmlPage *htmlPageFromForm(struct htmlPage *origPage, struct htmlForm *form, 
	char *buttonName, char *buttonVal);
/* Return a new htmlPage based on response to pressing indicated button
 * on indicated form in origPage. */

struct htmlPage *htmlPageGetWithCookies(char *url, struct htmlCookie *cookies);
/* Get page from URL giving server the given cookies.   Note only the
 * name and value parts of the cookies need to be filled in. */

struct htmlPage *htmlPageGet(char *url);
/* Get page from URL (may be a file). */

struct htmlPage *htmlPageForwarded(char *url, struct htmlCookie *cookies);
/* Get html page.  If it's just a forwarding link then get do the
 * forwarding.  Cookies is a possibly empty list of cookies with
 * name and value parts filled in. */

struct htmlPage *htmlPageForwardedNoAbort(char *url, struct htmlCookie *cookies);
/* Try and get an HTML page.  Print warning and return NULL if there's a problem. */

struct htmlTag *findNextMatchingTag(struct htmlTag *list, char *name);
/* Return first tag in list that is of type name or NULL if not found. */

boolean isSelfClosingTag(struct htmlTag *tag);
/* Return strue if last attributes' name is "/" 
 * Self-closing tags are used with html5 and SGV */

#endif /* HTMLPAGE_H */

