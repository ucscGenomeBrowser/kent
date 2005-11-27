/* autoDtd - Give this a XML document to look at and it will come up with a DTD to describe it.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "xap.h"

static char const rcsid[] = "$Id: autoDtd.c,v 1.1 2005/11/27 04:57:28 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "autoDtd - Give this a XML document to look at and it will come up with a DTD\n"
  "to describe it.\n"
  "usage:\n"
  "   autoDtd in.xml out.dtd\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct type
/* Information on a type. */
    {
    struct type *next;
    char *name;		/* Name of type/field. */
    struct hash *attHash;	/* Hash of all elements keyed by name */
    struct attribute *attributes;
    struct hash *elHash;	/* Hash of all elements keyed by type->name */
    struct element *elements;
    int count;		/* Number of times we've seen this in file. */
    boolean nonInt;	/* True if text not an int. */
    boolean nonFloat;	/* True if text not a number. */
    boolean anyText;	/* True if ever has text. */
    };

struct attribute
/* Information on an attribute */
    {
    struct attribute *next;
    char *name;
    boolean isOptional;	/* True if it's not always there. */
    boolean nonInt;	/* True if not an int. */
    boolean nonFloat;	/* True if not a number. */
    boolean seenThisRound;  /* True if seen this round. */
    };

struct element
/* Information on an element */
    {
    struct element *next;
    struct type *type;	/* Element type */
    boolean isOptional;	/* True if it's optional. */
    boolean isList;	/* True if it's a list. */
    boolean seenThisRound;  /* True if seen this round. */
    };

struct hash *typeHash;	/* Keyed by struct type */
struct type *topType;	/* Highest level type */

boolean isAllInt(char *s)
/* Return true if it looks like an integer */
{
char c;
if (*s == '-')
   ++s;
while ((c = *s++) != 0)
    if (!isdigit(c))
        return FALSE;
return TRUE;
}

boolean isAllFloat(char *s)
/* Return true if it looks like an floating point */
{
char c;
if (*s == '-')
   ++s;
while ((c = *s++) != 0)
    if (!isdigit(c))
	break;
if (c == 0)
    return TRUE;
else if (c == '.')
    return isAllInt(s);
else
    return FALSE;
}


void *startHandler(struct xap *xap, char *name, char **atts)
/* Called at the start of a tag after attributes are parsed. */
{
int i;
struct type *type = hashFindVal(typeHash, name);
struct attribute *att;
struct element *el;

if (type == NULL)
    {
    AllocVar(type);
    hashAddSaveName(typeHash, name, type, &type->name);
    type->elHash = hashNew(6);
    type->attHash = hashNew(6);
    }

/* Zero out seenThisRound flags */
for (el = type->elements; el != NULL; el = el->next)
    el->seenThisRound = FALSE;
for (att = type->attributes; att != NULL; att = att->next)
    att->seenThisRound = FALSE;

for (i=0; atts[i] != NULL; i += 2)
    {
    char *name = atts[i], *val = atts[i+1];
    att = hashFindVal(type->attHash, name);
    if (att == NULL)
        {
	AllocVar(att);
	hashAddSaveName(type->attHash, name, att, &att->name);
	slAddTail(&type->attributes, att);
	if (type->count != 0)
	    att->isOptional = TRUE;
	}
    if (!att->nonInt)
	if (!isAllInt(val))
	    att->nonInt = TRUE;
    if (!att->nonFloat)
	if (!isAllFloat(val))
	    att->nonFloat = TRUE;
    att->seenThisRound = TRUE;
    }
for (att = type->attributes; att != NULL; att = att->next)
    {
    if (!att->seenThisRound)
        att->isOptional = TRUE;
    }

if (xap->stackDepth > 1)
    {
    struct xapStack *st = xap->stack+1;
    struct type *parent = st->object;
    el = hashFindVal(parent->elHash, name);
    if (el == NULL)
        {
	AllocVar(el);
	hashAdd(parent->elHash, name, el);
	el->type = type;
	slAddTail(&parent->elements, el);
	if (parent->count != 0)
	    att->isOptional = TRUE;
	}
    if (el->seenThisRound)
        el->isList = TRUE;
    el->seenThisRound = TRUE;
    }
return type;
}

void endHandler(struct xap *xap, char *name)
/* Called at end of a tag */
{
struct type *type = xap->stack->object;
char *text = skipLeadingSpaces(xap->stack->text->string);
struct element *el;
for (el = type->elements; el != NULL; el = el->next)
    {
    if (!el->seenThisRound)
        el->isOptional = TRUE;
    }
if (text[0] == 0)
    {
    type->nonFloat = type->nonInt = TRUE;
    }
else
    {
    type->anyText = TRUE;
    if (!type->nonInt)
	if (!isAllInt(text))
	    type->nonInt = TRUE;
    if (!type->nonFloat)
	if (!isAllFloat(text))
	    type->nonFloat = TRUE;
    }
topType = type;
}

void rWriteDtd(FILE *f, struct type *type, struct hash *uniqHash)
/* Recursively write out DTD. */
{
struct element *el;
struct attribute *att;
hashAdd(uniqHash, type->name, type);
fprintf(f, "<!ELEMENT %s (\n", type->name);
for (el = type->elements; el != NULL; el = el->next)
    {
    fprintf(f, "\t%s", el->type->name);
    if (el->isList)
        {
	if (el->isOptional)
	    fprintf(f, "*");
	else
	    fprintf(f, "+");
	}
    else
        {
	if (el->isOptional)
	    fprintf(f, "?");
	}
    if (el->next != NULL || type->anyText)
        fprintf(f, ",");
    fprintf(f, "\n");
    }
if (type->anyText)
    {
    fprintf(f, "\t");
    if (!type->nonInt)
        fprintf(f, "#INT");
    else if (!type->nonFloat)
        fprintf(f, "#FLOAT");
    else
        fprintf(f, "#PCDATA");
    fprintf(f, "\n");
    }
fprintf(f, ")>\n");

for (att = type->attributes; att != NULL; att = att->next)
    {
    fprintf(f, "<!ATTLIST %s %s ", type->name, att->name);
    if (!att->nonInt)
        fprintf(f, "INT");
    else if (!att->nonFloat)
        fprintf(f, "FLOAT");
    else
        fprintf(f, "CDATA");
    if (att->isOptional)
        fprintf(f, " #IMPLIED");
    else
	fprintf(f, " #REQUIRED");
    fprintf(f, ">\n");
    }
fprintf(f, "\n");

/* Now recurse if we haven't written children yet. */
for (el = type->elements; el != NULL; el = el->next)
    {
    if (!hashLookup(uniqHash, el->type->name))
        {
	rWriteDtd(f, el->type, uniqHash);
	}
    }
}

void writeDtd(char *fileName, struct type *type)
/* Write out DTD. */
{
struct hash *uniqHash = newHash(0);  /* Prevent writing dup defs for shared types. */
FILE *f = mustOpen(fileName, "w");
rWriteDtd(f, type, uniqHash);
}

void autoDtd(char *inXml, char *outDtd)
/* autoDtd - Give this a XML document to look at and it will come up with a 
 * DTD to describe it.. */
{
struct xap *xap = xapNew(startHandler, endHandler, inXml);
typeHash = newHash(0);
xapParseFile(xap, inXml);
writeDtd(outDtd, topType);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
autoDtd(argv[1], argv[2]);
return 0;
}
