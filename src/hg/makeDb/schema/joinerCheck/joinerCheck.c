/* joinerCheck - Parse and check joiner file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"

static char const rcsid[] = "$Id: joinerCheck.c,v 1.3 2004/03/11 08:21:33 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "joinerCheck - Parse and check joiner file\n"
  "usage:\n"
  "   joinerCheck file.joiner\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct joinerField
/* A field that can be joined on. */
    {
    struct joinerField *next;	/* Next in list. */
    struct slName *dbList;	/* List of possible databases. */
    char *table;		/* Associated table. */
    char *field;		/* Associated field. */
    char *chopBefore;		/* Chop before string */
    char *chopAfter;		/* Chop after string */
    char *separator;		/* Separators for lists. */
    boolean indexOf;		/* True if id is index in this list. */
    };

struct joinerSet
/* Information on a set of fields that can be joined together. */
    {
    struct joinerSet *next;		/* Next in list. */
    char *name;				/* Name of field set. */
    struct joinerSet *parent;		/* Parsed-out parent type if any. */
    char *typeOf;			/* Parent type name if any. */
    char *external;			/* External name if any. */
    char *description;			/* Short description. */
    struct joinerField *fieldList;	/* List of fields. */
    char *fileName;		/* File parsed out of for error reporting */
    int lineIx;			/* Line index of start for error reporting */
    };

static boolean nextSubTok(struct lineFile *lf,
	char **pLine, char **retTok, int *retSize)
/* Return next token for substitution purposes.  Return FALSE
 * when no more left. */
{
char *start, *s, c;

s = start = *retTok = *pLine;
c = *s;

if (c == 0)
    return FALSE;
if (isspace(c))
    {
    while (isspace(*(++s)))
	;
    }
else if (isalnum(c))
    {
    while (isalnum(*(++s)))
	;
    }
else if (c == '$')
    {
    if (s[1] == '{')
        {
	s += 1;
	for (;;)
	    {
	    c = *(++s);
	    if (c == '}')
		{
		++s;
	        break;
		}
	    if (c == 0)	/* Arguably could warn/abort here. */
		{
		errAbort("End of line in ${var} line %d of %s",
			lf->lineIx, lf->fileName);
		}
	    }
	}
    else
        {
	while (isalnum(*(++s)))
	    ;
	}
    }
else
    {
    ++s;
    }
*pLine = s;
*retSize = s - start;
return TRUE;
}

char *subThroughHash(struct lineFile *lf,
	struct hash *hash, struct dyString *dy, char *s)
/* Return string that has any variables in string-valued hash looked up. 
 * The result is put in the passed in dyString, and also returned. */
{
char *tok;
int size;
dyStringClear(dy);
while (nextSubTok(lf, &s, &tok, &size))
    {
    if (tok[0] == '$')
        {
	char tokBuf[256], *val;

	/* Extract 'var' out of '$var' or '${var}' into tokBuf*/
	tok += 1;
	size -= 1;
	if (tok[0] == '{')
	    {
	    tok += 1;
	    size -= 2;
	    }
	if (size >= sizeof(tokBuf))
	    errAbort("Variable name too long line %d of %s", 
	    	lf->lineIx, lf->fileName);
	memcpy(tokBuf, tok, size);
	tokBuf[size] = 0;

	/* Do substitution. */
	val = hashFindVal(hash, tokBuf);
	if (val == NULL)
	    errAbort("$%s not defined line %d of %s", tokBuf, 
	    	lf->lineIx, lf->fileName);
	dyStringAppend(dy, val);
	}
    else
        {
	dyStringAppendN(dy, tok, size);
	}
    }
return dy->string;
}

static struct optionSpec options[] = {
   {NULL, 0},
};

char *nextSubbedLine(struct lineFile *lf, struct hash *hash, 
	struct dyString *dy)
/* Return next line after string substitutions.  This skips comments. */
{
char *line;
for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
	return NULL;
    if (line[0] != '#')
	break;
    }
return subThroughHash(lf, hash, dy, line);
}

void unspecifiedVar(struct lineFile *lf, char *var)
/* Complain about variable that needs to be followed by = but isn't */
{
errAbort("%s must be followed by = line %d of %s", var, 
	lf->lineIx, lf->fileName);
}

struct joinerSet *parseOneJoiner(struct lineFile *lf, char *line, 
	struct hash *symHash, struct dyString *dyBuf)
/* Parse out one joiner record - keep going until blank line or
 * end of file. */
{
struct joinerSet *js;
struct joinerField *jf;
struct slName *dbName;
char *word, *s, *e;
struct hash *varHash;
char *parts[3];
int partCount;

/* Parse through first line - first word is name. */
word = nextWord(&line);
if (word == NULL || strchr(word, '=') != NULL)
    errAbort("joiner without name line %d of %s\n", lf->lineIx, lf->fileName);
AllocVar(js);
js->name = cloneString(word);

while ((word = nextWord(&line)) != NULL)
    {
    char *e = strchr(word, '=');
    if (e == NULL)
        errAbort("Expecting name=value pair line %d of %s", 
		lf->lineIx, lf->fileName);
    *e++ = 0;
    if (sameString(word, "typeOf"))
	js->typeOf = cloneString(e);
    else if (sameString(word, "external"))
	js->external = cloneString(e);
    else
        errAbort("Unknown attribute %s line %d of %s", word, 
		lf->lineIx, lf->fileName);
    }

/* Parse second line, make sure it is quoted, and save as description. */
line = nextSubbedLine(lf, symHash, dyBuf);
if (line == NULL)
    lineFileUnexpectedEnd(lf);
line = trimSpaces(line);
if (line[0] != '"' || lastChar(line) != '"')
    errAbort("Expecting quoted line, line %d of %s\n", lf->lineIx, lf->fileName);
line[strlen(line)-1] = 0;
js->description = cloneString(line+1);

/* Go through subsequent lines. */
while ((line = nextSubbedLine(lf, symHash, dyBuf)) != NULL)
     {
     /* Keep grabbing until we get a blank line. */
     line = skipLeadingSpaces(line);
     if (line[0] == 0)
         break;

     /* First word in line should be database.tabe.field. */
     word = nextWord(&line);
     partCount = chopString(word, ".", parts, ArraySize(parts));
     if (partCount != 3)
         errAbort("Expecting database.table.field line %d of %s",
	 	lf->lineIx, lf->fileName);

     /* Allocate struct and save table and field. */
     AllocVar(jf);
     jf->table = cloneString(parts[1]);
     jf->field = cloneString(parts[2]);
     slAddHead(&js->fieldList, jf);

     /* Database may be a comma-separated list.  Parse it here. */
     s = parts[0];
     while (s != NULL)
         {
	 e = strchr(s, ',');
	 if (e != NULL)
	     {
	     *e++ = 0;
	     if (e[0] == 0)
	         e = NULL;
	     }
	 if (s[0] == 0)
	     errAbort("Empty database name line %d of %s", 
	     	lf->lineIx, lf->fileName);
	 dbName = slNameNew(s);
	 slAddHead(&jf->dbList, dbName);
	 s = e;
	 }
     slReverse(&jf->dbList);

     /* Look for other fields in subsequent space-separated words. */
     while ((word = nextWord(&line)) != NULL)
         {
	 if ((e = strchr(word, '=')) != NULL)
	     *e++ = 0;
	 if (sameString("comma", word))
	     jf->separator = cloneString(",");
	 else if (sameString("separator", word))
	     {
	     if (e == NULL)
		 unspecifiedVar(lf, word);
	     jf->separator = cloneString(e);
	     }
	 else if (sameString("chopBefore", word))
	     {
	     if (e == NULL)
		 unspecifiedVar(lf, word);
	     jf->chopBefore = cloneString(e);
	     }
	 else if (sameString("chopAfter", word))
	     {
	     if (e == NULL)
		 unspecifiedVar(lf, word);
	     jf->chopAfter = cloneString(e);
	     }
	 else if (sameString("indexOf", word))
	     {
	     jf->indexOf = TRUE;
	     }
	 else
	     {
	     errAbort("Unrecognized attribute %s line %d of %s",
	     	word, lf->lineIx, lf->fileName);
	     }
	 }
     if (jf->indexOf && jf->separator == NULL)
         errAbort("indexOf without comma or separator line %d of %s",
	 	lf->lineIx, lf->fileName);
     }
slReverse(&js->fieldList);
return js;
}

struct joinerSet *joinerParsePassOne(struct lineFile *lf)
/* Do first pass parsing of joiner file and return list of
 * joinerSets. */
{
char *line, *word;
struct hash *symHash = newHash(0);
struct dyString *dyBuf = dyStringNew(0);
struct joinerSet *jsList = NULL, *js;

while ((line = nextSubbedLine(lf, symHash, dyBuf)) != NULL)
    {
    if ((word = nextWord(&line)) != NULL)
        {
	if (sameString("set", word))
	    {
	    char *var, *val;
	    var = nextWord(&line);
	    if (var == NULL)
	        errAbort("set what line %d of %s", lf->lineIx, lf->fileName);
	    val = trimSpaces(line);
	    if (val[0] == 0)
	        errAbort("Set with no value line %d of %s", 
			lf->lineIx, lf->fileName);
	    hashAdd(symHash, var, cloneString(val));
	    }
	else if (sameString("joiner", word))
	    {
	    js = parseOneJoiner(lf, line, symHash, dyBuf);
	    if (js != NULL)
	        slAddHead(&jsList, js);
	    }
	}
    }
slReverse(&jsList);
return jsList;
}

void joinerParsePassTwo(struct joinerSet *jsList, char *fileName)
/* Go through and link together parents and children. */
{
struct joinerSet *js, *parent;
struct hash *hash = newHash(0);
for (js = jsList; js != NULL; js = js->next)
    hashAdd(hash, js->name, js);
for (js = jsList; js != NULL; js = js->next)
    {
    char *typeOf = js->typeOf;
    if (typeOf != NULL)
        {
	js->parent = hashFindVal(hash, typeOf);
	if (js->parent == NULL)
	    errAbort("%s not define line %d of %s", typeOf, js->lineIx, fileName);
	}
    }
for (js = jsList; js != NULL; js = js->next)
    {
    for (parent = js->parent; parent != NULL; parent = parent->parent)
        {
	if (parent == js)
	    errAbort("Circular typeOf dependency on joiner %s", js->name);
	}
    }
}

void joinerCheck(char *fileName)
/* joinerCheck - Parse and check joiner file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct joinerSet *js, *jsList = joinerParsePassOne(lf);
joinerParsePassTwo(jsList, fileName);

uglyf("Got %d joiners in %s\n", slCount(jsList), fileName);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
joinerCheck(argv[1]);
return 0;
}
