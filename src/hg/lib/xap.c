/* xap - XML Automatic Parser - together with autoXml this helps automatically
 * read in automatically generated data structures. */

#include "common.h"
#include "xap.h"
#include "errabort.h"
#include <expat.h>

void xapError(struct xap *xp, char *format, ...)
/* Issue an error message and abort*/
{
va_list args;
va_start(args, format);
vaWarn(format, args);
errAbort("line %d of %s", XML_GetCurrentLineNumber(xp->expat), xp->fileName);
va_end(args);
}

static void xapStartTag(void *userData, const char *name, const char **atts)
/* Handle beginning of a tag. */
{
struct xap *xp = userData;
void *object;
struct xapStack *stack;

stack = --xp->stack;
if (stack < xp->stackBuf)
    xapError(xp, "xap stack overflow");
++xp->stackDepth;
if (stack->text == NULL)
    stack->text = newDyString(256);
stack->elName = (char*)name;
if (xp->skipDepth == 0)
    stack->object = xp->startHandler(xp, (char*)name, (char**)atts);
if (xp->stackDepth == 1)
    {
    freeMem(xp->topType);
    xp->topType = cloneString(stack->elName);
    xp->topObject = stack->object;
    }
}

static void xapEndTag(void *userData, const char *name)
/* Handle end of tag. */
{
struct xap *xp = userData;
struct xapStack *stack;

if (xp->skipDepth == 0 || xp->skipDepth <= xp->stackDepth)
    {
    xp->skipDepth = 0;
    if (xp->endHandler)
	xp->endHandler(xp, (char*)name);
    }
stack = xp->stack++;
if (xp->stack > xp->endStack)
    xapError(xp, "xap stack underflow");
--xp->stackDepth;
dyStringClear(stack->text);
}

static void xapText(void *userData, const XML_Char *s, int len)
/* Handle some text. */
{
struct xap *xp = userData;
if (xp->skipDepth == 0)
    dyStringAppendN(xp->stack->text, (char *)s, len);
}

struct xap *xapNew(void *(*startHandler)(struct xap *xp, char *name, char **atts),
	void (*endHandler)(struct xap *xp, char *name) )
/* Create a new parse stack. */
{
struct xap *xp;
AllocVar(xp);
xp->endStack = xp->stack = xp->stackBuf + ArraySize(xp->stackBuf);
xp->startHandler = startHandler;
xp->endHandler = endHandler;
xp->expat = XML_ParserCreate(NULL);
XML_SetUserData(xp->expat, xp);
XML_SetElementHandler(xp->expat, xapStartTag, xapEndTag);
XML_SetCharacterDataHandler(xp->expat, xapText);
return xp;
}

void xapFree(struct xap **pXp)
/* Free up a parse stack. */
{
struct xap *xp = *pXp;
if (xp != NULL)
    {
    struct xapStack *stack;
    for (stack = xp->stackBuf; stack < xp->endStack; ++stack)
        {
	if (stack->text != NULL)
	   freeDyString(&stack->text);
	}
    if (xp->expat != NULL)
	XML_ParserFree(xp->expat);
    freeMem(xp->fileName);
    freeMem(xp->topType);
    freez(pXp);
    }
}

void xapParse(struct xap *xp, char *fileName)
/* Open up file and parse it all. */
{
FILE *f = mustOpen(fileName, "r");
char buf[BUFSIZ];
int done;
xp->fileName = cloneString(fileName);
do 
    {
    size_t len = fread(buf, 1, sizeof(buf), f);
    done = len < sizeof(buf);
    if (!XML_Parse(xp->expat, buf, len, done)) 
	{
	errAbort("%s at line %d of %s\n",
	      XML_ErrorString(XML_GetErrorCode(xp->expat)),
	      XML_GetCurrentLineNumber(xp->expat), fileName);
	}
    } while (!done);
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

void xapSkip(struct xap *xp)
/* Skip current tag and any children.  Called from startHandler. */
{
xp->skipDepth = xp->stackDepth;
}

void xapParseAny(char *fileName, char *type, 
	void *(*startHandler)(struct xap *xp, char *name, char **atts),
	void (*endHandler)(struct xap *xp, char *name),
	char **retType, void *retObj)
/* Parse any object out of an XML file. 
 * If type parameter is non-NULL, force type.
 * example:
 *     xapParseAny("file.xml", "das", dasStartHandler, dasEndHandler, &type, &obj); */
{
struct xap *xp = xapNew(startHandler, endHandler);
void **pObj = retObj;
xapParse(xp, fileName);
if (type != NULL && !sameString(xp->topType, type))
    xapError(xp, "Got %s, expected %s\n", xp->topType, type);
if (retType != NULL)
    *retType = cloneString(xp->topType);
*pObj = xp->topObject;
xapFree(&xp);
}

