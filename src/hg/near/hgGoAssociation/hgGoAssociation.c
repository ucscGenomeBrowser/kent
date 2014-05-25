/* hgGoAssociation - Load bits we care about in GO association table. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "jksql.h"
#include "hdb.h"
#include "hgRelate.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGoAssociation - Load bits we care about in GO association table\n"
  "usage:\n"
  "   hgGoAssociation database table goaIn\n"
  "options:\n"
  "   -taxon=XXX,YYYY,ZZZZ - Restrict to NCBI taxon IDs XXX,YYY,ZZZZ\n"
  "   -tab=dir - Output tab-separated files to directory.\n"
  "   -noLoad  - If true don't load database and don't clean up tab files\n"
  "   -limit=N - Only do limit rows of table, for testing\n"
  );
}

struct hash *taxonHash = NULL;
char *tabDir = ".";
boolean doLoad;
int limit = 0;


static struct optionSpec options[] = {
   {"taxon", OPTION_STRING},
   {"tab", OPTION_STRING},
   {"noLoad", OPTION_BOOLEAN},
   {"limit", OPTION_INT},
   {NULL, 0},
};

struct hash *hashCommaList(char *s)
/* Hash comma separated list. */
{
struct hash *hash = newHash(0);
char *e;
while (s != NULL && s[0] != 0)
    {
    e = strchr(s, ',');
    if (e != NULL)
        *e++ = 0;
    hashAdd(hash, s, NULL);
    s = e;
    }
return hash;
}

static void createTable(struct sqlConnection *conn, char *table)
/* Create goaPart type table. */
{
char query[1024];

sqlSafef(query, sizeof(query),
"CREATE TABLE %s (\n"
"    dbObjectId varchar(255) not null,	# Database accession - like 'Q13448'\n"
"    dbObjectSymbol varchar(255) not null,	# Name - like 'CIA1_HUMAN'\n"
"    notId varchar(255) not null,	# (Optional) If 'NOT'. Indicates object isn't goId\n"
"    goId varchar(255) not null,	# GO ID - like 'GO:0015888'\n"
"    aspect varchar(255) not null,	#  P (process), F (function) or C (cellular component)\n"
"              #Indices\n"
"    INDEX(dbObjectId(10)),\n"
"    INDEX(dbObjectSymbol(8))\n"
")\n",   table);
sqlRemakeTable(conn, table, query);
}


void hgGoAssociation(char *database, char *table, char *goaIn)
/* hgGoAssociation - Load bits we care about in GO association table. */
{
struct lineFile *lf = lineFileOpen(goaIn, TRUE);
struct hash *uniqHash = newHash(24);
char uniqName[256];
char *row[15];
int allCount = 0;
int uniqCount = 0;
int taxonCount = 0;
char *line;
int lineSize;

FILE *f = hgCreateTabFile(tabDir, table);

/* skip initial comment lines */
lineFileNext(lf, &line, &lineSize);
while (*line == '!')
	{
	lineFileNext(lf, &line, &lineSize);
	}
	
/* back off 1 line, to get the valid data line */	
lineFileReuse(lf);

while (lineFileNextRowTab(lf, row, 15))
    {
    char *dbObjectId = row[1];
    char *goId = row[4];
    char *taxon = row[12] + 6;
    int id = atoi(goId+3);
    ++allCount;
    if (taxonHash == NULL || hashLookup(taxonHash, taxon))
	{
	++taxonCount;
	safef(uniqName, sizeof(uniqName), "%s.%x", dbObjectId, id);
	if (hashLookup(uniqHash, uniqName) == NULL)
	    {
	    hashAdd(uniqHash, uniqName, NULL);
	    fprintf(f, "%s\t%s\t%s\t%s\t%s\n",
	    	row[1], row[2], row[3], row[4], row[8]);
	    uniqCount += 1;
	    if (limit != 0 && uniqCount >= limit)
	        break;
	    }
	}
    }
printf("Passed %d of %d of %d, %0.2f%%\n", 
	uniqCount, taxonCount, allCount, 100.0*uniqCount/allCount);

if (doLoad)
    {
    struct sqlConnection *conn = sqlConnect(database);
    createTable(conn, table);
    hgLoadTabFile(conn, tabDir, table, &f);
    hgRemoveTabFile(tabDir, table);
    sqlDisconnect(&conn);
    }
else
    carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (optionExists("taxon"))
    taxonHash = hashCommaList(optionVal("taxon", NULL));
doLoad = !optionExists("noLoad");
if (optionExists("tab"))
    {
    tabDir = optionVal("tab", tabDir);
    makeDir(tabDir);
    }
limit = optionInt("limit", limit);
if (argc != 4)
    usage();
hgGoAssociation(argv[1], argv[2], argv[3]);
return 0;
}
