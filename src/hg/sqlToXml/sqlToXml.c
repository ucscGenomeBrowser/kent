/* sqlToXml - Given a database, .as file, .joiner file, and a sql select 
 * statement, dump out results as XML. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "asParse.h"

static char const rcsid[] = "$Id: sqlToXml.c,v 1.1 2005/11/27 11:47:39 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sqlToXml - Given a database, .as file, .dumpSpec file, and a sql select\n"
  "statement, dump out results as XML.\n"
  "usage:\n"
  "   sqlToXml database table 'query' db.as xml.dumpSpec output.xml\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct dyString *escaper;

struct specTree 
/* Holds dump specification tree. */
   {
   struct specTree *next;	/* Sibling */
   struct specTree *children;
   char *field;		/* Field in table to link on */
   char *target;	/* Target table.field  */
   };

struct specTree *specTreeTarget(struct specTree *parent, char *field)
/* Look through all children for matching field. */
{
struct specTree *tree;
for (tree = parent->children; tree != NULL; tree = tree->next)
    if (sameString(tree->field, field))
	break;
return tree;
}

void rSpecTreeLoad(struct lineFile *lf, struct specTree *parent)
/* Load all children of parent. */
{
char *line, *pastSpace;
int indent, curIndent;
struct specTree *branch = NULL;
if (!lineFileNextReal(lf, &line))
    return;
pastSpace = skipLeadingSpaces(line);
curIndent = pastSpace - line;
lineFileReuse(lf);
for (;;)
    {
    if (!lineFileNextReal(lf, &line))
        break;
    pastSpace = skipLeadingSpaces(line);
    indent = pastSpace - line;
    if (indent > curIndent)
	{
	lineFileReuse(lf);
	rSpecTreeLoad(lf, branch);
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
	char *pastEnd = nextWord(&pastSpace);
	if (target == NULL)
	   errAbort("Missing target line %d of %s", lf->lineIx, lf->fileName);
	if (pastEnd != NULL)
	   errAbort("Extra word line %d of %s", lf->lineIx, lf->fileName);
	AllocVar(branch);
	branch->field = cloneString(field);
	branch->target = cloneString(target);
	slAddTail(&parent->children, branch);
	}
    }
}

struct specTree *specTreeLoad(char *fileName)
/* Load up a spec-tree from a file. */
{
struct specTree *root = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;

AllocVar(root);
if (!lineFileNextReal(lf, &line))
    lineFileUnexpectedEnd(lf);
line = trimSpaces(line);
verbose(1, "%s starts with %s\n", fileName, line);
rSpecTreeLoad(lf, root);

lineFileClose(&lf);
return root;
}

void rSqlToXml(struct sqlConnCache *cc, char *table, char *entryField,
	char *query, struct hash *asHash, struct specTree *tree,
	FILE *f, int depth)
/* Recursively output XML */
{
struct sqlConnection *conn = sqlAllocConnection(cc);
struct sqlResult *sr;
char **row;
struct asObject *as = hashFindVal(asHash, table);
struct asColumn *col;
boolean subObjects = FALSE;

if (as == NULL)
    errAbort("Couldn't find %s in .as file", table);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    int rowIx = 0;

    /* Print non-joining columns as attributes. */
    spaceOut(f, depth*2);
    fprintf(f, "<%s", table);
    for (col = as->columnList; col != NULL; col = col->next, rowIx+=1)
        {
	char *val = row[rowIx];
	struct specTree *branch = specTreeTarget(tree, col->name);
	if (branch != NULL)
	    subObjects = TRUE;
	else if (!sameString(col->name, entryField))
	    {
	    int len = strlen(val);
	    dyStringBumpBufSize(escaper, len*2);
	    escCopy(val, escaper->string, '"', '\\');
	    fprintf(f, " %s=\"%s\"", col->name, escaper->string);
	    }
	}

    /* If no subobjects, then close up tag, else proceed to subobjects. */
    if (!subObjects)
        fprintf(f, "/>\n");
    else
        {
	fprintf(f, ">\n");
	rowIx = 0;
	for (col = as->columnList; col != NULL; col = col->next, rowIx+=1)
	    {
	    struct specTree *branch = specTreeTarget(tree, col->name);
	    if (branch != NULL)
	        {
		char *target = branch->target;
		char targetTable[256];
		char query[512];
		char *targetField = strchr(target, '.');
		if (targetField != NULL)
		    targetField += 1;
		safef(targetTable, sizeof(targetTable), target);
		chopSuffix(targetTable);
		safef(query, sizeof(query),
		    "select * from %s where %s = %s", targetTable,
			target, row[rowIx]);
		rSqlToXml(cc, targetTable, targetField, query, asHash, branch, f, depth+1);
		}
	    }
	spaceOut(f, depth*2);
	fprintf(f, "</%s>\n", table);
	}
    }
sqlFreeResult(&sr);
sqlFreeConnection(cc, &conn);
}

struct hash *asHashFile(char *fileName)
/* Return hash full of definitions from .as file */
{
struct asObject *as, *asList = asParseFile(fileName);
struct hash *hash = hashNew(0);
for (as = asList; as != NULL; as = as->next)
    hashAdd(hash, as->name, as);
return hash;
}

void sqlToXml(char *database, char *table, char *query, char *asFile, 
	char *dumpSpec, char *outputXml)
/* sqlToXml - Given a database, .as file, .joiner file, and a sql select 
 * statement, dump out results as XML. */
{
struct sqlConnCache *cc = sqlNewConnCache(database);
struct sqlConnection *conn = sqlAllocConnection(cc);
struct specTree *tree = specTreeLoad(dumpSpec);
struct hash *asHash = asHashFile(asFile);
FILE *f = mustOpen(outputXml, "w");

if (!sqlTableExists(conn, table))
    errAbort("No table %s in %s", table, database);
if (!stringIn(table, query))
    errAbort("No mention of table %s in query.", table);
sqlFreeConnection(cc, &conn);

verbose(1, "%d tables in %s\n",
	asHash->elCount,  asFile);

escaper = dyStringNew(0);
rSqlToXml(cc, table, "", query, asHash, tree, f, 0);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
sqlToXml(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
