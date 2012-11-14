/* autoXml - Generate structures code and parser for XML file from DTD-like spec. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "obscure.h"
#include "portable.h"
#include "dtdParse.h"


/* Variables that can be over-ridden from command line. */
char *textField = "text";
char *fileComment = "autoXml generated file";
boolean picky;	/* Generate a parser that is afraid of the unknown. */
boolean makeMain;	/* Generate main() routine as test shell. */
boolean positiveOnly;	/* Don't write out negative numbers. */
char prefix[128];	/* Added to start of output file and structure names. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "autoXml - Generate structures code and parser for XML file from DTD-like spec\n"
  "usage:\n"
  "   autoXml file.dtdx root\n"
  "This will generate root.c, root.h\n"
  "options:\n"
  "   -textField=xxx what to name text between start/end tags. Default 'text'\n"
  "   -comment=xxx Comment to appear at top of generated code files\n"
  "   -picky  Generate parser that rejects stuff it doesn't understand\n"
  "   -main   Put in a main routine that's a test harness\n"
  "   -prefix=xxx Prefix to add to structure names. By default same as root\n"
  "   -positive Don't write out optional attributes with negative values\n"
  );
}

char *cAttType(char *type)
/* Return C declaration corresponding to type. */
{
if (sameWord(type, "INT"))
    return "int ";
else if (sameWord(type, "FLOAT"))
    return "double ";
else
    return "char *";
}

char fAttType(char *type)
/* Return printf %code to type. */
{
if (sameWord(type, "INT"))
    return 'd';
else if (sameWord(type, "FLOAT"))
    return 'f';
else
    return 's';
}

boolean childMatch(struct dtdElChild *children, struct dtdElement *el)
/* Return TRUE if any of children are el. */
{
struct dtdElChild *ec;
for (ec = children; ec != NULL; ec = ec->next)
    {
    if (ec->el == el)
	return TRUE;
    }
return FALSE;
}

boolean anyParent(struct dtdElement *elList, struct dtdElement *child)
/* Return TRUE if anybody in elList could be a parent to child. */
{
struct dtdElement *el;
for (el = elList; el != NULL; el = el->next)
    {
    if (childMatch(el->children, child))
        return TRUE;
    }
return FALSE;
}

void freeFunctionPrototype(struct dtdElement *el, FILE *f, char *addSemi)
/* Put up function prototype for elFree function. */
{
char *name = el->mixedCaseName;
fprintf(f, "void %sFree(struct %s **pObj)%s", name, name, addSemi);
fprintf(f, "\n");
fprintf(f, "/* Free up %s. */\n",  name);
}

void freeListFunctionPrototype(struct dtdElement *el, FILE *f, char *addSemi)
/* Put up function prototype for elFreeList function. */
{
char *name = el->mixedCaseName;
fprintf(f, "void %sFreeList(struct %s **pList)%s", name, name, addSemi);
fprintf(f, "\n");
fprintf(f, "/* Free up list of %s. */\n",  name);
}


void saveFunctionPrototype(struct dtdElement *el, FILE *f, char *addSemi)
/* Put up function prototype for elSave function. */
{
char *name = el->mixedCaseName;
fprintf(f, "void %sSave(struct %s *obj, int indent, FILE *f)%s", name, name, addSemi);
fprintf(f, "\n");
fprintf(f, "/* Save %s to file. */\n",  name);
}

void loadFunctionPrototype(struct dtdElement *el, FILE *f, char *addSemi)
/* Put up function prototype for elLoad function. */
{
char *name = el->mixedCaseName;
fprintf(f, "struct %s *%sLoad(char *fileName)%s\n", name, name, addSemi);
fprintf(f, "/* Load %s from XML file where it is root element. */\n", name);
}

void loadFunctionBody(struct dtdElement *el, FILE *f)
/* Write body of elLoad function. */
{
fprintf(f, "{\n");
fprintf(f, "struct %s *obj;\n", el->mixedCaseName);
fprintf(f, "xapParseAny(fileName, \"%s\", %sStartHandler, %sEndHandler, NULL, &obj);\n", 
	el->name, prefix, prefix);
fprintf(f, "return obj;\n");
fprintf(f, "}\n");
fprintf(f, "\n");
}

void loadNextFunctionPrototype(struct dtdElement *el, FILE *f, char *addSemi)
/* Put up function prototype for elLoad function. */
{
char *name = el->mixedCaseName;
fprintf(f, "struct %s *%sLoadNext(struct xap *xap)%s\n", name, name, addSemi);
fprintf(f, "/* Load next %s element.  Use xapOpen to get xap. */\n", name);
}

void loadNextFunctionBody(struct dtdElement *el, FILE *f)
/* Write body of elLoad function. */
{
fprintf(f, "{\n");
fprintf(f, "return xapNext(xap, \"%s\");\n", el->name);
fprintf(f, "}\n");
fprintf(f, "\n");
}


void startHandlerPrototype(FILE *f, char *addSemi)
/* Print function prototype for startHandler. */
{
fprintf(f, "void *%sStartHandler(struct xap *xp, char *name, char **atts)%s\n", prefix, addSemi);
fprintf(f, "/* Called by xap with start tag.  Does most of the parsing work. */\n");
}

void makeStartHandler(struct dtdElement *elList, FILE *f)
/* Create function that gets called by expat at start of tag. */
{
struct dtdElement *el;
struct dtdAttribute *att;

startHandlerPrototype(f, "");
fprintf(f, "{\n");
fprintf(f, "struct xapStack *st = xp->stack+1;\n");
fprintf(f, "int depth = xp->stackDepth;\n");
fprintf(f, "int i;\n");
fprintf(f, "\n");
for (el = elList; el != NULL; el = el->next)
    {
    fprintf(f, "%sif (sameString(name, \"%s\"))\n", (el == elList ? "" : "else "), el->name);
    fprintf(f, "    {\n");
    fprintf(f, "    struct %s *obj;\n", el->mixedCaseName);
    fprintf(f, "    AllocVar(obj);\n");
    if (el->attributes != NULL)
        {
	boolean first = TRUE;
	for (att = el->attributes; att != NULL; att = att->next)
	    {
	    if (att->usual != NULL)
		{
		char *quote = "\"";
		if (sameString(att->type, "INT") || sameString(att->type, "FLOAT"))
		    quote = "";
	        fprintf(f, "    obj->%s = %s%s%s;\n", att->mixedCaseName, quote, att->usual, quote);
		}
	    }
	fprintf(f, "    for (i=0; atts[i] != NULL; i += 2)\n");
	fprintf(f, "        {\n");
	fprintf(f, "        char *name = atts[i], *val = atts[i+1];\n");
	for (att = el->attributes; att != NULL; att = att->next)
	    {
	    fprintf(f, "        %s (sameString(name, \"%s\"))\n",
		    (first ? "if " : "else if"), att->name);
	    if (sameWord(att->type, "INT"))
		fprintf(f, "            obj->%s = atoi(val);\n", att->mixedCaseName);
	    else if (sameWord(att->type, "FLOAT"))
		fprintf(f, "            obj->%s = atof(val);\n", att->mixedCaseName);
	    else
	        fprintf(f, "            obj->%s = cloneString(val);\n", att->mixedCaseName);
	    first = FALSE;
	    }
	if (picky)
	    {
	    fprintf(f, "        else\n");
	    fprintf(f, "            xapError(xp, \"Unknown attribute %%s\", name);\n");
	    }
	fprintf(f, "        }\n");
	for (att = el->attributes; att != NULL; att = att->next)
	    {
	    if (att->required)
	        {
		if (sameWord(att->type, "INT") || sameWord(att->type, "FLOAT"))
		    {
		    /* For the moment can't check these. */
		    }
		else
		    {
		    fprintf(f, "    if (obj->%s == NULL)\n", att->mixedCaseName);
		    fprintf(f, "        xapError(xp, \"missing %s\");\n", att->name);
		    }
		}
	    }
	}
    if (anyParent(elList, el))
        {
	struct dtdElement *parent;
	boolean first = TRUE;
	fprintf(f, "    if (depth > 1)\n");
	fprintf(f, "        {\n");
	for (parent = elList; parent != NULL; parent = parent->next)
	    {
	    if (childMatch(parent->children, el))
	        {
		fprintf(f, "        %s (sameString(st->elName, \"%s\"))\n", 
			(first ? "if " : "else if"), parent->name);
		fprintf(f, "            {\n");
		fprintf(f, "            struct %s *parent = st->object;\n", parent->mixedCaseName);
		fprintf(f, "            slAddHead(&parent->%s, obj);\n", el->mixedCaseName);
		fprintf(f, "            }\n");
		first = FALSE;
		}
	    }
	if (picky)
	    {
	    fprintf(f, "        else\n");
	    fprintf(f, "            xapError(xp, \"%%s misplaced\", name);\n");
	    }
	fprintf(f, "        }\n");
	}
    fprintf(f, "    return obj;\n");
    fprintf(f, "    }\n");
    }
fprintf(f, "else\n");
fprintf(f, "    {\n");
if (picky)
    fprintf(f, "    xapError(xp, \"Unknown element %%s\", name);\n");
else
    fprintf(f, "    xapSkip(xp);\n");
fprintf(f, "    return NULL;\n");
fprintf(f, "    }\n");
fprintf(f, "}\n");
fprintf(f, "\n");
}


void endHandlerPrototype(FILE *f, char *addSemi)
/* Print function prototype for endHandler. */
{
fprintf(f, "void %sEndHandler(struct xap *xp, char *name)%s\n", prefix, addSemi);
fprintf(f, "/* Called by xap with end tag.  Checks all required children are loaded. */\n");
}

void makeEndHandler(struct dtdElement *elList, FILE *f)
/* Create function that gets called by expat at end of tag. */
{
struct dtdElement *el;
struct dtdElChild *ec;
boolean first = TRUE;

endHandlerPrototype(f, "");
fprintf(f, "{\n");
fprintf(f, "struct xapStack *stack = xp->stack;\n");
for (el = elList; el != NULL; el = el->next)
    {
    if (el->children || el->textType)
        {
	fprintf(f, "%sif (sameString(name, \"%s\"))\n", 
	   (first ? "" : "else "), el->name);
	fprintf(f, "    {\n");
	fprintf(f, "    struct %s *obj = stack->object;\n", el->mixedCaseName);
	for (ec = el->children; ec != NULL; ec = ec->next)
	    {
	    char *cBIG = ec->el->name;
	    char *cSmall = ec->el->mixedCaseName;
	    if (ec->copyCode == '1')
	        {
		fprintf(f, "    if (obj->%s == NULL)\n", cSmall);
		fprintf(f, "        xapError(xp, \"Missing %s\");\n", cBIG);
		fprintf(f, "    if (obj->%s->next != NULL)\n", cSmall);
		fprintf(f, "        xapError(xp, \"Multiple %s\");\n", cBIG);
		}
	    else if (ec->copyCode == '+')
	        {
		if (! ec->isOr)
		    {
		    /* bypassing this is not the Right thing to do -- 
		     * really we should make sure that somebody in the Or list
		     * was present */
		    fprintf(f, "    if (obj->%s == NULL)\n", cSmall);
		    fprintf(f, "        xapError(xp, \"Missing %s\");\n",cBIG);
		    }
		fprintf(f, "    slReverse(&obj->%s);\n", cSmall);
		}
	    else if (ec->copyCode == '*')
	        {
		fprintf(f, "    slReverse(&obj->%s);\n", cSmall);
		}
	    else if (ec->copyCode == '?')
	        {
		fprintf(f, "    if (obj->%s != NULL && obj->%s->next != NULL)\n", cSmall, cSmall);
		fprintf(f, "        xapError(xp, \"Multiple %s\");\n", cBIG);
		}
	    }
	if (el->textType)
	    {
	    if (sameString(el->textType, "#INT"))
		fprintf(f, "    obj->%s = atoi(stack->%s->string);\n", textField, textField);
	    else if (sameString(el->textType, "#FLOAT"))
		fprintf(f, "    obj->%s = atof(stack->%s->string);\n", textField, textField);
	    else
		fprintf(f, "    obj->%s = cloneString(stack->%s->string);\n", textField, textField);
	    }
	fprintf(f, "    }\n");
	first = FALSE;
	}
    }
fprintf(f, "}\n");
fprintf(f, "\n");
}

void makeH(struct dtdElement *elList, char *fileName)
/* Produce C header file. */
{
FILE *f = mustOpen(fileName, "w");
struct dtdElement *el;
struct dtdAttribute *att;
struct dtdElChild *ec;
char upcPrefix[128];

fprintf(f, "/* %s.h %s */\n", prefix, fileComment);
strcpy(upcPrefix, prefix);
touppers(upcPrefix);
fprintf(f, "#ifndef %s_H\n", upcPrefix);
fprintf(f, "#define %s_H\n", upcPrefix);
fprintf(f, "\n");
fprintf(f, "#ifndef XAP_H\n");
fprintf(f, "#include \"xap.h\"\n");
fprintf(f, "#endif\n");
fprintf(f, "\n");

fprintf(f, 
 "/* The start and end handlers here are used with routines defined in xap.h.\n"
 " * In particular if you want to read just parts of the XML file into memory\n"
 " * call xapOpen() with these, and then xapNext() with the name of the tag\n"
 " * you want to load. */\n\n");
startHandlerPrototype(f, ";");
fprintf(f, "\n");
endHandlerPrototype(f, ";");
fprintf(f, "\n");
fprintf(f, "\n");

for (el = elList; el != NULL; el = el->next)
    {
    fprintf(f, "struct %s\n", el->mixedCaseName);
    fprintf(f, "    {\n");
    fprintf(f, "    struct %s *next;\n", el->mixedCaseName);
    if (el->textType != NULL)
	 {
	 if (sameString(el->textType, "#INT"))
	     fprintf(f, "    int %s;\n", textField);
	 else if (sameString(el->textType, "#FLOAT"))
	     fprintf(f, "    float %s;\n", textField);
	 else
	     fprintf(f, "    char *%s;\n", textField);
	 }
    for (att = el->attributes; att != NULL; att = att->next)
	{
	fprintf(f, "    %s%s;", cAttType(att->type), att->mixedCaseName);
	if (att->required)
	    fprintf(f, "\t/* Required */");
	else 
	    {
	    if (att->usual != NULL)
	       fprintf(f, "\t/* Defaults to %s */", att->usual);
	    else
	       fprintf(f, "\t/* Optional */");
	    }
	fprintf(f, "\n");
	}
    for (ec = el->children; ec != NULL; ec = ec->next)
	{
	fprintf(f, "    struct %s *%s;", ec->el->mixedCaseName, ec->el->mixedCaseName);
	if (ec->copyCode == '1')
	    fprintf(f, "\t/** Single instance required. **/");
	else if (ec->copyCode == '+')
	    fprintf(f, "\t/** Non-empty list required. **/");
	else if (ec->copyCode == '*')
	    fprintf(f, "\t/** Possibly empty list. **/");
	else if (ec->copyCode == '?')
	    fprintf(f, "\t/** Optional (may be NULL). **/");
	fprintf(f, "\n");
	}
    fprintf(f, "    };\n");
    fprintf(f, "\n");
    freeFunctionPrototype(el, f, ";");
    fprintf(f, "\n");
    freeListFunctionPrototype(el, f, ";");
    fprintf(f, "\n");
    saveFunctionPrototype(el, f, ";");
    fprintf(f, "\n");
    loadFunctionPrototype(el, f, ";");
    fprintf(f, "\n");
    loadNextFunctionPrototype(el, f, ";");
    fprintf(f, "\n");
    }

fprintf(f, "#endif /* %s_H */\n", upcPrefix);
fprintf(f, "\n");
}

boolean optionalChildren(struct dtdElement *el)
/* Return TRUE if not sure whether you have children. */
{
struct dtdElChild *ec;
boolean required = FALSE, optional = FALSE;
for (ec = el->children; ec != NULL; ec = ec->next)
    {
    if (ec->copyCode == '*' || ec->copyCode == '?')
	optional = TRUE;
    else
        required = TRUE;
    }
return !required && optional;
}

boolean isAtomic(struct dtdElement *el)
/* Return TRUE if by definition no children. */
{
return el->textType == NULL && el->children == NULL;
}

void saveFunctionBody(struct dtdElement *el, FILE *f)
/* Write out save function body. */
{
struct dtdElChild *ec;
struct dtdAttribute *att;
boolean optKids = optionalChildren(el);
boolean isAtom = isAtomic(el);

fprintf(f, "{\n");

/* Declare variables to walk through list if need be. */
for (ec = el->children; ec != NULL; ec = ec->next)
    {
    if (ec->copyCode == '*' || ec->copyCode == '+')
	{
	char *name = ec->el->mixedCaseName;
        fprintf(f, "struct %s *%s;\n", name, name);
	}
    }
if (optKids)
    fprintf(f, "boolean isNode = TRUE;\n");

fprintf(f, "if (obj == NULL) return;\n");
fprintf(f, "xapIndent(indent, f);\n");
if (el->attributes == NULL)
    {
    if (isAtom)
	fprintf(f, "fprintf(f, \" ->\\n\");\n");
    else
	fprintf(f, "fprintf(f, \"<%s>\");\n", el->name);
    }
else 
    {
    fprintf(f, "fprintf(f, \"<%s\");\n", el->name);
    for (att = el->attributes; att != NULL; att = att->next)
        {
	if (att->required )
	    fprintf(f, "fprintf(f, \" %s=\\\"%%%c\\\"\", obj->%s);\n", att->name, fAttType(att->type), att->mixedCaseName);
	else
	    {
	    if (sameString(att->type, "INT") || sameString(att->type, "FLOAT"))
	        {
		if (positiveOnly)
		    {
		    fprintf(f, "if (obj->%s >= 0)\n    ", att->mixedCaseName);
		    }
	        fprintf(f, "fprintf(f, \" %s=\\\"%%%c\\\"\", obj->%s);\n", att->name, fAttType(att->type), att->mixedCaseName);
		}
	    else
		{
		fprintf(f, "if (obj->%s != NULL)\n", att->mixedCaseName);
		fprintf(f, "    fprintf(f, \" %s=\\\"%%%c\\\"\", obj->%s);\n", att->name, fAttType(att->type), att->mixedCaseName);
		}
	    }
	}
    if (isAtom)
	fprintf(f, "fprintf(f, \"/>\\n\");\n");
    else
	fprintf(f, "fprintf(f, \">\");\n");
    }
if (el->textType != NULL)
    {
    if (sameString(el->textType, "#INT"))
	fprintf(f, "fprintf(f, \"%%d\", obj->%s);\n", textField);
    else if (sameString(el->textType, "#FLOAT"))
	fprintf(f, "fprintf(f, \"%%f\", obj->%s);\n", textField);
    else
	fprintf(f, "fprintf(f, \"%%s\", obj->%s);\n", textField);
    }
if (isAtom)
    {
    }
else if (el->children == NULL)
    {
    fprintf(f, "fprintf(f, \"</%s>\\n\");\n", el->name);
    }
else
    {
    if (!optKids)
	fprintf(f, "fprintf(f, \"\\n\");\n");
    for (ec = el->children; ec != NULL; ec = ec->next)
	{
	char *name = ec->el->mixedCaseName;
	if (ec->copyCode == '*' || ec->copyCode == '+')
	    {
	    fprintf(f, "for (%s = obj->%s; %s != NULL; %s = %s->next)\n", name, name, name, name, name);
	    fprintf(f, "   {\n");
	    if (optKids)
		{
		fprintf(f, "   if (isNode)\n");
		fprintf(f, "       {\n");
		fprintf(f, "       fprintf(f, \"\\n\");\n");
		fprintf(f, "       isNode = FALSE;\n");
		fprintf(f, "       }\n");
		}
	    fprintf(f, "   %sSave(%s, indent+2, f);\n", name, name);
	    fprintf(f, "   }\n");
	    }
	else
	    {
	    if (optKids)
	        {
		fprintf(f, "if (obj->%s != NULL)\n", name);
		fprintf(f, "    {\n");
		fprintf(f, "    if (isNode)\n");
		fprintf(f, "       {\n");
		fprintf(f, "       fprintf(f, \"\\n\");\n");
		fprintf(f, "       isNode = FALSE;\n");
		fprintf(f, "       }\n");
		fprintf(f, "    %sSave(obj->%s, indent+2, f);\n", name, name);
		fprintf(f, "    }\n");
		}
	    else
		fprintf(f, "%sSave(obj->%s, indent+2, f);\n", name, name);
	    }
	}
    if (optKids)
        {
	fprintf(f, "if (!isNode)\n");
	fprintf(f, "    xapIndent(indent, f);\n");
	}
    else
	{
	fprintf(f, "xapIndent(indent, f);\n");
	}
    fprintf(f, "fprintf(f, \"</%s>\\n\");\n", el->name);
    }
fprintf(f, "}\n");
fprintf(f, "\n");
}

void freeFunctionBody(struct dtdElement *el, FILE *f)
/* Write out free function body. */
{
struct dtdElChild *ec;
struct dtdAttribute *att;

fprintf(f, "{\n");
fprintf(f, "struct %s *obj = *pObj;\n",  el->mixedCaseName);

/* Declare variables to walk through list if need be. */
fprintf(f, "if (obj == NULL) return;\n");
for (att = el->attributes; att != NULL; att = att->next)
    {
    if (!sameString(att->type, "INT") && !sameString(att->type, "FLOAT"))
	fprintf(f, "freeMem(obj->%s);\n", att->mixedCaseName);
    }
if (el->textType != NULL)
    {
    if (!sameString(el->textType, "#INT") && 
    	!sameString(el->textType, "#FLOAT"))
	fprintf(f, "freeMem(obj->text);\n");
    }
for (ec = el->children; ec != NULL; ec = ec->next)
    {
    char *name = ec->el->mixedCaseName;
    if (ec->copyCode == '*' || ec->copyCode == '+')
	fprintf(f, "%sFreeList(&obj->%s);\n", name, name);
    else
	fprintf(f, "%sFree(&obj->%s);\n", name, name);
    }
fprintf(f, "freez(pObj);\n");
fprintf(f, "}\n");
fprintf(f, "\n");
}

void freeListFunctionBody(struct dtdElement *el, FILE *f)
/* Write out free function body. */
{
fprintf(f, "{\n");
fprintf(f, "struct %s *el, *next;\n",  el->mixedCaseName);
fprintf(f, "for (el = *pList; el != NULL; el = next)\n");
fprintf(f, "    {\n");
fprintf(f, "    next = el->next;\n");
fprintf(f, "    %sFree(&el);\n", el->mixedCaseName);
fprintf(f, "    el = next;\n");
fprintf(f, "    }\n");
fprintf(f, "}\n");
fprintf(f, "\n");
}


void makeTestDriver(struct dtdElement *rootEl, FILE *f)
/* Make main routine. */
{
char *symName = rootEl->mixedCaseName;
fprintf(f, "int main(int argc, char *argv[])\n");
fprintf(f, "/* Test driver for %s routines */\n", prefix);
fprintf(f, "{\n");
fprintf(f, "struct %s *obj;\n", symName);
fprintf(f, "if (argc != 2)\n");
fprintf(f, "    errAbort(\"Please run again with a xml filename.\");\n");
fprintf(f, "obj = %sLoad(argv[1]);\n", symName);
fprintf(f, "%sSave(obj, 0, stdout);\n", symName);
fprintf(f, "%sFree(&obj);\n", symName);
fprintf(f, "return 0;\n");
fprintf(f, "}\n");
}


void makeC(struct dtdElement *elList, char *fileName, char *incName)
/* Produce C code file. */
{
FILE *f = mustOpen(fileName, "w");
struct dtdElement *el;
char incFile[128], incExt[64];

splitPath(incName, NULL, incFile, incExt);

fprintf(f, "/* %s.c %s */\n", prefix, fileComment);
fprintf(f, "\n");
fprintf(f, "#include \"common.h\"\n");
fprintf(f, "#include \"xap.h\"\n");
fprintf(f, "#include \"%s%s\"\n", incFile, incExt);
fprintf(f, "\n");

fprintf(f, "\n");
for (el = elList; el != NULL; el = el->next)
    {
    freeFunctionPrototype(el, f, "");
    freeFunctionBody(el, f);
    freeListFunctionPrototype(el, f, "");
    freeListFunctionBody(el, f);
    saveFunctionPrototype(el, f, "");
    saveFunctionBody(el, f);
    loadFunctionPrototype(el, f, "");
    loadFunctionBody(el, f);
    loadNextFunctionPrototype(el, f, "");
    loadNextFunctionBody(el, f);
    }
makeStartHandler(elList, f);
makeEndHandler(elList, f);
if (makeMain)
   makeTestDriver(elList, f);
}

void autoXml(char *dtdxFile, char *outRoot)
/* autoXml - Generate structures code and parser for XML file from DTD-like spec. */
{
struct dtdElement *elList = NULL;
struct hash *elHash = NULL;
char hName[512], cName[512];
char outDir[256];

splitPath(outRoot, outDir, prefix, NULL);
if (cgiVarExists("prefix"))
    strcpy(prefix, cgiString("prefix"));
if (outDir[0] != 0)
    makeDir(outDir);
dtdParse(dtdxFile, prefix, textField, &elList, &elHash);
printf("Parsed %d elements in %s\n", slCount(elList), dtdxFile);
sprintf(hName, "%s.h", outRoot);
makeH(elList, hName);
sprintf(cName, "%s.c", outRoot);
makeC(elList, cName, hName);
printf("Generated code in %s\n", cName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
textField = cgiUsualString("textField", textField);
fileComment = cgiUsualString("comment", fileComment);
picky = cgiBoolean("picky");
makeMain = cgiBoolean("main");
positiveOnly = cgiBoolean("positive");
if (argc != 3)
    usage();
autoXml(argv[1], argv[2]);
return 0;
}
