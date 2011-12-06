/* t - Test expat. */
#include "common.h"
#include "xap.h"
#include "t.h"
#include "memalloc.h"


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

void *testStartHandler(struct xap *xp, char *name, char **atts)
/* Test handler - ultimately will be generated computationally. */
{
struct xapStack *st;
int depth = xp->stackDepth;
int i;

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
	    xapError(xp, "%s misplaced", name);
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
	char *name = atts[i], *val = atts[i+1];
	if (sameString(name, "id"))
	    obj->id = cloneString(val);
	else if (sameString(name, "version"))
	    obj->version = cloneString(val);
	}
    if (obj->id == NULL)
        xapError(xp, "missing id");
    if (depth > 1)
        {
	if (sameString(st->elName, "DSN"))
	    {
	    struct dsn *parent = st->object;
	    slAddHead(&parent->source, obj);
	    }
	else
	    {
	    xapError(xp, "%s misplaced", name);
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
	    xapError(xp, "%s misplaced", name);
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
	char *name = atts[i], *val = atts[i+1];
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
	    xapError(xp, "%s misplaced", name);
	    }
        }
    return obj;
    }
else
    {
    xapSkip(xp);
    return NULL;
    }
}

void testEndHandler(struct xap *xp, char *name)
/* Test end handler - makes sure that all required fields are
 * there. Ultimately will be generated computationally. */
{
struct xapStack *stack = xp->stack;
if (sameString(name, "DASDSN"))
    {
    struct dasdsn *obj = stack->object;
    if (obj->dsn == NULL)
        xapError(xp, "Missing DSN");
    slReverse(&obj->dsn);
    }
else if (sameString(name, "DSN"))
    {
    struct dsn *obj = stack->object;
    if (obj->source == NULL)
        xapError(xp, "Missing SOURCE");
    else if (obj->source->next != NULL)
        xapError(xp, "Multiple SOURCE");
    if (obj->mapmaster == NULL)
        xapError(xp, "Missing MAPMASTER");
    else if (obj->mapmaster->next != NULL)
        xapError(xp, "Multiple MAPMASTER");
    }
else if (sameString(name, "SOURCE"))
    {
    struct source *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "MAPMASTER"))
    {
    struct mapmaster *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "DESCRIPTION"))
    {
    struct description *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
}

void sourceSave(struct source *obj, int indent, FILE *f)
/* Save a source. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<SOURCE");
fprintf(f, " id=\"%s\"", obj->id);
if (obj->version != NULL)
    fprintf(f, " version=\"%s\"", obj->version);
fprintf(f, ">%s</SOURCE>\n", obj->text);
}

struct source *sourceLoad(FILE *f)
/* Load source from open file. */
{
}

void descriptionSave(struct description *obj, int indent, FILE *f)
/* Save a description. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<DESCRIPTION");
if (obj->href != NULL)
    fprintf(f, " href=\"%s\"", obj->href);
fprintf(f, ">%s</DESCRIPTION>\n", obj->text);
}

void mapmasterSave(struct mapmaster *obj, int indent, FILE *f)
/* Save a mapmaster. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<MAPMASTER");
fprintf(f, ">%s</MAPMASTER>\n", obj->text);
}


void dsnSave(struct dsn *obj, int indent, FILE *f)
/* Save a das. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<DSN");
fprintf(f, ">\n");
sourceSave(obj->source, indent+2, f);
descriptionSave(obj->description, indent+2, f);
mapmasterSave(obj->mapmaster, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</DSN>\n");
}

void dasdsnSave(struct dasdsn *obj, int indent, FILE *f)
/* Save a dasdsn. */
{
struct dsn *dsn;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<DASDSN");
fprintf(f, ">\n");
for (dsn=obj->dsn; dsn != NULL; dsn = dsn->next)
   dsnSave(dsn, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</DASDSN>\n");
}

void testExpas(char *fileName)
/* Test out parse. */
{
int depth = 0;
struct xap *xp = xapNew(testStartHandler, testEndHandler);
xapParse(xp, fileName);
dasdsnSave(xp->topObject, 0, stdout);
xapFree(&xp);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
pushCarefulMemHandler(1000000);
testExpas(argv[1]);
carefulCheckHeap();
return 0;
}
