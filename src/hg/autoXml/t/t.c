/* t - Test expat. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "t.h"
#include "errabort.h"
#include <expat.h>

void usage()
/* Explain usage and exit. */
{
errAbort(
  "t - Test expas\n"
  "usage:\n"
  "   t XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct parseStack
/* An element of parse stack. */
   {
   void *object;	/* Object being parsed. */
   char *elName;	/* Name of element.  Mostly useful for debugging. */
   };

struct xmlParser
/* A parsing stack frame. */
   {
   struct parseStack stackBuf[128];
   struct parseStack *endStack;
   struct parseStack *startStack;
   struct parseStack *stack;
   int stackDepth;
   void *(*startHandler)(struct xmlParser *xp, char *name, char **atts);
   void (*endHandler)(struct xmlParser *xp, char *name);
   XML_Parser *parser;
   char *fileName;
   };

static void xpError(struct xmlParser *xp, char *format, ...)
/* Issue a warning message. */
{
va_list args;
va_start(args, format);
vaWarn(format, args);
errAbort("line %d of %s", XML_GetCurrentLineNumber(xp->parser), xp->fileName);
va_end(args);
}

static void xmlParserStartElement(void *userData, const char *name, const char **atts)
/* Handle beginning of a tag. */
{
struct xmlParser *xp = userData;
void *object;

if (xp->stack < xp->startStack)
    xpError(xp, "xmlParser stack overflow");
xp->stack -= 1;
++xp->stackDepth;
xp->stack->elName = (char*)name;
xp->stack->object = xp->startHandler(xp, (char*)name, (char**)atts);
}

static void xmlParserEndElement(void *userData, const char *name)
/* Handle end of tag. */
{
struct xmlParser *xp = userData;

if (xp->endHandler != NULL)
    xp->endHandler(xp, (char*)name);
xp->stack += 1;
--xp->stackDepth;
if (xp->stack > xp->endStack)
    xpError(xp, "xmlParser stack underflow");
}


struct xmlParser *xmlParserNew(void *(*startHandler)(struct xmlParser *xp, char *name, char **atts),
	void (*endHandler)(struct xmlParser *xp, char *name) )
/* Create a new parse stack. */
{
struct xmlParser *xp;
AllocVar(xp);
xp->endStack = xp->stack = xp->stackBuf + ArraySize(xp->stackBuf);
xp->startStack = xp->stackBuf;
xp->startHandler = startHandler;
xp->endHandler = endHandler;
xp->parser = XML_ParserCreate(NULL);
XML_SetUserData(xp->parser, xp);
XML_SetElementHandler(xp->parser, xmlParserStartElement, xmlParserEndElement);
return xp;
}

void xmlParserFree(struct xmlParser **pXp)
/* Free up a parse stack. */
{
struct xmlParser *xp = *pXp;
if (xp != NULL)
    {
    if (xp->parser != NULL)
	XML_ParserFree(xp->parser);
    freez(pXp);
    }
}

void xmlParse(struct xmlParser *xp, char *fileName)
/* Open up file and parse it all. */
{
FILE *f = mustOpen(fileName, "r");
char buf[BUFSIZ];
int done;
xp->fileName = fileName;
do 
    {
    size_t len = fread(buf, 1, sizeof(buf), f);
    done = len < sizeof(buf);
    if (!XML_Parse(xp->parser, buf, len, done)) 
	{
	errAbort("%s at line %d of %s\n",
	      XML_ErrorString(XML_GetErrorCode(xp->parser)),
	      XML_GetCurrentLineNumber(xp->parser), fileName);
	}
    } while (!done);
}

void *testStartHandler(struct xmlParser *xp, char *name, char **atts)
/* Test handler - ultimately will be generated computationally. */
{
struct parseStack *st;
int depth = xp->stackDepth;
int i;

for (st = xp->endStack; --st >= xp->stack; )
    printf("%s ", st->elName);
printf("\n");

st = xp->stack+1;
if (sameString(name, "DASDSN"))
    {
    struct dasdsn *obj;
    AllocVar(obj);
    return obj;
    }
else if (sameString(name, "DSN"))
    {
    struct dsn *obj;
    AllocVar(obj);
    if (depth > 1)
        {
	if (sameString(st->elName, "DASDSN"))
	    {
	    struct dasdsn *parent = st->object;
	    slAddHead(&parent->dsn, obj);
	    }
	else
	    {
	    xpError(xp, "%s misplaced", name);
	    }
	}
    return obj;
    }
else if (sameString(name, "SOURCE"))
    {
    struct source *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
	char *name = atts[0], *val = atts[1];
	if (sameString(name, "id"))
	    obj->id = cloneString(val);
	else if (sameString(name, "version"))
	    obj->version = cloneString(val);
	}
    if (obj->id == NULL)
        xpError(xp, "missing id");
    if (depth > 1)
        {
	if (sameString(st->elName, "DSN"))
	    {
	    struct dsn *parent = st->object;
	    slAddHead(&parent->source, obj);
	    }
	else
	    {
	    xpError(xp, "%s misplaced", name);
	    }
	}
    return obj;
    }
else if (sameString(name, "MAPMASTER"))
    {
    struct mapmaster *obj;
    AllocVar(obj);
    if (depth > 1)
        {
	if (sameString(st->elName, "DSN"))
	    {
	    struct dsn *parent = st->object;
	    slAddHead(&parent->mapmaster, obj);
	    }
	else
	    {
	    xpError(xp, "%s misplaced", name);
	    }
        }
    return obj;
    }
else if (sameString(name, "DESCRIPTION"))
    {
    struct description *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
	char *name = atts[0], *val = atts[1];
	if (sameString(name, "href"))
	    obj->href = cloneString(val);
	}
    if (depth > 1)
        {
	if (sameString(st->elName, "DSN"))
	    {
	    struct dsn *parent = st->object;
	    slAddHead(&parent->description, obj);
	    }
	else
	    {
	    xpError(xp, "%s misplaced", name);
	    }
        }
    }
else
    {
    xpError(xp, "Unknown type %s\n", name);
    }
return NULL;
}

void testEndHandler(struct xmlParser *xp, char *name)
/* Test end handler - makes sure that all required fields are
 * there. Ultimately will be generated computationally. */
{
if (sameString(name, "DASDSN"))
    {
    struct dasdsn *obj = xp->stack->object;
    if (obj->dsn == NULL)
        xpError(xp, "Missing DSN");
    }
else if (sameString(name, "DSN"))
    {
    struct dsn *obj = xp->stack->object;
    if (obj->source == NULL)
        xpError(xp, "Missing SOURCE");
    else if (obj->source->next != NULL)
        xpError(xp, "Multiple SOURCE");
    if (obj->mapmaster == NULL)
        xpError(xp, "Missing MAPMASTER");
    else if (obj->mapmaster->next != NULL)
        xpError(xp, "Multiple MAPMASTER");
    }
else if (sameString(name, "SOURCE"))
    {
    struct source *obj = xp->stack->object;
    }
else if (sameString(name, "MAPMASTER"))
    {
    struct mapmaster *obj = xp->stack->object;
    }
else if (sameString(name, "DESCRIPTION"))
    {
    struct description *obj = xp->stack->object;
    }
}

void testExpas(char *fileName)
/* Test out parse. */
{
int depth = 0;
struct xmlParser *xp = xmlParserNew(testStartHandler, testEndHandler);
xmlParse(xp, fileName);
xmlParserFree(&xp);
}


int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
testExpas(argv[1]);
return 0;
}
