/* xap - XML Automatic Parser - together with autoXml this helps automatically
 * read in automatically generated data structures.  Calls lower level routine
 * in xp module, which originally was just a thin shell around expat. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "xap.h"
#include "errabort.h"
#include "xp.h"

static char const rcsid[] = "$Id: xap.c,v 1.10 2005/11/29 17:24:21 kent Exp $";

void xapError(struct xap *xap, char *format, ...)
/* Issue an error message and abort*/
{
va_list args;
va_start(args, format);
vaWarn(format, args);
errAbort("line %d of %s", xpLineIx(xap->xp), xap->fileName);
va_end(args);
}

static void xapStartTag(void *userData, char *name, char **atts)
/* Handle beginning of a tag. */
{
struct xap *xap = userData;
struct xapStack *stack;

stack = --xap->stack;
if (stack < xap->stackBuf)
    xapError(xap, "xap stack overflow");
++xap->stackDepth;
if (stack->text == NULL)
    stack->text = newDyString(256);
stack->elName = (char*)name;
if (xap->skipDepth == 0)
    stack->object = xap->startHandler(xap, (char*)name, (char**)atts);
if (xap->stackDepth == 1)
    {
    freeMem(xap->topType);
    xap->topType = cloneString(stack->elName);
    xap->topObject = stack->object;
    }
}

static void xapEndTag(void *userData, char *name, char *text)
/* Handle end of tag. */
{
struct xap *xap = userData;
struct xapStack *stack;

dyStringAppend(xap->stack->text, text);
if (xap->skipDepth == 0 || xap->skipDepth <= xap->stackDepth)
    {
    xap->skipDepth = 0;
    if (xap->endHandler)
	xap->endHandler(xap, (char*)name);
    }
stack = xap->stack++;
if (xap->stack > xap->endStack)
    xapError(xap, "xap stack underflow");
--xap->stackDepth;
dyStringClear(stack->text);
}

#ifdef EXPAT
static void xapText(void *userData, char *s, int len)
/* Handle some text. */
{
struct xap *xap = userData;
if (xap->skipDepth == 0)
    dyStringAppendN(xap->stack->text, (char *)s, len);
}
#endif /* EXPAT */

static int xapRead(void *userData, char *buf, int bufSize)
/* Read some text. */
{
struct xap *xap = userData;
return fread(buf, 1, bufSize, xap->f);
}

struct xap *xapNew(void *(*startHandler)(struct xap *xap, char *name, char **atts),
	void (*endHandler)(struct xap *xap, char *name) , char *fileName)
/* Create a new parse stack. */
{
struct xap *xap;
AllocVar(xap);
xap->endStack = xap->stack = xap->stackBuf + ArraySize(xap->stackBuf) - 1;
xap->startHandler = startHandler;
xap->endHandler = endHandler;
xap->xp = xpNew(xap, xapStartTag, xapEndTag, xapRead, fileName);
xap->fileName = cloneString(fileName);
return xap;
}

void xapFree(struct xap **pXp)
/* Free up a parse stack. */
{
struct xap *xap = *pXp;
if (xap != NULL)
    {
    struct xapStack *stack;
    for (stack = xap->stackBuf; stack < xap->endStack; ++stack)
        {
	if (stack->text != NULL)
	   freeDyString(&stack->text);
	}
    xpFree(&xap->xp);
    freeMem(xap->fileName);
    freeMem(xap->topType);
    freez(pXp);
    }
}

void xapParseFile(struct xap *xap, char *fileName)
/* Open up file and parse it all. */
{
xap->f = mustOpen(fileName, "r");
xpParse(xap->xp);
carefulClose(&xap->f);
}

void xapIndent(int count, FILE *f)
/* Write out some spaces. */
{
int i;
for (i=0; i<count; ++i)
    {
    fputc(' ', f);
    }
}

void xapSkip(struct xap *xap)
/* Skip current tag and any children.  Called from startHandler. */
{
xap->skipDepth = xap->stackDepth;
}

void xapParseAny(char *fileName, char *type, 
	void *(*startHandler)(struct xap *xap, char *name, char **atts),
	void (*endHandler)(struct xap *xap, char *name),
	char **retType, void *retObj)
/* Parse any object out of an XML file. 
 * If type parameter is non-NULL, force type.
 * example:
 *     xapParseAny("file.xml", "das", dasStartHandler, dasEndHandler, &type, &obj); */
{
struct xap *xap = xapNew(startHandler, endHandler, fileName);
void **pObj = retObj;
xapParseFile(xap, fileName);
if (type != NULL && !sameString(xap->topType, type))
    xapError(xap, "Got %s, expected %s\n", xap->topType, type);
if (retType != NULL)
    *retType = cloneString(xap->topType);
*pObj = xap->topObject;
xapFree(&xap);
}

static void skipThrough(FILE *f, char *fileName, char *pattern)
/* Go through file until we see pattern. */
{
int fileC, patC;
int patIx = 0;

for (;;)
    {
    patC = pattern[patIx];
    if (patC == 0)
         break;		/* We made it to end of pattern string! */
    fileC = getc(f);
    if (fileC == EOF)
        errAbort("Couldn't find %s in %s", pattern, fileName);
    if (fileC == patC)
	patIx += 1;
    else
        patIx = 0;
    }
}

struct xap *xapListOpen(char *fileName, char *outerType,
	void *(*startHandler)(struct xap *xap, char *name, char **atts),
	void (*endHandler)(struct xap *xap, char *name))
/* This handles the common case where an xml file
 * contains a list of repeated structures just inside
 * the file-level tag.  We're not wanting to load
 * the whole XML into memory necessarily.  So this
 * routine will basically skip over the opening tag.
 * You can then call xapListNext repeatedly on the
 * rest of the file.   When done, call xapFree.*/
{
struct xap *xap = xapNew(startHandler, endHandler, fileName);
FILE *f;
char tag[256];
safef(tag, sizeof(tag), "<%s>", outerType);
xap->f = f = mustOpen(fileName, "r");
skipThrough(f, fileName, tag);
return xap;
}

void *xapListNext(struct xap *xap, char *listType)
/* This returns the next item in XML file.  It returns NULL
 * at end of file (or more properly when the outer tag closes */
{
if (!xpParse(xap->xp))
    return NULL;
if (!sameString(xap->topType, listType))
    errAbort("Expecting %s tag, got %s tag", listType, xap->topType);
return xap->topObject;
}

