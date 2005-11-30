/* xp - A minimal non-verifying xml parser.  It's
 * stream oriented much like expas.  It's a bit faster
 * and smaller than expas.  I'm not sure it handles unicode
 * as well.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "dystring.h"
#include "errabort.h"
#include "hash.h"
#include "xp.h"

static char const rcsid[] = "$Id: xp.c,v 1.10 2005/11/30 16:02:48 kent Exp $";


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

struct hash *makeSymHash()
/* Return hash of symbols to lookup. */
{
struct hash *symHash = newHash(6);
hashAdd(symHash, "lt", "<");
hashAdd(symHash, "gt", ">");
hashAdd(symHash, "amp", "&");
hashAdd(symHash, "apos", "'");
hashAdd(symHash, "quot", "\"");
return symHash;
}

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
xp->symHash = makeSymHash();
return xp;
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

void xpRecurseParse(struct xp *xp)
/* Parse from start tag to end tag.  Throw error if a problem.
 * Returns FALSE if nothing left to parse.  This should get
 * called after the first '<' in tag has been read. */
{
/* This routine is written to be fast rather than dense.  
 * You'd have to get pretty extreme to make it faster and
 * still keep the i/o basically a character at a time.
 * It runs about 30% faster than expat. */
char c, quotC;
int i, attCount = 0;
struct dyString *dy;

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
dy = stack->tag;
for (;;)
    {
    dyStringAppendC(dy, c);
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
	if (attCount >=  ArraySize(xp->attBuf)-2)
	    xpError(xp, "Attribute stack overflow");
	dy = xp->attDyBuf[attCount];
	if (dy == NULL)
	    dy = xp->attDyBuf[attCount] = newDyString(64);
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
	    {
	    xpError(xp, "Expecting quoted string after =");
	    }

	/* Allocate space in attribute table. */
	if (attCount >=  ArraySize(xp->attBuf)-2)
	    xpError(xp, "Attribute stack overflow");
	dy = xp->attDyBuf[attCount];
	if (dy == NULL)
	    dy = xp->attDyBuf[attCount] = newDyString(64);
	else
	    dyStringClear(dy);
	++attCount;

	/* Read until next quote. */
	quotC = c;
	i = xp->lineIx;
	for (;;)
	    {
	    if ((c = xpGetChar(xp)) == 0)
	       xpError(xp, "End of file inside literal string that started at line %d", i);
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

/* Unpack attributes into simple array of strings. */
for (i=0; i<attCount; ++i)
    xp->attBuf[i] = xp->attDyBuf[i]->string;
xp->attBuf[attCount] = NULL;

/* Call user start function. */
xp->atStartTag(xp->userData, stack->tag->string, xp->attBuf);

if (c == '/')
    {
    if ((c = xpGetChar(xp)) == 0)
       xpUnexpectedEof(xp);
    if (c != '>')
       xpError(xp, "Expecting '>' after '/'");
    }
else
    {
    /* Collect text up into next tag. */
    for (;;)
	{
	dy = stack->text;
	for (;;)
	    {
	    if ((c = xpGetChar(xp)) == 0)
	       xpError(xp, "End of file inside %s tag", stack->tag->string);
	    if (c == '&')
	       {
	       xpLookup(xp, xp->endTag, dy);
	       }
	    else
		{
		if (c == '<')
		   break;
		else if (c == '\n')
		   ++xp->lineIx;
		dyStringAppendC(dy, c);
		}
	    }

	/* Get next character to see if it's an end tag or a start tag. */
	if ((c = xpGetChar(xp)) == 0)
	   xpUnexpectedEof(xp);
	if (c == '/')
	    {
	    /* It's an end tag - we validate that it matches our start tag. */
	    dy = xp->endTag;
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

	    if (!sameString(dy->string, stack->tag->string))
		xpError(xp, "Mismatch between start tag %s and end tag %s",  stack->tag->string, dy->string);
	    break;
	    }
	else if (c == '?' || c == '!')
	    xpEatComment(xp, c);
	else
	    {
	    xpUngetChar(xp);
	    xpRecurseParse(xp);
	    }
	}
    }
/* Call user end function and pop stack. */
xp->atEndTag(xp->userData, stack->tag->string, stack->text->string);
++xp->stack;
}

boolean xpParse(struct xp *xp)
/* Parse from start tag to end tag.  Throw error if a problem.
 * Returns FALSE if nothing left to parse. */
{
char c;

/* Skip space until the first '<' */
for (;;)
    {
    if ((c = xpGetChar(xp)) == 0)
	return FALSE;
    if (isspace(c))
	{
	if (c == '\n')
	   ++xp->lineIx;
	}
    else if (c == '<')
	{
	if ((c = xpGetChar(xp)) == 0)
	    xpUnexpectedEof(xp);
	if (c == '?' || c == '!')
	    {
	    xpEatComment(xp, c);
	    }
	else if (c == '/') /* We're starting with a closing tag, treat as EOF. */
	    {
	    return FALSE;
	    }
	else
	    {
	    xpUngetChar(xp);
	    break;
	    }
	}
    else
        xpError(xp, "Text outside of tags.");
    }
xpRecurseParse(xp);
return TRUE;
}
