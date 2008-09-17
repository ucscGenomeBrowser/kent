/* dtdParse - parse an XML DTD file.  Actually this only
 * parses a relatively simple subset of DTD's.  It's still
 * useful for autoXml and xmlToSql. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "obscure.h"
#include "dtdParse.h"

static char const rcsid[] = "$Id: dtdParse.c,v 1.5 2008/09/17 17:56:37 kent Exp $";

static void syntaxError(struct lineFile *lf)
/* Report syntax error and exit. */
{
errAbort("Syntax error line %d of %s", lf->lineIx, lf->fileName);
}

static char *needNextWord(char **pLine, struct lineFile *lf)
/* Get next word in line.  Squawk and die if can't find it. */
{
char *word = nextWord(pLine);
if (word == NULL)
    errAbort("Missing data line %d of %s", lf->lineIx, lf->fileName);
return word;
}

void needQuotedString( char *in, char *out, struct lineFile *lf, char **retNext)
/* Grab quoted string starting at in and put it into out.  Advance retNext
 * to just past quoted string.  In and out may point to same buffer. */
{
if (!parseQuotedString(in, out, retNext))
    errAbort("Missing closing quote line %d of %s", lf->lineIx, lf->fileName);
}

static boolean isAllUpper(char *s)
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

static boolean isAllLower(char *s)
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


static char *mixedCaseName(char *prefix, char *orig)
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
   if (c == '_' || c == '-' || c == ':')
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

static struct hash *initialEntityHash()
/* Make an initial entity hash - one that is just made up
 * of our built-ins. */
{
struct hash *hash = hashNew(0);
hashAdd(hash, "INTEGER", cloneString("#INT"));
hashAdd(hash, "REAL", cloneString("#FLOAT"));
hashAdd(hash, "INT", cloneString("INT"));
hashAdd(hash, "FLOAT", cloneString("FLOAT"));
return hash;
}

static struct dtdElement *parseElement(
	char *prefix, char *textField, char *line, 
	struct hash *elHash, struct lineFile *lf)
/* Parse out <!ELEMENT line after <!ELEMENT. */
{
char *word, *s, *e;
char *words[256];
int wordCount, i;
struct dtdElChild *ec;
struct dtdElement *el;
boolean isOr;
char orCopyCode = '?';

word = needNextWord(&line, lf);
s = word + strlen(word)-1;
if (s[0] == '>')
   *s = 0;
if ((el = hashFindVal(elHash, word)) != NULL)
    errAbort("Duplicate element %s line %d and %d of %s", word, el->lineIx, lf->lineIx, lf->fileName);
AllocVar(el);
el->lineIx = lf->lineIx;
hashAddSaveName(elHash, word, el, &el->name);
el->mixedCaseName = mixedCaseName(prefix, el->name);
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

static void parseAttribute(char *line, char *textField,
	struct hash *elHash, struct lineFile *lf)
/* Parse out <!ATTLIST line after <!ATTLIST. */
{
char *word;
struct dtdAttribute *att;
struct dtdElement *el;
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
att->mixedCaseName = mixedCaseName("", att->name);
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
    needQuotedString(word, word, lf, &line);
    att->usual = cloneString(word);
    }
else
    {
    word = needNextWord(&line, lf);
    att->usual = cloneString(word);
    }
slAddTail(&el->attributes, att);
}


void parseEntity(struct hash *entityHash, struct hash *predefEntityHash,
	char *line, struct lineFile *lf)
/* Parse out an entity and add it to hash.  We'll dodge our predefined entities. */
{
char *percent = needNextWord(&line, lf);
char *name = needNextWord(&line, lf);
char *value = skipLeadingSpaces(line);
if (value[0] != '"')
    errAbort("Expecting quoted string at end of ENTITY tag line %d of %s",
    	lf->lineIx, lf->fileName);
needQuotedString(value, value, lf, &line);
if (!sameString(percent, "%"))
    errAbort("Expecting %% after ENTITY tag line %d of %s", lf->lineIx, lf->fileName);
if (hashLookup(predefEntityHash, name) == NULL)
/* We don't want to overwrite the predefined entities.  These are all
 * defined to be #PCDATA or CDATA for the benefit of non-UCSC XML tools.
 * Internally we map them to #INT/#FLOAT etc. so we can have numbers
 * as well as strings in our C structures and relational database tables. */
    {
    char *oldVal = hashFindVal(entityHash, name);
    if (oldVal != NULL)
        {
	if (!sameString(oldVal, value))
	    errAbort("Entity %s redefined line %d of %s", name, lf->lineIx, lf->fileName);
	}
    else
        {
	hashAdd(entityHash, name, cloneString(value));
	}
    }
}


static void fixupChildRefs(struct dtdElement *elList, struct hash *elHash, char *fileName)
/* Go through all of elements children and make sure that the corresponding
 * elements are defined. */
{
struct dtdElement *el, *child;
struct dtdElChild *ec;
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

static char *eatComment(struct lineFile *lf, char *line)
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

static void expandEntities(char *s, struct hash *entityHash, struct lineFile *lf,
	struct dyString *dest)
/* Copy s into dest, expanding any entity (something in format %name;) 
 * by looking it up in entity hash. */
{
char c;
while ((c = *s++) != 0)
    {
    if (c == '%' && !isspace(s[0]))
        {
	char *name = s;
	char *end = strchr(s, ';');
	char *value;
	if (end == NULL)
	    errAbort("Can't find ; after %% to close entity line %d of %s",
	    	lf->lineIx, lf->fileName);
	*end++ = 0;
	s = end;
	value = hashFindVal(entityHash, name);
	if (value == NULL)
	    errAbort("Entity %%%s; is not defined line %d of %s",
	    	name, lf->lineIx, lf->fileName);
	dyStringAppend(dest, value);
	}
    else
        dyStringAppendC(dest, c);
    }
}

static char *dtdxTag(struct lineFile *lf, struct hash *entityHash,
	struct dyString *buf)
/* Return next tag. */
{
char *line;

/* Skip until get a line that starts with '<' */
if (!lineFileNextReal(lf,  &line))
    return NULL;
line = trimSpaces(line);
if (line[0] != '<')
    errAbort("Text outside of a tag line %d of %s", lf->lineIx, lf->fileName);
dyStringClear(buf);
for (;;)
    {
    expandEntities(line, entityHash, lf, buf);
    if (buf->string[buf->stringSize-1] == '>')
         break;
    dyStringAppendC(buf, ' ');
    if (!lineFileNext(lf, &line, NULL))
        errAbort("End of file %s inside of a tag.", lf->fileName);
    line = trimSpaces(line);
    }
return buf->string;
}

void dtdParse(char *fileName, char *prefix, char *textField,
	struct dtdElement **retList, struct hash **retHash)
/* Parse out a dtd file into elements that are returned in retList,
 * and for your convenience also in retHash (which is keyed by the
 * name of the element.  Note that XML element names can include the '-'
 * character.  For this and other reasons in addition to the element
 * name as it appears in the XML tag, the element has a mixedCaseName
 * that strips '-' and '_' chars, and tries to convert the name to
 * a mixed-case convention style name.  The prefix if any will be
 * prepended to mixed-case names.  The textField is what to name
 * the field that contains the letters between tags.  By default
 * (if NULL) it is "text." */
{
struct hash *elHash = newHash(8);
struct hash *entityHash = initialEntityHash();
struct hash *predefEntityHash = initialEntityHash();
struct dtdElement *elList = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;
struct dyString *buf = dyStringNew(0);

if (prefix == NULL)
    prefix = "";
if (textField == NULL)
    textField = "text";
while ((line = dtdxTag(lf, entityHash, buf)) != NULL)
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
	el = parseElement(prefix, textField, line, elHash, lf);
	slAddHead(&elList, el);
	}
    else if (sameWord("ATTLIST", word))
        {
	parseAttribute(line, textField, elHash, lf);
	}
    else if (sameWord("ENTITY", word))
        {
	parseEntity(entityHash, predefEntityHash, line, lf);
	}
    else
        {
	errAbort("Don't understand %s line %d of %s", word, lf->lineIx, lf->fileName);
	}
    }
lineFileClose(&lf);
dyStringFree(&buf);
slReverse(&elList);
fixupChildRefs(elList, elHash, fileName);
freeHashAndVals(&entityHash);
freeHashAndVals(&predefEntityHash);
*retHash = elHash;
*retList = elList;
}

void dtdElementDump(struct dtdElement *el, FILE *f)
/* Dump info on element. */
{
struct dtdElChild *ec;
struct dtdAttribute *att;
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

