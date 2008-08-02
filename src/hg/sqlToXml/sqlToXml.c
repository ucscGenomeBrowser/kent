/* sqlToXml - Given a database, .as file, .joiner file, and a sql select 
 * statement, dump out results as XML. */
/* This file is copyright 2005 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"

static char const rcsid[] = "$Id: sqlToXml.c,v 1.14.94.1 2008/08/02 04:06:34 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sqlToXml - dump out all or part of a relational database to XML, guided\n"
  "by a dump specification.  See sqlToXml.doc for additional information.\n"
  "usage:\n"
  "   sqlToXml database dumpSpec.od output.xml\n"
  "options:\n"
  "   -topTag=name - Give the top level XML tag the given name.  By\n"
  "               default it will be the same as the database name.\n"
  "   -query=file.sql - Instead of dumping whole database, just dump those\n"
  "                  records matching SQL select statement in file.sql.\n"
  "                  This statement should be of the form:\n"
  "           select * from table where ...\n"
  "                   or\n"
  "           select table.* from table,otherTables where ...\n"
  "                   Where the table is the same as the table in the first\n"
  "                   line of dumpSpec.\n"
  "   -tab=N - number of spaces betweeen tabs in xml.dumpSpec - by default it's 8.\n"
  "            (It may be best just to avoid tabs in that file though.)\n"
  "   -maxList=N - This will limit any lists in the output to no more than\n"
  "                size N.  This is mostly just for testing.\n"
  );
}

int tabSize = 8;
int maxList = 0;
char *topTag = NULL;

static struct optionSpec options[] = {
   {"topTag", OPTION_STRING},
   {"query", OPTION_STRING},
   {"tab", OPTION_INT},
   {"maxList", OPTION_INT},
   {NULL, 0},
};

struct dyString *escaper;

struct specTree 
/* Holds dump specification tree. */
   {
   struct specTree *next;	/* Sibling */
   struct specTree *children;
   char *field;		/* Field in table to link on. 
                         * At top level is master table name instead.  */
   int fieldIx;		/* Index of field in table. */
   char *target;	/* Target table.field  */
   char *targetTable;	/* Table part of target */
   char *targetField;	/* Field part of target */
   char *outName;	/* Name to use for output, defaults to table name. */
   bool needsQuote;	/* Does join need value in quotes? */
   bool hideTable;	/* If true hide table (but not children) */
   bool hideField;	/* If true hide field. */
   bool textField;	/* IF true this is text field. */
   bool allInAttributes; /* If true all info is in attributes. */
   bool gotChildTags;    /* If true has child tags. */
   };

#ifdef DEBUG
static void dumpSpecTree(FILE *f, struct specTree *tree, int level)
{
struct specTree *child;
spaceOut(f, level+2);
fprintf(f, "f %s, fIx %d, target %s, hideTable %d, hideField %d, textField %d, allInI %d\n",
	tree->field, tree->fieldIx, tree->target, tree->hideTable, tree->hideField, 
	tree->textField, tree->allInAttributes);
for (child = tree->children; child != NULL; child = child->next)
    dumpSpecTree(f, child, level+1);
} 
#endif /* DEBUG */

struct specTree *specTreeTarget(struct specTree *parent, char *field)
/* Look through all children for matching field. */
{
struct specTree *tree;
for (tree = parent->children; tree != NULL; tree = tree->next)
    if (sameString(tree->field, field))
	break;
return tree;
}

struct typedField
/* A field - name and type */
    {
    struct typedField *next;
    char *name;	/* Field name. */
    char type;  /* Value is " or space. */
    };

int countIndent(char *s, int size)
/* Count the number of spaces and tabs. */
{
int count = 0;
int i;
char c;
for (i=0; i<size; ++i)
    {
    c = s[i];
    if (c == ' ')
        count += 1;
    else if (c == '\t')
        {
	int nextTab = (count + tabSize)/tabSize * tabSize;
	count = nextTab;
	}
    else
        internalErr();
    }
return count;
}

struct typedField *findField(struct typedField *list, char *name)
/* Return field of given name, or NULL if none. */
{
struct typedField *field;
for (field = list; field != NULL; field = field->next)
    if (sameString(field->name, name))
        break;
return field;
}


boolean isStringField(struct hash *tableHash, char *tableDotField)
/* Return true if given field is a string type field, needing quotations
 * in sql. */
{
char *dot = strchr(tableDotField, '.');
if (dot != NULL)
    {
    struct typedField *field, *fieldList;
    *dot = 0;
    fieldList = hashMustFindVal(tableHash, tableDotField);
    field = findField(fieldList, dot+1);
    *dot = '.';
    if (field == NULL)
        errAbort("No field %s", tableDotField);
    return field->type == '"';
    }
return FALSE;
}

void rSpecTreeLoad(struct lineFile *lf, 
	struct specTree *parent, struct hash *tableHash)
/* Load all children of parent. */
{
char *line, *pastSpace;
int indent, curIndent;
struct specTree *branch = NULL;
struct typedField *tf, *tfList = hashMustFindVal(tableHash, parent->targetTable);
if (!lineFileNextReal(lf, &line))
    return;
pastSpace = skipLeadingSpaces(line);
curIndent = countIndent(line, pastSpace - line);
lineFileReuse(lf);
for (;;)
    {
    if (!lineFileNextReal(lf, &line))
        break;
    pastSpace = skipLeadingSpaces(line);
    indent = countIndent(line, pastSpace - line);
    if (indent > curIndent)
	{
	lineFileReuse(lf);
	rSpecTreeLoad(lf, branch, tableHash);
	}
    else if (indent < curIndent)
	{
	lineFileReuse(lf);
	break;
	}
    else
	{
	char *field = nextWord(&pastSpace);
	char *target = nextWord(&pastSpace);
	char *extra = nextWord(&pastSpace);
	char *dot, targetTable[256], *targetField;
	if (target == NULL)
	   errAbort("Missing target line %d of %s", lf->lineIx, lf->fileName);
	AllocVar(branch);
	branch->allInAttributes = TRUE;
	if (field[0] != '.')
	    errAbort("Field must start with . line %d of %s", 
	    	lf->lineIx, lf->fileName);
	field += 1;
	branch->field = cloneString(field);
	branch->target = cloneString(target);
	dot = strchr(target, '.');
	if (dot != NULL)
	    {
	    /* Parse out tableName and fieldName. */
	    targetField = dot+1;
	    *dot = 0;
	    safef(targetTable, sizeof(targetTable), "%s", target);
	    *dot = '.';
	    branch->targetTable = cloneString(targetTable);
	    branch->targetField = cloneString(targetField);

	    /* Get info on current field. */
	    tf = findField(tfList, field);
	    if (tf == NULL)
		errAbort("Field %s.%s doesn't exists",  parent->targetTable, field);
	    branch->needsQuote = (tf->type == '"');
	    branch->fieldIx = slIxFromElement(tfList, tf);

	    /* See if it is followed by 'hide' */
	    if (extra != NULL && sameString(extra, "hide"))
	        {
		extra = nextWord(&pastSpace);
		branch->hideTable = TRUE;
		}

	    parent->allInAttributes = FALSE;
	    parent->gotChildTags = TRUE;
	    }
	else
	    {
	    if (sameString(target, "hide"))
		{
		branch->hideField = TRUE;
		}
	    else if (sameString(target, "text"))
	        {
		tf = findField(tfList, field);
		branch->fieldIx = slIxFromElement(tfList, tf);
		branch->textField = TRUE;
		parent->allInAttributes = FALSE;
		}
	    else
	        {
		errAbort("Don't recognize %s line %d of %s", 
			target, lf->lineIx, lf->fileName);
		}
	    }
	if (extra != NULL)
	   {
	   if (sameString(extra, "as"))
	       {
	       char *outName = nextWord(&pastSpace);
	       if (outName == NULL)
	           errAbort("Expecting something after 'as' line %d of %s.",
		   	lf->lineIx, lf->fileName);
	       branch->outName = cloneString(outName);
	       extra = nextWord(&pastSpace);
	       }
	   }
	if (extra != NULL)
	    errAbort("There's extra stuff I don't understand line %d of %s.",
		    lf->lineIx, lf->fileName);
	if (branch->outName == NULL)
	    branch->outName = cloneString(branch->targetTable);
	slAddTail(&parent->children, branch);
	}
    }
}

struct specTree *specTreeLoad(char *fileName, struct hash *tableHash)
/* Load up a spec-tree from a file. */
{
struct specTree *root = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
char *table, *as, *outName;

AllocVar(root);
if (!lineFileNextReal(lf, &line))
    lineFileUnexpectedEnd(lf);
line = trimSpaces(line);
table = nextWord(&line);
as = nextWord(&line);
outName = nextWord(&line);
root->targetTable = cloneString(table);
if (as != NULL)
   {
   if (outName == NULL)
       errAbort("Expecting something after 'as' line %d of %s", 
       		lf->lineIx, lf->fileName);
   root->outName = cloneString(outName);
   }
else
   root->outName = cloneString(root->targetTable);
rSpecTreeLoad(lf, root, tableHash);

lineFileClose(&lf);
return root;
}

struct hash *tablesAndFields(struct sqlConnection *conn)
/* Get hash of all tables.  Hash is keyed by table name.
 * Hash values are lists of typedField.   */
{
struct hash *hash = hashNew(0);
struct slName *table, *tableList = sqlListTables(conn);
for (table = tableList; table != NULL; table = table->next)
    {
    char query[256], **row;
    struct sqlResult *sr;
    struct typedField *fieldList = NULL, *field;
    safef(query, sizeof(query), "describe %s", table->name);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	char *name = row[0];
	char *type = row[1];
	char code = '"';
	if ( startsWith("int", type)  
	  || startsWith("float", type)
	  || startsWith("tinyint", type)
	  || startsWith("smallint", type))
	    code = ' ';
	AllocVar(field);
	field->name = cloneString(name);
	field->type = code;
	slAddHead(&fieldList, field);
	}
    slReverse(&fieldList);
    hashAdd(hash, table->name, fieldList);
    }
return hash;
}

void escToXmlString(char *in, char *out)
/* Put in the escapes xml needs inside of a string */
{
char c;
while ((c = *in++) != 0)
    {
    if (c == '&')
	{
        strcpy(out, "&amp;");
	out += 5;
	}
    else if (c == '"')
        {
	strcpy(out, "&quot;");
	out += 6;
	}
    else
        *out++ = c;
    }
*out = 0;
}

void rSqlToXml(struct sqlConnCache *cc, char *table, char *entryField,
	char *query, struct hash *tableHash, struct specTree *tree,
	FILE *f, int depth)
/* Recursively output XML */
{
struct sqlConnection *conn = sqlConnCacheAlloc(cc);
struct sqlResult *sr;
char **row;
struct typedField *col, *colList = hashMustFindVal(tableHash, table);

verbose(2, "%s\n", query);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    int rowIx = 0;

    /* Print non-joining columns as attributes. */
    if (!tree->hideTable)
	{
	spaceOut(f, depth*2);
	fprintf(f, "<%s", tree->outName);
	}
    for (col = colList; col != NULL; col = col->next, rowIx+=1)
        {
	char *val = row[rowIx];
	struct specTree *branch = specTreeTarget(tree, col->name);
	if (branch == NULL && !sameString(col->name, entryField))
	    {
	    int len = strlen(val);
	    dyStringBumpBufSize(escaper, len*5);
	    escToXmlString(val, escaper->string);
	    if (!tree->hideTable)
		fprintf(f, " %s=\"%s\"", col->name, escaper->string);
	    }
	}

    /* If no subobjects, then close up tag, else proceed to subobjects. */
    if (tree->allInAttributes)
	{
	if (!tree->hideTable)
	    fprintf(f, "/>\n");
	}
    else
        {
	struct specTree *branch;
	if (!tree->hideTable)
	    fprintf(f, ">");
	rowIx = 0;
	for (branch = tree->children; branch != NULL; branch = branch->next)
	    {
	    if (sameString(branch->target, "text"))
	        {
		char *val = row[branch->fieldIx];
		int len = strlen(val);
		dyStringBumpBufSize(escaper, len*5);
		escToXmlString(val, escaper->string);
		fprintf(f, "%s", escaper->string);
		}
	    }
	if (tree->gotChildTags)
	    if (!tree->hideTable)
		fprintf(f, "\n");
	for (branch = tree->children; branch != NULL; branch = branch->next)
	    {
	    char *target = branch->target;
	    if (sameString(target, "text"))
	        ;
	    else if (!sameString(target, "hide"))
		{
		struct dyString *sql = dyStringNew(0);
		int newDepth = depth;
		if (!tree->hideTable)
		    newDepth += 1;
		dyStringPrintf(sql, "select * from %s where %s = ",
			branch->targetTable, target);
		if (branch->needsQuote)
		    dyStringPrintf(sql, "\"%s\"", row[branch->fieldIx]);
		else
		    dyStringPrintf(sql, "%s", row[branch->fieldIx]);
		if (maxList != 0)
		    dyStringPrintf(sql, " limit %d", maxList);
		rSqlToXml(cc, branch->targetTable, branch->targetField, 
			sql->string, tableHash, branch, f, newDepth);
		dyStringFree(&sql);
		}
	    }
	if (!tree->hideTable)
	    {
	    if (tree->gotChildTags)
		spaceOut(f, depth*2);
	    fprintf(f, "</%s>\n", tree->outName);
	    }
	}
    }
sqlFreeResult(&sr);
sqlConnCacheDealloc(cc, &conn);
}


void sqlToXml(char *database, char *dumpSpec, char *outputXml)
/* sqlToXml - Given a database, .as file, .joiner file, and a sql select 
 * statement, dump out results as XML. */
{
struct sqlConnCache *cc = sqlConnCacheNew(database);
struct sqlConnection *conn = sqlConnCacheAlloc(cc);
struct hash *tableHash = tablesAndFields(conn);
struct specTree *tree = specTreeLoad(dumpSpec, tableHash);
FILE *f = mustOpen(outputXml, "w");
char *topTag = optionVal("topTag", database);
char *table = tree->targetTable;
struct dyString *sql = dyStringNew(0);

if (optionExists("query"))
    {
    char *queryFile = optionVal("query", NULL);
    char *query;
    readInGulp(queryFile, &query, NULL);
    if (!stringIn(table, query))
	errAbort("No mention of table %s in %s.", table, queryFile);
    dyStringAppend(sql, query);
    }
else
    dyStringPrintf(sql, "select * from %s", table);

if (maxList > 0)
    dyStringPrintf(sql, " limit %d", maxList);

if (!sqlTableExists(conn, table))
    errAbort("No table %s in %s", table, database);
sqlConnCacheDealloc(cc, &conn);

verbose(1, "%d tables in %s\n",
	tableHash->elCount,  database);

escaper = dyStringNew(0);
fprintf(f, "<%s>\n", topTag);
rSqlToXml(cc, table, "", sql->string, tableHash, tree, f, 1);
fprintf(f, "</%s>\n", topTag);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
tabSize = optionInt("tab", tabSize);
maxList = optionInt("maxList", maxList);
sqlToXml(argv[1], argv[2], argv[3]);
return 0;
}
