/* autoXml - Generate structures code and parser for XML file from DTD-like spec. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "obscure.h"

/* Variables that can be over-ridden from command line. */
char *prefix = "";
char *textField = "text";
char *fileComment = "autoXml generated file";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "autoXml - Generate structures code and parser for XML file from DTD-like spec\n"
  "usage:\n"
  "   autoXml file.dtdx out\n"
  "This will generate out.dtd, out.c, out.h\n"
  "options:\n"
  "   -prefix=xxx  prepend prefix to all structure elements.  Default is no prefix\n"
  "   -textField=xxx what to name field that contains text between start/end tags. Default 'text'\n"
  "   -comment=xxx Comment to appear at top of generated code files\n"
  );
}

struct element
/* An element in an XML file. */
    {
    struct element *next;	/* Next in list. */
    char *name;			/* Element Name. */
    char *mixedCaseName;	/* Name converted from EL_NAME to elName. */
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

char *mixedCaseName(char *orig)
/* Convert var_like_this or VAR_LIKE_THIS to varLikeThis. */
{
char *mixed;
char *d, *s = orig;
char c;
int prefixLen = strlen(prefix), len;
boolean nextUpper;

/* Allocate string big enough for prefix and all. */
len = strlen(orig) + prefixLen;
mixed = d = needMem(len+1);
strcpy(d, prefix);
d += prefixLen;
nextUpper = (prefixLen > 0);

for (;;)
   {
   c = *s++;
   if (c == '_')
       nextUpper = TRUE;
   else
       {
       if (nextUpper)
           c = toupper(c);
       else
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

word = needNextWord(&line, lf);
if ((el = hashFindVal(elHash, word)) != NULL)
    errAbort("Duplicate element %s line %d and %d of %s", word, el->lineIx, lf->lineIx, lf->fileName);
AllocVar(el);
el->lineIx = lf->lineIx;
hashAddSaveName(elHash, word, el, &el->name);
el->mixedCaseName = mixedCaseName(el->name);
if ((s = strchr(line, '(')) != NULL)
    {
    s += 1;
    if ((e = strchr(line, ')')) == NULL)
        errAbort("Missing ')' line %d of %s", lf->lineIx, lf->fileName);
    *e = 0;
    wordCount = chopString(s, " ,\t", words, ArraySize(words));
    if (wordCount == ArraySize(words))
        errAbort("Too many children in list line %d of %s", lf->lineIx, lf->fileName);
    for (i=0; i<wordCount; ++i)
        {
	char *name = words[i];
	int len = strlen(name);
	char lastC = name[len-1];
	if (name[0] == '#')
	    {
	    if (el->textType != NULL)
	        errAbort("Multiple types for text between tags line %d of %s", 
			lf->lineIx, lf->fileName);
	    el->textType = cloneString(name);
	    }
	else
	    {
	    AllocVar(ec);
	    slAddHead(&el->children, ec);
	    if (lastC == '+' || lastC == '?' || lastC == '*')
		{
		ec->copyCode = lastC;
		name[len-1] = 0;
		}
	    else
	        ec->copyCode = '1';
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
	    errAbort("%s's child %d undefined line %d of %s", el->name, ec->name, el->lineIx, fileName);
	ec->el = child;
	}
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

void makeH(struct element *elList, char *fileName)
/* Produce C header file. */
{
FILE *f = mustOpen(fileName, "w");
char fileOnly[128];
struct element *el;
struct attribute *att;
struct elChild *ec;

fprintf(f, "/* %s */\n", fileComment);
splitPath(fileName, NULL, fileOnly, NULL);
touppers(fileOnly);
fprintf(f, "#ifndef %s_H\n", fileOnly);
fprintf(f, "#define %s_H\n", fileOnly);
fprintf(f, "\n");

for (el = elList; el != NULL; el = el->next)
    {
    fprintf(f, "struct %s\n", el->mixedCaseName);
    fprintf(f, "    {\n");
    fprintf(f, "    struct %s *next;\n", el->mixedCaseName);
    if (el->textType != NULL)
         fprintf(f, "    char *%s;\n", textField);
    for (att = el->attributes; att != NULL; att = att->next)
	fprintf(f, "    %s%s;\n", cAttType(att->type), att->name);
    for (ec = el->children; ec != NULL; ec = ec->next)
	{
	fprintf(f, "    struct %s *%s;", ec->el->mixedCaseName, ec->el->mixedCaseName);
	if (ec->copyCode == '1')
	    fprintf(f, "\t/* Single instance required. */");
	else if (ec->copyCode == '+')
	    fprintf(f, "\t/* Non-empty list required. */");
	else if (ec->copyCode == '*')
	    fprintf(f, "\t/* Possibly empty list. */");
	else if (ec->copyCode == '?')
	    fprintf(f, "\t/* Optional (may be NULL). */");
	fprintf(f, "\n");
	}
    fprintf(f, "    };\n");
    fprintf(f, "\n");
    }

fprintf(f, "#endif /* %s_H */\n", fileOnly);
fprintf(f, "\n");
}

void autoXml(char *dtdxFile, char *outRoot)
/* autoXml - Generate structures code and parser for XML file from DTD-like spec. */
{
struct element *elList = NULL, *el;
struct hash *elHash = NULL;
char fileName[512];

parseDtdx(dtdxFile, &elList, &elHash);
for (el = elList; el != NULL; el = el->next)
    dumpElement(el, uglyOut);
sprintf(fileName, "%s.h", outRoot);
makeH(elList, fileName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
prefix = cgiUsualString("prefix", prefix);
textField = cgiUsualString("textField", textField);
fileComment = cgiUsualString("comment", fileComment);
if (argc != 3)
    usage();
autoXml(argv[1], argv[2]);
return 0;
}
