/* joinerCheck - Parse and check joiner file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"
#include "jksql.h"

static char const rcsid[] = "$Id: joinerCheck.c,v 1.5 2004/03/11 21:34:25 baertsch Exp $";

boolean parseOnly; 

void usage()
/* Explain usage and exit. */
{
errAbort(
  "joinerCheck - Parse and check joiner file\n"
  "usage:\n"
  "   joinerCheck file.joiner\n"
  "options:\n"
  "   -parseOnly just parse joiner file, don't check database.\n"
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
   {"parseOnly", OPTION_BOOLEAN},
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
js->lineIx = lf->lineIx;
js->fileName = cloneString(lf->fileName);

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
        else
            {
            errAbort("unrecognized '%s' line %d of %s",word, lf->lineIx, lf->fileName);
            }
	}
    }
slReverse(&jsList);
return jsList;
}

void joinerParsePassTwo(struct joinerSet *jsList)
/* Go through and link together parents and children. */
{
struct joinerSet *js, *parent;
struct hash *hash = newHash(0);
for (js = jsList; js != NULL; js = js->next)
    {
    if (hashLookup(hash, js->name))
        errAbort("Duplicate joiner %s line %d of %s",
		js->name, js->lineIx, js->fileName);
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
	    	typeOf, js->lineIx, js->fileName);
	}
    }
for (js = jsList; js != NULL; js = js->next)
    {
    for (parent = js->parent; parent != NULL; parent = parent->parent)
        {
	if (parent == js)
	    errAbort("Circular typeOf dependency on joiner %s line %d of %s", 
	    	js->name, js->lineIx, js->fileName);
	}
    }
}

struct slName *sqlListTables(struct sqlConnection *conn)
/* Return list of tables in database associated with conn. */
{
struct sqlResult *sr = sqlGetResult(conn, "show tables");
char **row;
struct slName *list = NULL, *el;
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = slNameNew(row[0]);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
slReverse(&list);
return list;
}

struct hash *sqlAllFields()
/* Get hash of all database.table.field. */
{
struct hash *fullHash = hashNew(18);
struct hash *dbHash = sqlHashOfDatabases();
struct hashEl *dbList, *db;
char fullName[512];
int count = 0;

dbList = hashElListHash(dbHash);
uglyf("%d databases\n", slCount(dbList));
for (db = dbList; db != NULL; db = db->next)
    {
    struct sqlConnection *conn = sqlConnect(db->name);
    struct slName *table, *tableList = sqlListTables(conn);
    struct sqlResult *sr;
    char query[256];
    char **row;
    uglyf("%s %d tables\n", db->name, slCount(tableList));
    for (table = tableList; table != NULL; table = table->next)
        {
	safef(query, sizeof(query), "describe %s", table->name);
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    safef(fullName, sizeof(fullName), "%s.%s.%s", 
	    	db->name, table->name, row[0]);
	    hashAdd(fullHash, fullName, NULL);
	    ++count;
	    }
	sqlFreeResult(&sr);
	}
    slFreeList(&tableList);
    sqlDisconnect(&conn);
    }
slFreeList(&dbList);
hashFree(&dbHash);
uglyf("%d total fields\n", count);
return fullHash;
}

void printField(struct joinerField *jf, FILE *f)
/* Print out field info to f. */
{
struct slName *db;
for (db = jf->dbList; db != NULL; db = db->next)
    {
    if (db != jf->dbList)
        fprintf(f, ",");
    fprintf(f, "%s", db->name);
    }
fprintf(f, ".%s.%s", jf->table, jf->field);
}

boolean fieldExists(struct hash *fieldHash,
	struct hash *dbChromHash,
	struct joinerSet *js, struct joinerField *jf)
/* Make sure field exists in at least one database. */
{
struct slName *db;

/* First try to find it as non-split in some database. */
for (db = jf->dbList; db != NULL; db = db->next)
    {
    char fieldName[512];
    safef(fieldName, sizeof(fieldName), "%s.%s.%s", 
    	db->name, jf->table, jf->field);
    if (hashLookup(fieldHash, fieldName))
        return TRUE;
    }

/* Next try to find it as split. */
for (db = jf->dbList; db != NULL; db = db->next)
    {
    struct slName *chrom, *chromList = hashFindVal(dbChromHash, db->name);
    char fieldName[512];
    for (chrom = chromList; chrom != NULL; chrom = chrom->next)
        {
	safef(fieldName, sizeof(fieldName), "%s.%s_%s.%s", 
	    db->name, chrom->name, jf->table, jf->field);
	if (hashLookup(fieldHash, fieldName))
	    return TRUE;
	}
    }

return FALSE;
}

struct hash *getDbChromHash()
/* Return hash with chromosome name list for each database
 * that has a chromInfo table. */
{
struct sqlConnection *conn = sqlConnect("mysql");
struct slName *dbList = sqlGetAllDatabase(conn), *db;
struct hash *dbHash = newHash(10);
struct slName *chromList, *chrom;
sqlDisconnect(&conn);

for (db = dbList; db != NULL; db = db->next)
     {
     conn = sqlConnect(db->name);
     if (sqlTableExists(conn, "chromInfo"))
          {
	  struct sqlResult *sr;
	  char **row;
	  struct slName *chromList = NULL, *chrom;
	  sr = sqlGetResult(conn, "select chrom from chromInfo");
	  while ((row = sqlNextRow(sr)) != NULL)
	      {
	      chrom = slNameNew(row[0]);
	      slAddHead(&chromList, chrom);
	      }
	  sqlFreeResult(&sr);
	  slReverse(&chromList);
	  hashAdd(dbHash, db->name, chromList);
	  }
     sqlDisconnect(&conn);
     }
slFreeList(&dbList);
return dbHash;
}

void joinerValidateOnDbs(struct joinerSet *jsList)
/* Make sure that joiner refers to fields that exist at
 * least somewhere. */
{
struct joinerSet *js;
struct joinerField *jf;
struct slName *db;
struct hash *fieldHash = sqlAllFields();
struct hash *dbChromHash = getDbChromHash();

for (js=jsList; js != NULL; js = js->next)
    {
    for (jf = js->fieldList; jf != NULL; jf = jf->next)
        {
	if (!fieldExists(fieldHash, dbChromHash, js, jf))
	     {
	     printField(jf, stderr);
	     fprintf(stderr, " not found in %s after line %d of %s\n",
	     	js->name, js->lineIx, js->fileName);
	     }
	}
    }
}

void joinerCheck(char *fileName)
/* joinerCheck - Parse and check joiner file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct joinerSet *js, *jsList = joinerParsePassOne(lf);
joinerParsePassTwo(jsList);
uglyf("Got %d joiners in %s\n", slCount(jsList), fileName);
if (!parseOnly)
    joinerValidateOnDbs(jsList);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
parseOnly = optionExists("parseOnly");
joinerCheck(argv[1]);
return 0;
}
