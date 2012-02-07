/* xp - A minimal non-verifying xml parser.  It's
 * stream oriented much like expas.  It's a bit faster
 * and smaller than expas.  I'm not sure it handles unicode
 * as well.
 *
 * This file is copyright 2002-2005 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "dystring.h"
#include "errabort.h"
#include "hash.h"
#include "xp.h"
#include "xmlEscape.h"



char xpNextBuf(struct xp *xp)
/* Fetch a new buffer and return first char.  Return 0 at EOF. */
{
int size = xp->read(xp->userData, xp->inBuf, sizeof(xp->inBuf));
if (size <= 0)
    return 0;
xp->inBufEnd = xp->inBuf + size;
xp->in = xp->inBuf+1;
return xp->inBuf[0];
}

#define xpGetChar(xp) \
    (xp->in < xp->inBufEnd ? *xp->in++ : xpNextBuf(xp))
/* Macro to quickly fetch next char. */

#define xpUngetChar(xp) \
    (--xp->in)
/* Oops, don't fetch that after all. */

struct xp *xpNew(void *userData, 
   void (*atStartTag)(void *userData, char *name, char **atts),
   void (*atEndTag)(void *userData, char *name, char *text),
   int (*read)(void *userData, char *buf, int bufSize),
   char *fileName)
/* Form a new xp parser.  File name may be NULL - just used for
 * error reporting. */
{
struct xp *xp;
AllocVar(xp);
xp->stack = xp->stackBufEnd = xp->stackBuf + ArraySize(xp->stackBuf);
xp->userData = userData;
xp->atStartTag = atStartTag;
xp->atEndTag = atEndTag;
xp->read = read;
xp->lineIx = 1;
xp->endTag = newDyString(64);
if (fileName)
    xp->fileName = cloneString(fileName);
else
    xp->fileName = cloneString("XML");
xp->inBufEnd = xp->in = xp->inBuf;		
xp->symHash = xmlEscapeSymHash();
return xp;
}

int xpReadFromFile(void *userData, char *buf, int bufSize)
/* Read some text assuming a file was passed in as user data. */
{
FILE *f = userData;
return fread(buf, 1, bufSize, f);
}



void xpFree(struct xp **pXp)
/* Free up an xp parser. */
{
int i;
struct xp *xp = *pXp;
if (xp != NULL)
    {
    struct xpStack *stack;
    for (stack = xp->stackBufEnd; --stack >= xp->stackBuf; )
        {
	if (stack->tag == NULL)
	    break;
	freeDyString(&stack->tag);
	freeDyString(&stack->text);
	}
    for (i=0; i<ArraySize(xp->attDyBuf); ++i)
        {
	if (xp->attDyBuf[i] == NULL)
	    break;
	freeDyString(&xp->attDyBuf[i]);
	}
    freeDyString(&xp->endTag);
    freeMem(xp->fileName);
    hashFree(&xp->symHash);
    freez(pXp);
    }
}

int xpLineIx(struct xp *xp)
/* Return current line number. */
{
return xp->lineIx;
}

char *xpFileName(struct xp *xp)
/* Return current file name. */
{
return xp->fileName;
}

void xpError(struct xp *xp, char *format, ...)
/* Output an error message with filename and line number included. */
{
va_list args;
va_start(args, format);
vaWarn(format, args);
errAbort("line %d of %s", xpLineIx(xp), xpFileName(xp));
va_end(args);
}

static void xpUnexpectedEof(struct xp *xp)
/* Squawk and die about EOF. */
{
xpError(xp, "Unexpected end of file.");
}

static void xpEatComment(struct xp *xp, char commentC)
/* Skip characters until comment end. */
{
int startLine = xp->lineIx;
char lastC = 0;
char c;
for (;;)
    {
    if ((c = xpGetChar(xp)) == 0)
        xpError(xp, "End of file in comment that started line %d", startLine);
    if (c == '\n')
        ++xp->lineIx;
    if (c == '>')
        {
	if (lastC == commentC || commentC == '!')
	break;
	}
    lastC = c;
    }
}

static void xpLookup(struct xp *xp, struct dyString *temp, struct dyString *text)
/* Parse after '&' until ';' and look up symbol.  Put result into text. */
{
char c;
char *s;
dyStringClear(temp);
for (;;)
    {
    if ((c = xpGetChar(xp)) == 0)
	xpError(xp, "End of file in after & and before ;");
    if (isspace(c))
        xpError(xp, "& without ;");
    if (c == ';')
        break;
    dyStringAppendC(temp, c);
    }
s = temp->string;
if (s[0] == '#')
    {
    c = atoi(s+1);
    dyStringAppendC(text, c);
    }
else if ((s = hashFindVal(xp->symHash, s)) == NULL)
    {
    dyStringAppendC(text, '&');
    dyStringAppend(text, temp->string);
    dyStringAppendC(text, ';');
    }
else
    {
    dyStringAppend(text, s);
    }
}

void xpForceMatch(struct xp *xp, char *matchString)
/* Make sure that the next characters are match, and eat them. */
{
char *match = matchString, m;
while ((m = *match++) != 0)
    {
    if (m != xpGetChar(xp))
        xpError(xp, "Expecting %s", matchString);
    }
}

void xpTextUntil(struct xp *xp, char *endPattern)
/* Stuff xp->text with everything up to endPattern. */
{
int endSize = strlen(endPattern);
int endPos = 0;
char c;
struct dyString *dy = xp->stack->text;
for (;;)
    {
    if ((c = xpGetChar(xp)) == 0)
	xpUnexpectedEof(xp);
    if (c == endPattern[endPos])
        {
	endPos += 1;
	if (endPos == endSize)
	    return;
	}
    else
        {
	if (endPos > 0)
	    dyStringAppendN(dy, endPattern, endPos);
	dyStringAppendC(dy, c);
	endPos = 0;
	}
    }
}


void xpParseStartTag(struct xp *xp, 
	int maxAttCount,		  /* Maximum attribute count. */
	struct dyString *retName, 	  /* Returns tag name */
	int *retAttCount, 		  /* Returns attribute count. */
	struct dyString **retAttributes,  /* Name, value, name, value... */
	boolean *retClosed)	  /* If true then is self-closing (ends in />) */
/* Call this after the first '<' in a tag has been read.  It'll
 * parse out until the '>' tag. */
{
char c, quotC;
int attCount = 0;
struct dyString *dy;
int lineStart;

dyStringClear(retName);

/* Skip white space after '<' and before tag name. */
for (;;)
    {
    if ((c = xpGetChar(xp)) == 0)
	xpUnexpectedEof(xp);
    if (isspace(c))
        {
	if (c == '\n')
	    ++xp->lineIx;
        }
    else
        break;
    }

/* Read in tag name. */
for (;;)
    {
    dyStringAppendC(retName, c);
    if ((c = xpGetChar(xp)) == 0)
	xpUnexpectedEof(xp);
    if (c == '>' || c == '/' || isspace(c))
        break;
    }
if (c == '\n')
    ++xp->lineIx;

/* Parse attributes. */
if (c != '>' && c != '/')
    {
    for (;;)
	{
	/* Skip leading white space. */
	for (;;)
	    {
	    if ((c = xpGetChar(xp)) == 0)
		xpUnexpectedEof(xp);
	    if (isspace(c))
		{
		if (c == '\n')
		    ++xp->lineIx;
		}
	    else
		break;
	    }
	if (c == '>' || c == '/')
	    break;

	/* Allocate space in attribute table. */
	if (attCount >= maxAttCount - 2)
	    xpError(xp, "Attribute stack overflow");
	dy = retAttributes[attCount];
	if (dy == NULL)
	    dy = retAttributes[attCount] = newDyString(64);
	else
	    dyStringClear(dy);
	++attCount;

	/* Read until not a label character. */
	for (;;)
	    {
	    dyStringAppendC(dy, c);
	    if ((c = xpGetChar(xp)) == 0)
		xpUnexpectedEof(xp);
	    if (isspace(c))
		{
		if (c == '\n')
		    ++xp->lineIx;
		break;
		}
	    if (c == '=')
		break;
	    if (c == '/' || c == '>')
		xpError(xp, "Expecting '=' after attribute name");
	    }

	/* Skip white space until '=' */
	if (c != '=')
	    {
	    for (;;)
		{
		if ((c = xpGetChar(xp)) == 0)
		    xpUnexpectedEof(xp);
		if (isspace(c))
		    {
		    if (c == '\n')
			++xp->lineIx;
		    }
		else
		    break;
		}
	    if (c != '=')
		xpError(xp, "Expecting '=' after attribute name");
	    }

	/* Skip space until quote. */
	for (;;)
	    {
	    if ((c = xpGetChar(xp)) == 0)
		xpUnexpectedEof(xp);
	    else if (isspace(c))
		{
		if (c == '\n')
		    ++xp->lineIx;
		}
	    else
		break;
	    }
	if (c != '\'' && c != '"')
	    xpError(xp, "Expecting quoted string after =");

	/* Allocate space in attribute table. */
	if (attCount >= maxAttCount - 2)
	    xpError(xp, "Attribute stack overflow");
	dy = retAttributes[attCount];
	if (dy == NULL)
	    dy = retAttributes[attCount] = newDyString(64);
	else
	    dyStringClear(dy);
	++attCount;

	/* Read until next quote. */
	quotC = c;
	lineStart = xp->lineIx;
	for (;;)
	    {
	    if ((c = xpGetChar(xp)) == 0)
	       xpError(xp, "End of file inside literal string that started at line %d", lineStart);
	    if (c == quotC)
		break;
	    if (c == '&')
	       xpLookup(xp, xp->endTag, dy);
	    else
		{
		if (c == '\n')
		    ++xp->lineIx;
		dyStringAppendC(dy, c);
		}
	    }
	}
    }
if (c == '/')
    {
    *retClosed = TRUE;
    c = xpGetChar(xp);
    if (c != '>')
        xpError(xp, "Expecting '>' after '/'");
    }
else
    *retClosed = FALSE;
*retAttCount = attCount;
}

void xpParseEndTag(struct xp *xp, char *tagName)
/* Call this after have seen </.  It will parse through
 * > and make sure that the tagName matches. */
{
struct dyString *dy = xp->endTag;
char c;

dyStringClear(dy);

/* Skip leading space. */
for (;;)
    {
    if ((c = xpGetChar(xp)) == 0)
	xpUnexpectedEof(xp);
    if (isspace(c))
	{
	if (c == '\n')
	    ++xp->lineIx;
	}
    else
	break;
    }

/* Read end tag. */
for (;;)
    {
    dyStringAppendC(dy, c);
    if ((c = xpGetChar(xp)) == 0)
	xpUnexpectedEof(xp);
    if (isspace(c))
	{
	if (c == '\n')
	    ++xp->lineIx;
	break;
	}
    if (c == '>')
	break;
    }

/* Skip until '>' */
while (c != '>')
    {
    dyStringAppendC(dy, c);
    if ((c = xpGetChar(xp)) == 0)
	xpUnexpectedEof(xp);
    if (isspace(c))
	{
	if (c == '\n')
	    ++xp->lineIx;
	}
    else if (c != '>')
	xpError(xp, "Unexpected characters past first word in /%s tag", dy->string);
    }

if (!sameString(dy->string, tagName))
    xpError(xp, "Mismatch between start tag %s and end tag %s",  tagName, dy->string);
}

boolean xpParseNext(struct xp *xp, char *tag)
/* Skip through file until get given tag.  Then parse out the
 * tag and all of it's children (calling atStartTag/atEndTag).
 * You can call this repeatedly to process all of a given tag
 * in file. */

{
char c;
int i, attCount = 0;
struct dyString *text = NULL;
boolean isClosed;
boolean inside = (tag == NULL);
struct xpStack *initialStack = xp->stack;

for (;;)
    {
    /* Load up text until next tag. */
    for (;;)
        {
	if ((c = xpGetChar(xp)) == 0)
	    return FALSE;
	if (c == '<')
	    break;
	if (c == '&')
	   xpLookup(xp, xp->endTag, text);
	else 
	    {
	    if (c == '\n')
		++xp->lineIx;
	    if (text != NULL)
		dyStringAppendC(text, c);
	    }
	}

    /* Get next character to figure out what type of tag. */
    c = xpGetChar(xp);
    if (c == 0)
       xpError(xp, "End of file inside tag");
    else if (c == '?' || c == '!')
        xpEatComment(xp, c);
    else if (c == '/')  /* Closing tag. */
        {
	struct xpStack *stack = xp->stack;
	if (stack >= xp->stackBufEnd)
	    xpError(xp, "Extra end tag");
	xpParseEndTag(xp, stack->tag->string);
	if (inside)
	    xp->atEndTag(xp->userData, stack->tag->string, stack->text->string);
	xp->stack += 1;
	if (xp->stack == initialStack)
	    return TRUE;
	}
    else	/* Start tag. */
        {
	/* Push new frame on stack and check for overflow and unallocated strings. */
	struct xpStack *stack = --xp->stack;
	if (stack < xp->stackBuf)
	    xpError(xp, "Stack overflow");
	if (stack->tag == NULL)
	    stack->tag = newDyString(32);
	else
	    dyStringClear(stack->tag);
	if (stack->text == NULL)
	    stack->text = newDyString(256);
	else
	    dyStringClear(stack->text);
	text = stack->text;

	/* Parse the start tag. */
	xpUngetChar(xp);
	xpParseStartTag(xp, ArraySize(xp->attDyBuf), stack->tag, 
		&attCount, xp->attDyBuf, &isClosed);

	if (!inside && sameString(stack->tag->string, tag))
	    {
	    inside = TRUE;
	    initialStack = xp->stack + 1;
	    }

	/* Call user start function, and if closed tag, end function too. */
	if (inside)
	    {
	    /* Unpack attributes into simple array of strings. */
	    for (i=0; i<attCount; ++i)
		xp->attBuf[i] = xp->attDyBuf[i]->string;
	    xp->attBuf[attCount] = NULL;
	    xp->atStartTag(xp->userData, stack->tag->string, xp->attBuf);
	    }
	if (isClosed)
	    {
	    if (inside)
		xp->atEndTag(xp->userData, stack->tag->string, stack->text->string);
	    xp->stack += 1;
	    if (xp->stack == initialStack)
		return TRUE;
	    }
	}
    }
}

void xpParse(struct xp *xp)
/* Parse from start tag to end tag.  Throw error if a problem. */
{
xpParseNext(xp, NULL);
}

