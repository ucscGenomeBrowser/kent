/* joiner - information about what fields in what tables
 * in what databases can fruitfully be related together
 * or joined.  Another way of looking at it is this
 * defines identifiers shared across tables. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "obscure.h"
#include "jksql.h"
#include "joiner.h"

void joinerFieldFree(struct joinerField **pJf)
/* Free up memory associated with joinerField. */
{
struct joinerField *jf = *pJf;
if (jf != NULL)
    {
    slFreeList(&jf->dbList);
    freeMem(jf->table);
    freeMem(jf->field);
    slFreeList(&jf->chopBefore);
    slFreeList(&jf->chopAfter);
    freeMem(jf->separator);
    freez(pJf);
    }
}

void joinerFieldFreeList(struct joinerField **pList)
/* Free up memory associated with list of joinerFields. */
{
struct joinerField *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    joinerFieldFree(&el);
    }
*pList = NULL;
}

void joinerSetFree(struct joinerSet **pJs)
/* Free up memory associated with joinerSet. */
{
struct joinerSet *js = *pJs;
if (js != NULL)
    {
    freeMem(js->name);
    joinerFieldFreeList(&js->fieldList);
    freeMem(js->typeOf);
    freeMem(js->external);
    freeMem(js->description);
    freez(pJs);
    }
}

void joinerSetFreeList(struct joinerSet **pList)
/* Free up memory associated with list of joinerSets. */
{
struct joinerSet *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    joinerSetFree(&el);
    }
*pList = NULL;
}

void joinerFree(struct joiner **pJoiner)
/* Free up memory associated with joiner */
{
struct joiner *joiner = *pJoiner;
if (joiner != NULL)
    {
    freeMem(joiner->fileName);
    joinerSetFreeList(&joiner->jsList);
    freeHashAndVals(&joiner->symHash);
    freez(pJoiner);
    }
}

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

static char *subThroughHash(struct lineFile *lf,
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

static char *nextSubbedLine(struct lineFile *lf, struct hash *hash, 
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

static void unspecifiedVar(struct lineFile *lf, char *var)
/* Complain about variable that needs to be followed by = but isn't */
{
errAbort("%s must be followed by = line %d of %s", var, 
	lf->lineIx, lf->fileName);
}

static char *cloneSpecified(struct lineFile *lf, char *var, char *val)
/* Make sure that val exists, and return clone of it. */
{
if (val == NULL)
    unspecifiedVar(lf, var);
return cloneString(val);
}

static struct joinerSet *parseOneJoiner(struct lineFile *lf, char *line, 
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
js->lineIx = lf->lineIx;

while ((word = nextWord(&line)) != NULL)
    {
    char *e = strchr(word, '=');
    if (e != NULL)
	*e++ = 0;
    if (sameString(word, "typeOf"))
	{
	js->typeOf = cloneSpecified(lf, word, e);
	}
    else if (sameString(word, "external"))
	{
	js->external = cloneSpecified(lf, word, e);
	}
    else if (sameString(word, "fuzzy"))
        {
	js->isFuzzy = TRUE;
	}
    else
	{
        errAbort("Unknown attribute %s line %d of %s", word, 
		lf->lineIx, lf->fileName);
	}
    }

/* Parse second line, make sure it is quoted, and save as description. */
line = nextSubbedLine(lf, symHash, dyBuf);
if (line == NULL)
    lineFileUnexpectedEnd(lf);
line = trimSpaces(line);
if (line[0] != '"' || lastChar(line) != '"')
    errAbort("Expecting quoted line, line %d of %s\n", 
    	lf->lineIx, lf->fileName);
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
     jf->lineIx = lf->lineIx;
     jf->table = cloneString(parts[1]);
     jf->field = cloneString(parts[2]);
     if (js->fieldList == NULL && !js->isFuzzy)
         jf->isPrimary = TRUE;
     jf->minCheck = 1.0;
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
	     {
	     jf->separator = cloneString(",");
	     }
	 else if (sameString("separator", word))
	     {
	     jf->separator = cloneSpecified(lf, word, e);
	     }
	 else if (sameString("chopBefore", word))
	     {
	     if (e == NULL) 
	     	unspecifiedVar(lf, word);
	     slNameStore(&jf->chopBefore, e);
	     }
	 else if (sameString("chopAfter", word))
	     {
	     if (e == NULL) 
	     	unspecifiedVar(lf, word);
	     slNameStore(&jf->chopAfter, e);
	     }
	 else if (sameString("indexOf", word))
	     {
	     jf->indexOf = TRUE;
	     }
	 else if (sameString("dupeOk", word))
	     {
	     jf->dupeOk = TRUE;
	     }
	 else if (sameString("oneToOne", word))
	     {
	     jf->oneToOne = TRUE;
	     }
	 else if (sameString("minCheck", word))
	     {
	     if (e == NULL)
	         unspecifiedVar(lf, word);
	     jf->minCheck = atof(e);
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
     if (jf->isPrimary && jf->separator != NULL)
         errAbort("Error line %d of %s\n"
	          "Primary key can't be a list (comma or separator)." 
		  , lf->lineIx, lf->fileName);
     }
slReverse(&js->fieldList);
return js;
}

static struct joinerSet *joinerParsePassOne(struct lineFile *lf, 
	struct hash *symHash)
/* Do first pass parsing of joiner file and return list of
 * joinerSets. */
{
char *line, *word;
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
	else if (sameString("identifier", word))
	    {
	    js = parseOneJoiner(lf, line, symHash, dyBuf);
	    if (js != NULL)
	        slAddHead(&jsList, js);
	    }
        else
            {
            errAbort("unrecognized '%s' line %d of %s",word, lf->lineIx, lf->fileName);
            }
	}
    }
dyStringFree(&dyBuf);
slReverse(&jsList);
return jsList;
}

static struct joinerSet *joinerExpand(struct joinerSet *jsList, char *fileName)
/* Expand joiners that have [] in them. */
{
struct joinerSet *js, *nextJs, *newJs, *newList = NULL;

for (js=jsList; js != NULL; js = nextJs)
    {
    char *startBracket, *endBracket;
    nextJs = js->next;
    if ((startBracket = strchr(js->name, '[')) != NULL)
        {
	char *dbStart,*dbEnd;
	char *dbCommaList;
	struct joinerField *jf, *newJf;
	struct dyString *dy = dyStringNew(0);
	endBracket = strchr(startBracket, ']');
	if (endBracket == NULL)
	    errAbort("[ without ] line %s of %s", js->lineIx, fileName);
	dbCommaList = cloneStringZ(startBracket+1, endBracket - startBracket - 1);
	dbStart = dbCommaList;
	while (dbStart != NULL)
	    {
	    /* Parse out comma-separated list. */
	    dbEnd = strchr(dbStart, ',');
	    if (dbEnd != NULL)
	       {
	       *dbEnd++ = 0;
	       if (dbEnd[0] == 0)
	           dbEnd = NULL;
	       }
	    if (dbStart[0] == 0)
	       errAbort("Empty element in comma separated list line %d of %s",
	       	   js->lineIx, fileName);

	    /* Make up name for new joiner. */
	    dyStringClear(dy);
	    dyStringAppendN(dy, js->name, startBracket-js->name);
	    dyStringAppend(dy, dbStart);
	    dyStringAppend(dy, endBracket+1);

	    /* Allocate new joiner and fill in most data elements. */
	    AllocVar(newJs);
	    newJs->name = cloneString(dy->string);
	    newJs->typeOf = cloneString(js->typeOf);
	    newJs->external = cloneString(js->external);
	    newJs->description = cloneString(js->description);
	    newJs->isFuzzy = js->isFuzzy;
	    newJs->lineIx = js->lineIx;

	    /* Fill in new joiner fieldList */
	    for (jf = js->fieldList; jf != NULL; jf = jf->next)
	        {
		char *bs = NULL, *be = NULL;
		/* Allocate vars and do basic fields. */
		AllocVar(newJf);
		newJf->dbList = slNameCloneList(jf->dbList);
		newJf->field = cloneString(jf->field);
		newJf->chopBefore = slNameCloneList(jf->chopBefore);
		newJf->chopAfter = slNameCloneList(jf->chopBefore);
		newJf->separator = cloneString(jf->separator);
		newJf->indexOf = jf->indexOf;
		newJf->isPrimary = jf->isPrimary;
		newJf->dupeOk = jf->dupeOk;
		newJf->oneToOne = jf->oneToOne;
		newJf->minCheck = jf->minCheck;

		/* Do substituted table field. */
		if ((bs = strchr(jf->table, '[')) != NULL)
		    be = strchr(bs, ']');
		if (bs == NULL || be == NULL)
		    errAbort("Missing [] in field '%s' line %d of %s",
		    	jf->table, jf->lineIx, fileName);
		dyStringClear(dy);
		dyStringAppendN(dy, jf->table, bs - jf->table);
		dyStringAppend(dy, dbStart);
		dyStringAppend(dy, be+1);
		newJf->table = cloneString(dy->string);

		slAddHead(&newJs->fieldList, newJf);
		}
	    newJs->expanded = TRUE;
	    slReverse(&newJs->fieldList);
	    slAddHead(&newList, newJs);

	    dbStart = dbEnd;
	    }
	dyStringFree(&dy);
	freez(&dbCommaList);
	joinerSetFree(&js);
	}
    else
        {
	slAddHead(&newList, js);
	}
    }

slReverse(&newList);
return newList;
}

static void joinerParsePassTwo(struct joinerSet *jsList, char *fileName)
/* Go through and link together parents and children. */
{
struct joinerSet *js, *parent;
struct hash *hash = newHash(0);
for (js = jsList; js != NULL; js = js->next)
    {
    if (hashLookup(hash, js->name))
        errAbort("Duplicate joiner %s line %d of %s",
		js->name, js->lineIx, fileName);
    hashAdd(hash, js->name, js);
    }
for (js = jsList; js != NULL; js = js->next)
    {
    char *typeOf = js->typeOf;
    if (typeOf != NULL)
        {
	js->parent = hashFindVal(hash, typeOf);
	if (js->parent == NULL)
	    errAbort("%s not define line %d of %s", 
	    	typeOf, js->lineIx, fileName);
	}
    }
for (js = jsList; js != NULL; js = js->next)
    {
    for (parent = js->parent; parent != NULL; parent = parent->parent)
        {
	if (parent == js)
	    errAbort("Circular typeOf dependency on joiner %s line %d of %s", 
	    	js->name, js->lineIx, fileName);
	}
    }
}

struct joiner *joinerRead(char *fileName)
/* Read in a .joiner file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *symHash = hashNew(10);
struct joinerSet *jsList = joinerParsePassOne(lf, symHash);
struct joiner *joiner;

lineFileClose(&lf);
jsList = joinerExpand(jsList, fileName);
joinerParsePassTwo(jsList, fileName);
AllocVar(joiner);
joiner->jsList = jsList;
joiner->symHash = symHash;
joiner->fileName = cloneString(fileName);
return joiner;
}

