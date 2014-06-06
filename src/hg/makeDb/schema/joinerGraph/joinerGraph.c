/* joinerGraph - Make graph out of joiner by creating a .dot file for
 * Graphvis.. */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "joiner.h"
#include "asParse.h"

/* Variable that can be set from command line. */
char *genoDb = "hg16";	/* Genomic database. */
char *asFile = NULL;	/* autoSql file with field definitions if any. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "joinerGraph - Make graph out of joiner via Graphvis .dot file\n"
  "usage:\n"
  "   joinerGraph file.joiner graph.dot\n"
  "options:\n"
  "   -as=file.as - Create graph with fields as well as tables using .as file\n"
  "   -db=db - Use given database primarily (default is %s)\n"
  ,  genoDb
  );
}

char *ourDb(struct slName *list)
/* Return TRUE if it contains one of our databases. */
{
static char *dbs[] = {"hg16", "swissProt", "go", "hgFixed"};
int i;
dbs[0] = genoDb;
for (i=0; i<ArraySize(dbs); ++i)
    {
    char *db = dbs[i];
    if (slNameInList(list, db))
	return db;
    }
return NULL;
}

struct hash *readAs(char *fileName)
/* Read in .as file and put objects in hash keyed both
 * by table name, and by comma-separated field list. */
{
struct asObject *as, *asList = asParseFile(fileName);
struct hash *asHash = hashNew(10);
struct dyString *dy = dyStringNew(0);
for (as = asList; as != NULL; as = as->next)
    {
    struct asColumn *col;
    hashAdd(asHash, as->name, as);
    dyStringClear(dy);
    for (col = as->columnList; col != NULL; col = col->next)
	{
	dyStringAppend(dy, col->name);
	dyStringAppendC(dy, ',');
	}
    hashAdd(asHash, dy->string, as);
    }
dyStringFree(&dy);
return asHash;
}

struct hash *makeTypeLookupHash(struct joiner *joiner)
/* Make hash keyed by table names with autoSql table type
 * values. */
{
struct joinerType *type;
struct joinerTable *table;
struct hash *hash = hashNew(0);
for (type = joiner->typeList; type != NULL; type = type->next)
    {
    for (table = type->tableList; table != NULL; table = table->next)
	{
	hashAdd(hash, table->table, type->name);
	}
    }
return hash;
}

void edgeColor(FILE *f, char *primaryDb, struct joinerField *primary)
/* Write color of edge so that knownGene and refLink show up special. */
{
if (sameString(primary->table, "knownGene")
    || sameString(primary->table, "sgdGene")
    || sameString(primary->table, "bdgpGene")
    || sameString(primary->table, "sangerGene") )
    fprintf(f, " [color=purple]");
else if (sameString(primary->table, "refLink"))
    fprintf(f, " [color=blue]");
else if (sameString(primary->table, "ensGene") 
	|| sameString(primary->table, "ensTranscript"))
    fprintf(f, " [color=forestGreen]");
else if (sameString(primaryDb, "swissProt"))
    fprintf(f, " [color=red]");
else if (sameString(primaryDb, "hgFixed"))
    fprintf(f, " [color=orange]");
}

char *nodeColor(FILE *f, char *db, struct joinerField *jf)
/* Write color of database. */
{
char *col = NULL;
if (sameString("swissProt", db))
    col = "red";
else if (sameString("hgFixed", db))
    col = "orange";
else if (startsWith("ens", jf->table))
    col = "forestGreen";
else if (startsWith("ref", jf->table))
    col = "blue";
else if (startsWith("known", jf->table) || startsWith("kg", jf->table))
    col = "purple";
return col;
}

void edgeArrow(FILE *f, struct joinerField *jf)
/* Figure out what type of fancy arrow if any to make. */
{
if (jf->full && jf->unique)
    fprintf(f, "[arrowhead=invdot]");
else if (jf->full)
    fprintf(f, "[arrowhead=dot]");
else if (jf->unique)
    fprintf(f, "[arrowhead=invodot]");
}

void makeEdge(FILE *f, struct hash *asHash, char *primaryDb, 
	struct joinerField *primary, char *otherDb, struct joinerField *jf)
/* Cause a single edge to be drawn between primary and jf. */
{
if (asHash)
    {
    fprintf(f, "  %s_%s:%s -> %s_%s:%s", 
	primaryDb, primary->table,  primary->field,
	otherDb, jf->table, jf->field);
    }
else 
    {
    fprintf(f, "  %s_%s -> %s_%s", 
	primaryDb, primary->table,  otherDb, jf->table);
    }
edgeColor(f, primaryDb, primary);
edgeArrow(f, jf);
fprintf(f, ";\n");
}

void makeGraph(struct joiner *joiner, struct hash *asHash, char *dotFile)
/* Make graph out of joiner and as records showing fields. */
{
struct joinerSet *js;
struct hash *uniqHash = hashNew(0);
struct hash *typeLookupHash = makeTypeLookupHash(joiner);
FILE *f = mustOpen(dotFile, "w");
static char *dbs[] = {"go", "swissProt", NULL, "hgFixed"};
int dbIx;
char *db;

dbs[2] = genoDb;

fprintf(f, "digraph %s {\n", "schema");
if (asHash != NULL)
    {
    fprintf(f, " rankdir=LR;\n");
    fprintf(f, " node [shape=record];\n");
    }
fprintf(f, " edge [arrowhead=odot];\n");
for (dbIx = 0; dbIx < ArraySize(dbs); ++dbIx)
    {
    db = dbs[dbIx];
    if (sameString(db, "swissProt") || sameString(db, "go"))
	{
	fprintf(f, " subgraph cluster_%s {\n", db);
	fprintf(f, "  label=\"%s database\";\n", db);
	}
    else
	{
	fprintf(f, " subgraph %s {\n", db);
	}
    for (js = joiner->jsList; js != NULL; js = js->next)
	{
	struct joinerField *jf, *primary = js->fieldList;
	/* Draw nodes. */
	for (jf = js->fieldList; jf != NULL; jf = jf->next)
	    {
	    if (slNameInList(jf->dbList, db))
		{
		char dbTable[512];
		safef(dbTable, sizeof(dbTable), "%s.%s", db, jf->table);
		if (!hashLookup(uniqHash, dbTable))
		    {
		    char tableName[128];
		    hashAdd(uniqHash, dbTable, NULL);
		    struct asObject *as;
		    char *dbCol;
		    char *type = hashFindVal(typeLookupHash, jf->table);
		    if (type == NULL)
			type = jf->table;
		    if (sameString(db, "hgFixed"))
			 safef(tableName, sizeof(tableName), "%s\\n%s", db, jf->table);
		    else
			 safef(tableName, sizeof(tableName), "%s", jf->table);
		    if (asHash)
			{
			as = hashFindVal(asHash, type);
			fprintf(f, "  %s_%s [label=\"*%s*", db, jf->table, tableName);
			if (as != NULL)
			    {
			    struct asColumn *col;
			    if (!sameString(as->name, jf->table))
				fprintf(f, "|(%s)", as->name);
			    for (col = as->columnList; col != NULL; col = col->next)
				{
				fprintf(f, "|<%s>%s", col->name, col->name);
				}
			    }
			fprintf(f, "\"");
			}
		    else
			{
			fprintf(f, "%s_%s [label=\"%s\"", db, jf->table, tableName);
			}
		    dbCol = nodeColor(f, db, jf);
		    if (dbCol != NULL)
			fprintf(f, ", color=%s", dbCol);
		    fprintf(f, "];\n");
		    }
		}
	    }
	/* Draw edges within database. */
	if (primary != NULL && slNameInList(primary->dbList, db))
	    {
	    for (jf = primary->next; jf != NULL; jf = jf->next)
		{
		if (slNameInList(jf->dbList, db))
		    makeEdge(f, asHash, db, primary, db, jf);
		}
	    }
	}
    fprintf(f, " }\n");
    }
/* Draw edges between databases. */
for (js = joiner->jsList; js != NULL; js = js->next)
    {
    struct joinerField *jf, *primary = js->fieldList;
    if (primary != NULL)
	{
	char *primaryDb = ourDb(primary->dbList);
	if (primaryDb != NULL)
	    {
	    for (jf = primary; jf != NULL; jf = jf->next)
		{
		char *otherDb = ourDb(jf->dbList);
		if (otherDb != NULL && !sameString(otherDb, primaryDb))
		    {
		    if (jf != primary)
			{
			makeEdge(f, asHash, primaryDb, primary, otherDb, jf);
			}
		    }
		}
	    }
	}
    }
fprintf(f, "}\n");
carefulClose(&f);
}


void joinerGraph(char *joinerFile, char *asFile, char *dotFile)
/* joinerGraph - Make graph out of joiner. */
{
struct joiner *joiner = joinerRead(joinerFile);
struct hash *asHash = NULL;
if (asFile != NULL)
    asHash = readAs(asFile);
makeGraph(joiner, asHash, dotFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
genoDb = optionVal("db", genoDb);
asFile = optionVal("as", asFile);
if (argc != 3)
    usage();
joinerGraph(argv[1], asFile, argv[2]);
return 0;
}
