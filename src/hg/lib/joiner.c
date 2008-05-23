/* joiner - information about what fields in what tables
 * in what databases can fruitfully be related together
 * or joined.  Another way of looking at it is this
 * defines identifiers shared across tables.  This also
 * defines what tables depend on what other tables
 * through dependency attributes and statements.
 *
 * The main routines you'll want to use here are 
 *    joinerRead - to read in a joiner file
 *    joinerRelate - to get list of possible joins given a table.
 */


#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "obscure.h"
#include "jksql.h"
#include "joiner.h"

static void joinerFieldFree(struct joinerField **pJf)
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
    freeMem(jf->splitPrefix);
    slFreeList(&jf->exclude);
    freez(pJf);
    }
}

static void joinerFieldFreeList(struct joinerField **pList)
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

static void joinerSetFree(struct joinerSet **pJs)
/* Free up memory associated with joinerSet. */
{
struct joinerSet *js = *pJs;
if (js != NULL)
    {
    freeMem(js->name);
    joinerFieldFreeList(&js->fieldList);
    freeMem(js->typeOf);
    slFreeList(&js->children);
    freeMem(js->external);
    freeMem(js->description);
    freez(pJs);
    }
}

static void joinerSetFreeList(struct joinerSet **pList)
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

static void joinerTableFree(struct joinerTable **pTable)
/* Free up memory associated with joinerTable. */
{
struct joinerTable *table = *pTable;
if (table != NULL)
    {
    slFreeList(&table->dbList);
    freeMem(table->table);
    freez(pTable);
    }
}

static void joinerTableFreeList(struct joinerTable **pList)
/* Free up memory associated with list of joinerTables. */
{
struct joinerTable *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    joinerTableFree(&el);
    }
*pList = NULL;
}

static void joinerDependencyFree(struct joinerDependency **pDep)
/* Free up memory associated with joinerDependency. */
{
struct joinerDependency *dep = *pDep;
if (dep != NULL)
    {
    joinerTableFree(&dep->table);
    joinerTableFreeList(&dep->dependsOnList);
    freez(pDep);
    }
}

static void joinerDependencyFreeList(struct joinerDependency **pList)
/* Free up memory associated with list of joinerDependencys. */
{
struct joinerDependency *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    joinerDependencyFree(&el);
    }
*pList = NULL;
}

static void joinerIgnoreFree(struct joinerIgnore **pIg)
/* Free up memory associated with joinerIgnore. */
{
struct joinerIgnore *ig = *pIg;
if (ig != NULL)
    {
    slFreeList(&ig->dbList);
    slFreeList(&ig->tableList);
    freez(pIg);
    }
}

static void joinerIgnoreFreeList(struct joinerIgnore **pList)
/* Free up memory associated with list of joinerIgnores. */
{
struct joinerIgnore *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    joinerIgnoreFree(&el);
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
    hashFreeList(&joiner->exclusiveSets);
    freeHash(&joiner->databasesChecked);
    freeHash(&joiner->databasesIgnored);
    joinerDependencyFreeList(&joiner->dependencyList);
    joinerIgnoreFreeList(&joiner->tablesIgnored);
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
/* Return next line after string substitutions.  This removes comments too. */
{
char *line, *s;
for (;;)
    {
    if (!lineFileNext(lf, &line, NULL))
	return NULL;
    s = strchr(line, '#');
    if (s == NULL)	/* No sharp, it's a real line. */
	break;
    else
        {
	if (skipLeadingSpaces(line) != s)  
	    {
	    *s = 0;	/* Terminate line at sharp. */
	    break;
	    }
	/* Eat line if starts with sharp */
	}
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

static struct hash *makeNotHash(struct slName *list)
/* Return hash of elements in list if any that are 
 * starting with '!' */
{
struct slName *el;
struct hash *hash = NULL;
for (el = list; el != NULL; el = el->next)
    {
    if (el->name[0] == '!')
        {
	if (hash == NULL)
	    hash = hashNew(8);
	hashAdd(hash, el->name+1, NULL);
	}
    }
return hash;
}

static struct slName *parseDatabaseList(struct lineFile *lf, char *s)
/* Parse out comma-separated list of databases, with 
 * possible !db's. */
{
struct slName *list, *el;
struct hash *notHash;

/* Get comma-separated list. */
list = slNameListFromComma(s);
if (list == NULL)
     errAbort("Empty database name line %d of %s", 
	lf->lineIx, lf->fileName);

/* Remove !'s */
notHash = makeNotHash(list);
if (notHash != NULL)
    {
    struct slName *newList = NULL, *next;
    for (el = list; el != NULL; el = next)
        {
	next = el->next;
	if (el->name[0] != '!' && !hashLookup(notHash, el->name))
	    {
	    slAddHead(&newList, el);
	    }
	else
	    {
	    freeMem(el);
	    }
	}
    hashFree(&notHash);
    slReverse(&newList);
    list = newList;
    }
return list;
}

static struct joinerSet *parseIdentifierSet(struct lineFile *lf, 
	char *line, struct hash *symHash, struct dyString *dyBuf)
/* Parse out one joiner record - keep going until blank line or
 * end of file. */
{
struct joinerSet *js;
struct joinerField *jf;
char *word, *e;
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
    else if (sameString(word, "dependency"))
        {
	js->isDependency = TRUE;
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
	 {
         jf->isPrimary = TRUE;
	 jf->unique = TRUE;
	 jf->full = TRUE;
	 }
     jf->minCheck = 1.0;
     slAddHead(&js->fieldList, jf);

     /* Database may be a comma-separated list.  Parse it here. */
     jf->dbList = parseDatabaseList(lf, parts[0]);

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
	     if (!jf->isPrimary)
	         warn("dupeOk outsite primary key line %d of %s",
		 	lf->lineIx, lf->fileName);
	     jf->unique = FALSE;
	     }
	 else if (sameString("minCheck", word))
	     {
	     if (e == NULL)
	         unspecifiedVar(lf, word);
	     jf->minCheck = atof(e);
	     }
	 else if (sameString("unique", word))
	     {
	     jf->unique = TRUE;
	     }
	 else if (sameString("full", word))
	     {
	     jf->full = TRUE;
	     }
	 else if (sameString("splitPrefix", word))
	     {
	     jf->splitPrefix = cloneSpecified(lf, word, e);
	     }
	 else if (sameString("splitSuffix", word))
	     {
	     jf->splitSuffix = cloneSpecified(lf, word, e);
	     }
	 else if (sameString("exclude", word))
	     {
	     if (e == NULL) 
	     	unspecifiedVar(lf, word);
	     slNameStore(&jf->exclude, e);
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

static struct joinerIgnore *parseTablesIgnored(struct lineFile *lf, 
	char *line, struct hash *symHash, struct dyString *dyBuf)
/* Parse out one tables ignored record - keep going until blank line or
 * end of file. */
{
struct joinerIgnore *ig;
struct slName *table;

AllocVar(ig);
ig->dbList = parseDatabaseList(lf, trimSpaces(line));
while ((line = nextSubbedLine(lf, symHash, dyBuf)) != NULL)
    {
    /* Keep grabbing until we get a blank line. */
    line = skipLeadingSpaces(line);
    if (line[0] == 0)
        break;
    table = slNameNew(trimSpaces(line));
    slAddHead(&ig->tableList, table);
    }
slReverse(&ig->tableList);
return ig;
}

static struct joinerTable *parseTableSpec(struct lineFile *lf, char *spec)
/* Parse out table from spec.  Spec is form db1,db2...,dbN.table. */
{
struct joinerTable *table;
char *dotPos = strchr(spec, '.');
if (dotPos == NULL)
    errAbort("Need . in table specification line %d of %s", 
    	lf->lineIx, lf->fileName);
AllocVar(table);
*dotPos++ = 0;
table->table = cloneString(dotPos);
table->dbList = parseDatabaseList(lf, spec);
if (table->dbList == NULL)
    errAbort("Need at least one database before . in table spec line %d of %s",
    	lf->lineIx, lf->fileName);
return table;
}

static void dependencySyntaxErr(struct lineFile *lf)
/* Explain dependency syntax and exit. */
{
errAbort("Expecting at least two table specifiers after dependency line %d of %s",
    lf->lineIx, lf->fileName);
}

static struct joinerDependency *parseDependency(struct lineFile *lf, char *line)
/* Parse out dependency - just a space separated list of table specs, with
 * the first one having a special meaning. */
{
struct joinerDependency *dep;
struct joinerTable *table;
int count = 0;
char *word;
AllocVar(dep);
dep->lineIx = lf->lineIx;
word = nextWord(&line);
if (word == NULL)
    dependencySyntaxErr(lf);
dep->table = parseTableSpec(lf, word);
while ((word = nextWord(&line)) != NULL)
    {
    table = parseTableSpec(lf, word);
    slAddHead(&dep->dependsOnList, table);
    ++count;
    }
if (count < 1)
    dependencySyntaxErr(lf);
slReverse(&dep->dependsOnList);
return dep;
}

static struct joinerType *parseType(struct lineFile *lf, 
	char *line, struct hash *symHash, struct dyString *dyBuf)
/* Parse one type record. */
{
struct joinerType *type;
AllocVar(type);
type->name = cloneString(trimSpaces(line));
while ((line = nextSubbedLine(lf, symHash, dyBuf)) != NULL)
    {
    struct joinerTable *table;
    line = trimSpaces(line);
    if (line[0] == 0)
	break;
    table = parseTableSpec(lf, line);
    slAddHead(&type->tableList, table);
    }
slReverse(&type->tableList);
return type;
}

static void addDatabasesToHash(struct hash *hash, char *s, struct lineFile *lf)
/* Add contents of comma-separated list to hash. */
{
struct slName *el, *list = parseDatabaseList(lf, trimSpaces(s));
for (el = list; el != NULL; el = el->next)
    hashAdd(hash, el->name, NULL);
slFreeList(&list);
}

static struct joiner *joinerParsePassOne(char *fileName)
/* Do first pass parsing of joiner file and return list of
 * joinerSets. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;
struct dyString *dyBuf = dyStringNew(0);
struct joiner *joiner;
struct joinerSet *js;

AllocVar(joiner);
joiner->fileName = cloneString(fileName);
joiner->symHash = newHash(9);
joiner->databasesChecked = newHash(8);
joiner->databasesIgnored = newHash(8);
while ((line = nextSubbedLine(lf, joiner->symHash, dyBuf)) != NULL)
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
	    if ((val == NULL) || (val[0] == 0))
	        errAbort("Set with no value line %d of %s", 
			lf->lineIx, lf->fileName);
	    hashAdd(joiner->symHash, var, cloneString(val));
	    }
	else if (sameString("identifier", word))
	    {
	    js = parseIdentifierSet(lf, line, joiner->symHash, dyBuf);
	    if (js != NULL)
	        slAddHead(&joiner->jsList, js);
	    }
	else if (sameString("exclusiveSet", word))
	    {
	    struct hash *exHash = newHash(8);
	    addDatabasesToHash(exHash, line, lf);
	    slAddHead(&joiner->exclusiveSets, exHash);
	    }
	else if (sameString("databasesChecked", word))
	    {
	    addDatabasesToHash(joiner->databasesChecked, line, lf);
	    }
	else if (sameString("databasesIgnored", word))
	    {
	    addDatabasesToHash(joiner->databasesIgnored, line, lf);
	    }
	else if (sameString("tablesIgnored", word))
	    {
	    struct joinerIgnore *ig;
	    ig = parseTablesIgnored(lf, line, joiner->symHash, dyBuf);
	    slAddHead(&joiner->tablesIgnored, ig);
	    }
	else if (sameString("dependency", word))
	    {
	    struct joinerDependency *dep;
	    dep = parseDependency(lf, line);
	    slAddHead(&joiner->dependencyList, dep);
	    }
	else if (sameString("type", word))
	    {
	    struct joinerType *type;
	    type = parseType(lf, line, joiner->symHash, dyBuf);
	    slAddHead(&joiner->typeList, type);
	    }
        else
            {
            errAbort("unrecognized '%s' line %d of %s",
	    	word, lf->lineIx, lf->fileName);
            }
	}
    }
lineFileClose(&lf);
dyStringFree(&dyBuf);
slReverse(&joiner->jsList);
slReverse(&joiner->tablesIgnored);
slReverse(&joiner->typeList);
return joiner;
}

static void joinerExpand(struct joiner *joiner)
/* Expand joiners that have [] in them. */
{
struct joinerSet *js, *nextJs, *newJs, *newList = NULL;

for (js=joiner->jsList; js != NULL; js = nextJs)
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
	    errAbort("[ without ] line %d of %s", js->lineIx, joiner->fileName);
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
	       	   js->lineIx, joiner->fileName);

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
	    newJs->isDependency = js->isDependency;

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
		newJf->unique = jf->unique;
		newJf->full = jf->full;
		newJf->minCheck = jf->minCheck;
		newJf->splitPrefix = cloneString(jf->splitPrefix);
		newJf->exclude = slNameCloneList(jf->exclude);

		/* Do substituted table field. */
		if ((bs = strchr(jf->table, '[')) != NULL)
		    be = strchr(bs, ']');
		if (bs == NULL || be == NULL)
		    errAbort("Missing [] in field '%s' line %d of %s",
		    	jf->table, jf->lineIx, joiner->fileName);
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
joiner->jsList = newList;
}

static void joinerLinkParents(struct joiner *joiner)
/* Go through and link together parents and children. */
{
struct joinerSet *js, *parent;
struct hash *hash = newHash(0);
for (js = joiner->jsList; js != NULL; js = js->next)
    {
    if (hashLookup(hash, js->name))
        errAbort("Duplicate joiner %s line %d of %s",
		js->name, js->lineIx, joiner->fileName);
    hashAdd(hash, js->name, js);
    }
for (js = joiner->jsList; js != NULL; js = js->next)
    {
    char *typeOf = js->typeOf;
    if (typeOf != NULL)
        {
	js->parent = hashFindVal(hash, typeOf);
	if (js->parent == NULL)
	    errAbort("%s not define line %d of %s", 
	    	typeOf, js->lineIx, joiner->fileName);
	refAdd(&js->parent->children, js);
	}
    }
for (js = joiner->jsList; js != NULL; js = js->next)
    {
    for (parent = js->parent; parent != NULL; parent = parent->parent)
        {
	if (parent == js)
	    errAbort("Circular typeOf dependency on joiner %s line %d of %s", 
	    	js->name, js->lineIx, joiner->fileName);
	}
    slReverse(&js->children);
    }
}

static void checkIgnoreBalance(struct joiner *joiner)
/* Check that databases in fields are in the checked list.
 * Check that there is no overlap between checked and ignored
 * list. */
{
struct joinerSet *js;
struct joinerField *jf;
struct slName *db;
struct hashEl *ignoreList, *ignore;

/* Check that there is no overlap between databases ignored
 * and databases checked. */
ignoreList = hashElListHash(joiner->databasesIgnored);
for (ignore=ignoreList; ignore != NULL; ignore = ignore->next)
    {
    if (hashLookup(joiner->databasesChecked, ignore->name))
        errAbort("%s is in both databasesChecked and databasesIgnored",
		ignore->name);
    }
slFreeList(&ignoreList);

/* Check that all databases mentioned in fields are in 
 * databasesChecked. */
for (js = joiner->jsList; js != NULL; js = js->next)
    {
    for (jf = js->fieldList; jf != NULL; jf = jf->next)
        {
	for (db = jf->dbList; db != NULL; db = db->next)
	    {
	    if (!hashLookup(joiner->databasesChecked, db->name))
	        {
		errAbort("database %s line %d of %s is not in databasesChecked",
			db->name, jf->lineIx, joiner->fileName);
		}
	    }
	}
    }
}

static struct joinerTable *tableFromField(struct joinerField *jf)
/* Extract database/table info out of field. */
{
struct joinerTable *table;
AllocVar(table);
table->table = cloneString(jf->table);
table->dbList = slNameCloneList(jf->dbList);
return table;
}

static void addDependenciesFromIdentifiers(struct joiner *joiner)
/* Add dependencies that are specified through dependency 
 * attribute on identifier line. */
{
struct joinerSet *js;

for (js = joiner->jsList; js != NULL; js = js->next)
    {
    if (js->isDependency)
        {
	struct joinerField *primary = js->fieldList, *jf;
	for (jf = primary->next; jf != NULL; jf = jf->next)
	    {
	    struct joinerDependency *dep;
	    AllocVar(dep);
	    dep->lineIx = jf->lineIx;
	    dep->table = tableFromField(jf);
	    dep->dependsOnList = tableFromField(primary);
	    slAddHead(&joiner->dependencyList, dep);
	    }
	}
    }
}

struct joiner *joinerRead(char *fileName)
/* Read in a .joiner file. */
{
struct joiner *joiner = joinerParsePassOne(fileName);
joinerExpand(joiner);
joinerLinkParents(joiner);
checkIgnoreBalance(joiner);
addDependenciesFromIdentifiers(joiner);
slReverse(&joiner->dependencyList);
return joiner;
}

struct joinerDtf *joinerDtfNew(char *database, char *table, char *field)
/* Create new joinerDtf. */
{
struct joinerDtf *dtf;
AllocVar(dtf);
dtf->database = cloneString(database);
dtf->table = cloneString(table);
dtf->field = cloneString(field);
return dtf;
}

struct joinerDtf *joinerDtfClone(struct joinerDtf *dtf)
/* Return duplicate (deep copy) of joinerDtf. */
{
return joinerDtfNew(dtf->database, dtf->table, dtf->field);
}

static void notTriple(char *s)
/* Complain that s is not in dotted triple format. */
{
errAbort("%s not a dotted triple", s);
}

struct joinerDtf *joinerDtfFromDottedTriple(char *triple)
/* Get joinerDtf from something in db.table.field format. */
{
char *s, *e;
struct joinerDtf *dtf;
AllocVar(dtf);
s = triple;
e = strchr(s, '.');
if (e == NULL)
   notTriple(triple);
dtf->database = cloneStringZ(s, e-s);
s = e+1;
e = strchr(s, '.');
if (e == NULL)
   notTriple(triple);
dtf->table = cloneStringZ(s, e-s);
dtf->field = cloneString(e+1);
return dtf;
}

void joinerDtfFree(struct joinerDtf **pDtf)
/* Free up resources associated with joinerDtf. */
{
struct joinerDtf *dtf = *pDtf;
if (dtf != NULL)
    {
    freeMem(dtf->database);
    freeMem(dtf->table);
    freeMem(dtf->field);
    freez(pDtf);
    }
}

void joinerDtfFreeList(struct joinerDtf **pList)
/* Free up memory associated with list of joinerDtfs. */
{
struct joinerDtf *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    joinerDtfFree(&el);
    }
*pList = NULL;
}

void joinerPairFree(struct joinerPair **pJp)
/* Free up memory associated with joiner pair. */
{
struct joinerPair *jp = *pJp;
if (jp != NULL)
    {
    joinerDtfFree(&jp->a);
    joinerDtfFree(&jp->b);
    freez(pJp);
    }
}

void joinerPairFreeList(struct joinerPair **pList)
/* Free up memory associated with list of joinerPairs. */
{
struct joinerPair *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    joinerPairFree(&el);
    }
*pList = NULL;
}

void joinerPairDump(struct joinerPair *jpList, FILE *out)
/* Write out joiner pair list to file mostly for debugging. */
{
struct joinerPair *jp;
for (jp = jpList; jp != NULL; jp = jp->next)
    fprintf(out, "%s.%s.%s (via %s) %s.%s.%s\n",
    	jp->a->database, jp->a->table, jp->a->field,
	jp->identifier->name,
    	jp->b->database, jp->b->table, jp->b->field);
}

static struct joinerField *joinerSetIncludesTable(struct joinerSet *js, 
	char *database, char *table)
/* If joiner set includes database and table, return the associated field. */
{
struct joinerField *jf;
for (jf = js->fieldList; jf != NULL; jf = jf->next)
    {
    if (sameString(table, jf->table) && slNameInList(jf->dbList, database))
        return jf;
    }
return NULL;
}

static boolean tableExists(char *database, char *table, char *splitPrefix)
/* Return TRUE if database and table exist.  If splitPrefix is given,
 * check for existence with and without it. */
{
struct sqlConnection *conn = sqlMayConnect(database);
boolean exists;
char t2[1024];
if (conn == NULL)
    return FALSE;
if (isNotEmpty(splitPrefix))
    safef(t2, sizeof(t2), "%s%s", splitPrefix, table);
else
    safef(t2, sizeof(t2), "%s", table);
exists = sqlTableWildExists(conn, t2);
if (!exists && isNotEmpty(splitPrefix))
    {
    safef(t2, sizeof(t2), "%s", table);
    exists = sqlTableExists(conn, t2);
    }
sqlDisconnect(&conn);
return exists;
}

static void addChildren(struct joinerSet *js,  struct slRef **pList)
/* Recursively add children to list. */
{
struct slRef *childRef;
for (childRef = js->children; childRef != NULL; childRef = childRef->next)
    {
    struct joinerSet *child = childRef->val;
    refAdd(pList, child);
    addChildren(child, pList);
    }
}

struct slRef *joinerSetInheritanceChain(struct joinerSet *js)
/* Return list of self, children, and parents (but not siblings).
 * slFreeList result when done. */
{
struct slRef *list = NULL;
struct joinerSet *parent;

/* Add self and parents. */
for (parent = js; parent != NULL; parent = parent->parent)
    refAdd(&list, parent);
addChildren(js, &list);
slReverse(&list);
return list;
}

static struct joinerPair *joinerToField(
	char *aDatabase, struct joinerField *aJf,
	char *bDatabase, struct joinerField *bJf,
	struct joinerSet *identifier)
/* Construct joiner pair linking from a to b. */
{
struct joinerPair *jp;
AllocVar(jp);
jp->a = joinerDtfNew(aDatabase, aJf->table, aJf->field);
jp->b = joinerDtfNew(bDatabase, bJf->table, bJf->field);
jp->identifier = identifier;
return jp;
}


boolean joinerExclusiveCheck(struct joiner *joiner, char *aDatabase, 
	char *bDatabase)
/* Check that aDatabase and bDatabase are not in the same
 * exclusivity hash.  Return TRUE if join can happen between
 * these two databases. */
{
struct hash *exHash;
if (sameString(aDatabase, bDatabase))
    return TRUE;
for (exHash = joiner->exclusiveSets; exHash != NULL; exHash = exHash->next)
    {
    if (hashLookup(exHash, aDatabase) && hashLookup(exHash, bDatabase))
        return FALSE;
    }
return TRUE;
}

struct joinerPair *joinerRelate(struct joiner *joiner, char *database, 
	char *table)
/* Get list of all ways to link table in given database to other tables,
 * possibly in other databases. */
{
struct joinerSet *js, *jsChain;
struct joinerField *jf, *jfBase;
struct joinerPair *jpList = NULL, *jp;
struct slRef *chainList, *chainEl;
/* Return list of self, children, and parents (but not siblings) */

#ifdef SCREWS_UP_SPLITS
if (!tableExists(database, table, NULL))
    errAbort("%s.%s - table doesn't exist", database, table);
#endif

for (js = joiner->jsList; js != NULL; js = js->next)
    {
    if ((jfBase = joinerSetIncludesTable(js, database, table)) != NULL)
        {
	chainList = joinerSetInheritanceChain(js);
	for (chainEl = chainList; chainEl != NULL; chainEl = chainEl->next)
	    {
	    jsChain = chainEl->val;
	    for (jf = jsChain->fieldList; jf != NULL; jf = jf->next)
		{
		struct slName *db;
		for (db = jf->dbList; db != NULL; db = db->next)
		    {
		    if (joinerExclusiveCheck(joiner, database, db->name))
			{
			if (!sameString(database, db->name) 
				|| !sameString(table, jf->table))
			    {
			    if (tableExists(db->name, jf->table,
					    jf->splitPrefix))
				{
				jp = joinerToField(database, jfBase, 
					db->name, jf, jsChain); 
				slAddHead(&jpList, jp);
				}
			    }
			}
		    }
		}
	    }
	slFreeList(&chainList);
	}
    }
slReverse(&jpList);
return jpList;
}

static struct joinerPair *joinerPairClone(struct joinerPair *jp)
/* Return duplicate (deep copy) of joinerPair. */
{
struct joinerPair *dupe;
AllocVar(dupe);
dupe->a = joinerDtfClone(jp->a);
dupe->b = joinerDtfClone(jp->b);
dupe->identifier = jp->identifier;
return dupe;
}

boolean joinerDtfSameTable(struct joinerDtf *a, struct joinerDtf *b)
/* Return TRUE if they are in the same database and table. */
{
return sameString(a->database, b->database) && sameString(a->table, b->table);
}

static boolean identifierInArray(struct joinerSet *identifier, 
	struct joinerSet **array, int count)
/* Return TRUE if identifier is in array. */
{
int i;
for (i=0; i<count; ++i)
    {
    if (identifier == array[i])
	{
        return TRUE;
	}
    }
return FALSE;
}

static struct joinerSet *identifierStack[4];	/* Help keep search from looping. */

static struct joinerPair *rFindRoute(struct joiner *joiner, 
	struct joinerDtf *a,  struct joinerDtf *b, int level, int maxLevel)
/* Find a path that connects the two fields if possible.  Do limited
 * recursion. */
{
struct joinerPair *jpList, *jp;
struct joinerPair *path = NULL;
jpList = joinerRelate(joiner, a->database, a->table);
for (jp = jpList; jp != NULL; jp = jp->next)
    {
    if (joinerDtfSameTable(jp->b, b))
	{
	path = joinerPairClone(jp);
	break;
	}
    }
if (path == NULL && level < maxLevel)
    {
    for (jp = jpList; jp != NULL; jp = jp->next)
        {
	identifierStack[level] = jp->identifier;
	if (!identifierInArray(jp->identifier, identifierStack, level))
	    {
	    identifierStack[level] = jp->identifier;
	    path = rFindRoute(joiner, jp->b, b, level+1, maxLevel);
	    if (path != NULL)
		{
		struct joinerPair *jpClone = joinerPairClone(jp);
		slAddHead(&path, jpClone);
		break;
		}
	    }
	}
    }
joinerPairFreeList(&jpList);
return path;
}

struct joinerPair *joinerFindRoute(struct joiner *joiner, 
	struct joinerDtf *a,  struct joinerDtf *b)
/* Find route between a and b.  Note the field element of a and b
 * are unused. */
{
int i;
struct joinerPair *jpList = NULL;
for (i=1; i<ArraySize(identifierStack); ++i)
    {
    jpList = rFindRoute(joiner, a, b, 0, i);
    if (jpList != NULL)
        break;
    }
return jpList;
}


boolean joinerDtfAllSameTable(struct joinerDtf *fieldList)
/* Return TRUE if all joinerPairs refer to same table. */
{
struct joinerDtf *first = fieldList, *field;
if (first == NULL)
    return TRUE;
for (field = first->next; field != NULL; field = field->next)
    if (!joinerDtfSameTable(first, field))
        return FALSE;
return TRUE;
}

static boolean inRoute(struct joinerPair *routeList, struct joinerDtf *dtf)
/* Return TRUE if table in dtf is already in route. */
{
struct joinerPair *jp;
for (jp = routeList; jp != NULL; jp = jp->next)
    {
    if (joinerDtfSameTable(dtf, jp->a) || joinerDtfSameTable(dtf, jp->b))
        return TRUE;
    }
return FALSE;
}

static void joinerPairRemoveDupes(struct joinerPair **pRouteList)
/* Remove duplicate entries in route list. */
{
struct hash *uniqHash = newHash(0);
struct joinerPair *newList = NULL, *jp, *next;
char buf[256];

for (jp = *pRouteList; jp != NULL; jp = next)
    {
    next = jp->next;
    safef(buf, sizeof(buf), "%s.%s->%s.%s", 
    	jp->a->database, jp->a->table, 
    	jp->b->database, jp->b->table);
    if (hashLookup(uniqHash, buf))
	{
        joinerPairFree(&jp);
	}
    else
        {
	hashAdd(uniqHash, buf, NULL);
	slAddHead(&newList, jp);
	}
    }
slReverse(&newList);
*pRouteList = newList;
}

struct joinerPair *joinerFindRouteThroughAll(struct joiner *joiner, 
	struct joinerDtf *tableList)
/* Return route that gets to all tables in fieldList.  Note that
 * the field element of the items in tableList can be NULL. */
{
struct joinerPair *fullRoute = NULL, *pairRoute = NULL;
struct joinerDtf *first = tableList, *dtf;

if (first->next == NULL)
    return NULL;
for (dtf = first->next; dtf != NULL; dtf = dtf->next)
    {
    if (!inRoute(fullRoute, dtf) && !joinerDtfSameTable(first, dtf))
	{
	pairRoute = joinerFindRoute(joiner, first, dtf);
	fullRoute = slCat(fullRoute, pairRoute);
	}
    }
joinerPairRemoveDupes(&fullRoute);
return fullRoute;
}

