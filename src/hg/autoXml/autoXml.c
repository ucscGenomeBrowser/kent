/* autoXml - Generate structures code and parser for XML file from DTD-like spec. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "obscure.h"

static char const rcsid[] = "$Id: autoXml.c,v 1.17 2005/11/20 22:20:48 kent Exp $";

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

struct element
/* An element in an XML file. */
    {
    struct element *next;	/* Next in list. */
    char *name;			/* Element Name. */
    char *mixedCaseName;	/* Name converted from EL_NAME or el-name to elName. */
    struct elChild *children;	/* Child elements. */
    struct attribute *attributes; /* Attributes. */
    int lineIx;			/* Line where element occurs. */
    char *textType;		/* Text between tags if any. */
    };

struct elChild
/* A reference to a child element. */
   {
   struct elChild *next;	/* Next in list. */
   char *name;			/* Name of element. */
   char copyCode;		/* '1', '+', '?', or '*' */
   boolean isOr;                /* Is this part of a ( n | m ) "or" list? */
   struct element *el;		/* Element definition. */
   };

struct attribute
/* An attribute of some sort. */
    {
    struct attribute *next;	/* Next in list. */
    char *name;			/* Name of attribute. */
    char *type;			/* Element type - CDATA, INT, FLOAT, etc. */
    boolean required;		/* True if required. */
    char *usual;		/* Default value (or NULL if none) */
    };

void syntaxError(struct lineFile *lf)
/* Report syntax error and exit. */
{
errAbort("Syntax error line %d of %s", lf->lineIx, lf->fileName);
}

char *needNextWord(char **pLine, struct lineFile *lf)
/* Get next word in line.  Squawk and die if can't find it. */
{
char *word = nextWord(pLine);
if (word == NULL)
    errAbort("Missing data line %d of %s", lf->lineIx, lf->fileName);
return word;
}

boolean isAllUpper(char *s)
/* Return true if all alphabetical letters in string
 * are upper case. */
{
char c;
while ((c = *s++) != 0)
    {
    if (isalpha(c) && !isupper(c))
        return FALSE;
    }
return TRUE;
}

boolean isAllLower(char *s)
/* Return true if all alphabetical letters in string
 * are upper case. */
{
char c;
while ((c = *s++) != 0)
    {
    if (isalpha(c) && !islower(c))
        return FALSE;
    }
return TRUE;
}


char *mixedCaseName(char *orig)
/* Convert var_like_this or VAR_LIKE_THIS or even
 * var-like-this to varLikeThis. */
{
char *mixed;
char *d, *s = orig;
char c;
int prefixLen = strlen(prefix), len;
boolean nextUpper;
boolean allUpper = isAllUpper(orig); 
boolean allLower = isAllLower(orig);
boolean initiallyMixed = (!allUpper && !allLower);

/* Allocate string big enough for prefix and all. */
len = strlen(orig) + prefixLen;
mixed = d = needMem(len+1);
strcpy(d, prefix);
d += prefixLen;
nextUpper = (prefixLen > 0);

for (;;)
   {
   c = *s++;
   if (c == '_' || c == '-')
       nextUpper = TRUE;
   else
       {
       if (nextUpper)
           c = toupper(c);
       else if (!initiallyMixed)
           c = tolower(c);
       nextUpper = FALSE;
       *d++ = c;
       if (c == 0)
	   break;
       }
   }
return mixed;
}

struct element *parseElement(char *line, struct hash *elHash, struct lineFile *lf)
/* Parse out <!ELEMENT line after <!ELEMENT. */
{
char *word, *s, *e;
char *words[256];
int wordCount, i;
struct elChild *ec;
struct element *el;
boolean isOr;
char orCopyCode = '?';
boolean isSmall;

word = needNextWord(&line, lf);
s = word + strlen(word)-1;
if (s[0] == '>')
   *s = 0;
if ((el = hashFindVal(elHash, word)) != NULL)
    errAbort("Duplicate element %s line %d and %d of %s", word, el->lineIx, lf->lineIx, lf->fileName);
AllocVar(el);
el->lineIx = lf->lineIx;
hashAddSaveName(elHash, word, el, &el->name);
el->mixedCaseName = mixedCaseName(el->name);
if (line != NULL && (s = strchr(line, '(')) != NULL)
    {
    s += 1;
    if ((e = strchr(line, ')')) == NULL)
        errAbort("Missing ')' line %d of %s", lf->lineIx, lf->fileName);
    *e = 0;
    isOr = (strchr(s, '|') != NULL);
    if (isOr)
      {
	orCopyCode = *(e+1);
	if ((orCopyCode != '+') && (orCopyCode != '*'))
	  orCopyCode = '?';
      }
    wordCount = chopString(s, "| ,\t", words, ArraySize(words));
    if (wordCount == ArraySize(words))
	errAbort("Too many children in list line %d of %s", lf->lineIx, lf->fileName);
    for (i=0; i<wordCount; ++i)
	{
	char *name = words[i];
	int len = strlen(name);
	char lastC = name[len-1];
	if (name[0] == '#')
	    {
	    if (isOr)
	        errAbort("# character in enumeration not allowed line %d of %s",
		   lf->lineIx, lf->fileName);
	    if (el->textType != NULL)
		errAbort("Multiple types for text between tags line %d of %s", 
			lf->lineIx, lf->fileName);
	    el->textType = cloneString(name);
	    }
	else
	    {
	    AllocVar(ec);
	    slAddHead(&el->children, ec);
	    ec->isOr = isOr;
	    if (isOr)
	       ec->copyCode = orCopyCode;
	    else
		{
		if (lastC == '+' || lastC == '?' || lastC == '*')
		    {
		    ec->copyCode = lastC;
		    name[len-1] = 0;
		    }
		else
		    ec->copyCode = '1';
		}
	    if (sameString(name, textField))
		errAbort("Name conflict with default text field name line %d of %s", lf->lineIx, lf->fileName);
	    ec->name = cloneString(name);
	    }
	}
    slReverse(&el->children);
    }
return el;
}

void parseAttribute(char *line, struct hash *elHash, struct lineFile *lf)
/* Parse out <!ATTLIST line after <!ATTLIST. */
{
char *word;
struct attribute *att;
struct element *el;
char *e;

/* Get rid of trailing '>' */
e = strrchr(line, '>');
if (e == NULL)
    errAbort("Missing '>' line %d of %s", lf->lineIx, lf->fileName);
*e = 0;

word = needNextWord(&line, lf);
if ((el = hashFindVal(elHash, word)) == NULL)
    errAbort("Undefined %s line %d of %s", word, lf->lineIx, lf->fileName);
word = needNextWord(&line, lf);
if (sameString(word, textField))
    errAbort("Name conflict with text field name line %d of %s", lf->lineIx, lf->fileName);
AllocVar(att);
att->name = cloneString(word);
word = needNextWord(&line, lf);
att->type = cloneString(word);
line = skipLeadingSpaces(line);
if (line[0] == '#')
    {
    word = needNextWord(&line, lf);
    if (sameWord("#REQUIRED", word))
        att->required = TRUE;
    else if (sameWord("#IMPLIED", word))
        att->usual = NULL;
    else
        errAbort("Unknown directive %s line %d of %s", word, lf->lineIx, lf->fileName);
    }
else if (line[0] == '\'' || line[0] == '"')
    {
    word = line;
    if (!parseQuotedString(word, word, &line))
	errAbort("Missing closing quote line %d of %s", lf->lineIx, lf->fileName);
    att->usual = cloneString(word);
    }
else
    {
    word = needNextWord(&line, lf);
    att->usual = cloneString(word);
    }
slAddTail(&el->attributes, att);
}

void fixupChildRefs(struct element *elList, struct hash *elHash, char *fileName)
/* Go through all of elements children and make sure that the corresponding
 * elements are defined. */
{
struct element *el, *child;
struct elChild *ec;
for (el = elList; el != NULL; el = el->next)
    {
    for (ec = el->children; ec != NULL; ec = ec->next)
        {
	if ((child = hashFindVal(elHash, ec->name)) == NULL)
	    errAbort("%s's child %s undefined line %d of %s", el->name, ec->name, el->lineIx, fileName);
	ec->el = child;
	}
    }
}

char *eatComment(struct lineFile *lf, char *line)
/* Eat possibly multi-line comment.  Return line past end of comment */
{
char *s;
for (;;)
    {
    if ((s = stringIn("-->", line)) != NULL)
        {
	line = skipLeadingSpaces(s+3);
	if (line[0] == 0)
	    line = NULL;
	return line;
	}
    if (!lineFileNext(lf, &line, NULL))
        return NULL;
    }
}

void parseDtdx(char *fileName, struct element **retList, struct hash **retHash)
/* Parse out a dtdx file into element list/hash. */
{
struct hash *elHash = newHash(8);
struct element *elList = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;

while (lineFileNext(lf, &line, NULL))
    {
    line = trimSpaces(line);
    if (line == NULL || line[0] == 0 || line[0] == '#')
        continue;
    if (startsWith("<!--", line))
	{
        line = eatComment(lf, line);
	if (line == NULL)
	    continue;
	}
    if (!startsWith("<!", line))
        syntaxError(lf);
    line += 2;
    word = needNextWord(&line, lf);
    if (sameWord("ELEMENT", word))
        {
	el = parseElement(line, elHash, lf);
	slAddHead(&elList, el);
	}
    else if (sameWord("ATTLIST", word))
        {
	parseAttribute(line, elHash, lf);
	}
    else
        {
	errAbort("Don't understand %s line %d of %s", word, lf->lineIx, lf->fileName);
	}
    }
lineFileClose(&lf);
slReverse(&elList);
fixupChildRefs(elList, elHash, fileName);
*retHash = elHash;
*retList = elList;
}

void dumpElement(struct element *el, FILE *f)
/* Dump info on element. */
{
struct elChild *ec;
struct attribute *att;
fprintf(f, "%s %s (", el->name, el->mixedCaseName);
for (ec = el->children; ec != NULL; ec = ec->next)
    {
    fprintf(f, "%s", ec->name);
    if (ec->copyCode != '1')
        fprintf(f, "%c", ec->copyCode);
    if (ec->isOr)
        fprintf(f, " (isOr)");
    if (ec->next != NULL)
        fprintf(f, ", ");
    }
fprintf(f, ")");
if (el->textType != NULL)
    fprintf(f, " (%s)", el->textType);
fprintf(f, "\n");
for (att = el->attributes; att != NULL; att = att->next)
    {
    fprintf(f, "  %s %s %s %s\n",
        att->name, att->type, (att->usual ? att->usual : "n/a"),  
	(att->required ? "required" : "optional"));
    }
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

void saveFunctionPrototype(struct element *el, FILE *f, char *addSemi)
/* Put up function prototype for elSave function. */
{
char *name = el->mixedCaseName;
fprintf(f, "void %sSave(struct %s *obj, int indent, FILE *f)%s", name, name, addSemi);
fprintf(f, "\n");
fprintf(f, "/* Save %s to file. */\n",  name);
}

void loadFunctionPrototype(struct element *el, FILE *f, char *addSemi)
/* Put up function prototype for elLoad function. */
{
char *name = el->mixedCaseName;
fprintf(f, "struct %s *%sLoad(char *fileName)%s\n", name, name, addSemi);
fprintf(f, "/* Load %s from file. */\n", name);
}

void loadFunctionBody(struct element *el, FILE *f)
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

void makeH(struct element *elList, char *fileName)
/* Produce C header file. */
{
FILE *f = mustOpen(fileName, "w");
struct element *el;
struct attribute *att;
struct elChild *ec;
char upcPrefix[128];

fprintf(f, "/* %s.h %s */\n", prefix, fileComment);
strcpy(upcPrefix, prefix);
touppers(upcPrefix);
fprintf(f, "#ifndef %s_H\n", upcPrefix);
fprintf(f, "#define %s_H\n", upcPrefix);
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
	fprintf(f, "    %s%s;", cAttType(att->type), att->name);
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
    saveFunctionPrototype(el, f, ";");
    fprintf(f, "\n");
    loadFunctionPrototype(el, f, ";");
    fprintf(f, "\n");
    }

fprintf(f, "#endif /* %s_H */\n", upcPrefix);
fprintf(f, "\n");
}

boolean optionalChildren(struct element *el)
/* Return TRUE if not sure whether you have children. */
{
struct elChild *ec;
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

boolean isAtomic(struct element *el)
/* Return TRUE if by definition no children. */
{
return el->textType == NULL && el->children == NULL;
}

void saveFunctionBody(struct element *el, FILE *f)
/* Write out save function body. */
{
struct elChild *ec;
struct attribute *att;
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
	    fprintf(f, "fprintf(f, \" %s=\\\"%%%c\\\"\", obj->%s);\n", att->name, fAttType(att->type), att->name);
	else
	    {
	    if (sameString(att->type, "INT") || sameString(att->type, "FLOAT"))
	        {
		if (positiveOnly)
		    {
		    fprintf(f, "if (obj->%s >= 0)\n    ", att->name);
		    }
	        fprintf(f, "fprintf(f, \" %s=\\\"%%%c\\\"\", obj->%s);\n", att->name, fAttType(att->type), att->name);
		}
	    else
		{
		fprintf(f, "if (obj->%s != NULL)\n", att->name);
		fprintf(f, "    fprintf(f, \" %s=\\\"%%%c\\\"\", obj->%s);\n", att->name, fAttType(att->type), att->name);
		}
	    }
	}
    if (isAtom)
	fprintf(f, "fprintf(f, \"->\\n\");\n");
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

boolean childMatch(struct elChild *children, struct element *el)
/* Return TRUE if any of children are el. */
{
struct elChild *ec;
for (ec = children; ec != NULL; ec = ec->next)
    {
    if (ec->el == el)
	return TRUE;
    }
return FALSE;
}

boolean anyParent(struct element *elList, struct element *child)
/* Return TRUE if anybody in elList could be a parent to child. */
{
struct element *el;
struct elChild *ec;
for (el = elList; el != NULL; el = el->next)
    {
    if (childMatch(el->children, child))
        return TRUE;
    }
return FALSE;
}

void startHandlerPrototype(FILE *f, char *addSemi)
/* Print function prototype for startHandler. */
{
fprintf(f, "void *%sStartHandler(struct xap *xp, char *name, char **atts)%s\n", prefix, addSemi);
fprintf(f, "/* Called by expat with start tag.  Does most of the parsing work. */\n");
}

void makeStartHandler(struct element *elList, FILE *f)
/* Create function that gets called by expat at start of tag. */
{
struct element *el;
struct attribute *att;

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
	        fprintf(f, "    obj->%s = %s%s%s;\n", att->name, quote, att->usual, quote);
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
		fprintf(f, "            obj->%s = atoi(val);\n", att->name);
	    else if (sameWord(att->type, "FLOAT"))
		fprintf(f, "            obj->%s = atof(val);\n", att->name);
	    else
	        fprintf(f, "            obj->%s = cloneString(val);\n", att->name);
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
		    fprintf(f, "    if (obj->%s == NULL)\n", att->name);
		    fprintf(f, "        xapError(xp, \"missing %s\");\n", att->name);
		    }
		}
	    }
	}
    if (anyParent(elList, el))
        {
	struct element *parent;
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
fprintf(f, "/* Called by expat with end tag.  Checks all required children are loaded. */\n");
}

void makeEndHandler(struct element *elList, FILE *f)
/* Create function that gets called by expat at end of tag. */
{
struct element *el;
struct elChild *ec;
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


void makeTestDriver(struct element *rootEl, FILE *f)
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
fprintf(f, "return 0;\n");
fprintf(f, "}\n");
}


void makeC(struct element *elList, char *fileName, char *incName)
/* Produce C code file. */
{
FILE *f = mustOpen(fileName, "w");
struct element *el;
struct attribute *att;
struct elChild *ec;
char upcPrefix[128];

fprintf(f, "/* %s.c %s */\n", prefix, fileComment);
fprintf(f, "\n");
fprintf(f, "#include \"common.h\"\n");
fprintf(f, "#include \"xap.h\"\n");
fprintf(f, "#include \"%s\"\n", incName);
fprintf(f, "\n");

startHandlerPrototype(f, ";");
fprintf(f, "\n");
endHandlerPrototype(f, ";");
fprintf(f, "\n");
fprintf(f, "\n");
for (el = elList; el != NULL; el = el->next)
    {
    saveFunctionPrototype(el, f, "");
    saveFunctionBody(el, f);
    loadFunctionPrototype(el, f, "");
    loadFunctionBody(el, f);
    }
makeStartHandler(elList, f);
makeEndHandler(elList, f);
if (makeMain)
   makeTestDriver(elList, f);
}

void autoXml(char *dtdxFile, char *outRoot)
/* autoXml - Generate structures code and parser for XML file from DTD-like spec. */
{
struct element *elList = NULL, *el;
struct hash *elHash = NULL;
char hName[512], cName[512];

if (cgiVarExists("prefix"))
    {
    strcpy(prefix, cgiString("prefix"));
    }
else
    splitPath(outRoot, NULL, prefix, NULL);
parseDtdx(dtdxFile, &elList, &elHash);
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
