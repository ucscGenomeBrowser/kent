/* hgLoadNet - Load a chain/net file into database. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "obscure.h"
#include "hash.h"
#include "jksql.h"
#include "dystring.h"
#include "bed.h"
#include "hdb.h"
#include "chainNet.h"
#include "options.h"


/* Command line switches. */
boolean noBin = FALSE;		/* Suppress bin field. */
boolean oldTable = FALSE;	/* Don't redo table. */
boolean warnFlag = FALSE;           /* load even with missing fields */
boolean warned = FALSE;
char *sqlTable = NULL;		/* Read table from this .sql if non-NULL. */
char *qPrefix = NULL;		/* Prepend prefix and "-" to query name */
boolean test = FALSE;

static struct optionSpec optionSpecs[] = {
        {"noBin", OPTION_BOOLEAN},
        {"oldTable", OPTION_BOOLEAN},
        {"warn", OPTION_BOOLEAN},
        {"sqlTable", OPTION_STRING},
        {"qPrefix", OPTION_STRING},
        {"test", OPTION_BOOLEAN},
        {NULL, 0}
};


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadNet - Load a generic net file into database\n"
  "usage:\n"
  "   hgLoadNet database track files(s).net\n"
  "options:\n"
  "   -noBin   suppress bin field\n"
  "   -oldTable add to existing table\n"
  "   -sqlTable=table.sql Create table from .sql file\n"
  "   -qPrefix=xxx prepend \"xxx-\" to query name\n"
  "   -warn load even with missing fields\n"
  "   -test suppress loading table\n"
  );
}


void loadDatabase(char *database, char *tab, char *track)
/* Load database from tab file. */
{
struct sqlConnection *conn = sqlConnect(database);
struct dyString *dy = dyStringNew(1024);
/* First make table definition. */
if (sqlTable != NULL)
    {
    /* Read from file. */
    char *sql, *s;
    readInGulp(sqlTable, &sql, NULL);

    /* Chop of end-of-statement semicolon if need be. */
    s = strchr(sql, ';');
    if (s != NULL) *s = 0;
    
    sqlRemakeTable(conn, track, sql);
    freez(&sql);
    }
else if (!oldTable)
    {
    /* Create definition statement. */
    verbose(1, "Creating table definition for %s\n", track);
    sqlDyStringPrintf(dy, "CREATE TABLE %s (\n", track);
    if (!noBin)
	sqlDyStringPrintf(dy, "  bin smallint unsigned not null,\n");
    sqlDyStringPrintf(dy,
    "  level int unsigned not null,\n"
    "  tName varchar(255) not null,\n"
    "  tStart int unsigned not null,\n"
    "  tEnd int unsigned not null,\n"
    "  strand char(1) not null,\n"
    "  qName varchar(255) not null,\n"
    "  qStart int unsigned not null,\n"
    "  qEnd int unsigned not null,\n"
    "  chainId int unsigned not null,\n"
    "  ali int unsigned not null,\n"
    "  score double not null,\n"
    "  qOver int not null, \n"
    "  qFar int not null, \n"
    "  qDup int not null, \n"
    "  type varchar(255) not null,\n"
    "  tN int not null, \n"
    "  qN int not null, \n"
    "  tR int not null, \n"
    "  qR int not null, \n"
    "  tNewR int not null, \n"
    "  qNewR int not null, \n"
    "  tOldR int not null, \n"
    "  qOldR int not null, \n"
    "  tTrf int not null, \n"
    "  qTrf int not null, \n"
    "#Indices\n");
    if (!noBin)
	sqlDyStringPrintf(dy, "  INDEX(tName(16),bin),\n");
    sqlDyStringPrintf(dy,
    "  INDEX(tName(16),tStart)\n"
    ")\n");
    sqlRemakeTable(conn, track, dy->string);
    }

dyStringClear(dy);
sqlDyStringPrintf(dy, "load data local infile '%s' into table %s", tab, track);
verbose(1, "Loading %s into %s\n", track, database);
sqlUpdate(conn, dy->string);
/* add a comment to the history table and finish up connection */
hgHistoryComment(conn, "Loaded net table %s", track);
sqlDisconnect(&conn);
}

void cnWriteTables(char *chrom, struct cnFill *fillList, FILE *f, int depth)
/* Recursively write out fill and gap lists. */
{
char qName[64];
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    if (fill->chainId != 0)
	{
	if (fill->type == NULL)
	    errAbort("No type field, please run netSyntenic on input");
	if (fill->tN < 0)
	    {
            if (warnFlag)
                {
                if (!warned)
                    {
                    fprintf(stderr, "Warning: missing fields\n");
                    warned = TRUE;
                    }
                }
            else
                errAbort("Missing fields.  Please run netClass on input");
	    }
	}
    if (fill->score < 0)
        fill->score = 0;
    if (!noBin)
	fprintf(f, "%d\t", hFindBin(fill->tStart, fill->tStart + fill->tSize));
    qName[0] = 0;
    if (qPrefix != NULL)
        {
        strcat(qName, qPrefix);
        strcat(qName, "-");
        }
    strcat(qName, fill->qName);
    fprintf(f, "%d\t", depth);
    fprintf(f, "%s\t", chrom);
    fprintf(f, "%d\t", fill->tStart);
    fprintf(f, "%d\t", fill->tStart + fill->tSize);
    fprintf(f, "%c\t", fill->qStrand);
    fprintf(f, "%s\t", qName);
    fprintf(f, "%d\t", fill->qStart);
    fprintf(f, "%d\t", fill->qStart + fill->qSize);
    fprintf(f, "%d\t", fill->chainId);
    fprintf(f, "%d\t", fill->ali);
    fprintf(f, "%1.1f\t", fill->score);
    fprintf(f, "%d\t", fill->qOver);
    fprintf(f, "%d\t", fill->qFar);
    fprintf(f, "%d\t", fill->qDup);
    fprintf(f, "%s\t", (fill->type == NULL ? "gap" : fill->type));
    fprintf(f, "%d\t", fill->tN);
    fprintf(f, "%d\t", fill->qN);
    fprintf(f, "%d\t", fill->tR);
    fprintf(f, "%d\t", fill->qR);
    fprintf(f, "%d\t", fill->tNewR);
    fprintf(f, "%d\t", fill->qNewR);
    fprintf(f, "%d\t", fill->tOldR);
    fprintf(f, "%d\t", fill->qOldR);
    fprintf(f, "%d\t", fill->tTrf);
    fprintf(f, "%d\n", fill->qTrf);
    if (fill->children)
        cnWriteTables(chrom, fill->children, f, depth+1);
    }
}

void hgLoadNet(char *database, char *track, int netCount, char *netFiles[])
/* hgLoadNet - Load a net file into database. */
{
int i;
struct lineFile *lf ;
struct chainNet *net;
char alignFileName[] ="align.tab";
FILE *alignFile = mustOpen(alignFileName,"w");

for (i=0; i<netCount; ++i)
    {
    lf = lineFileOpen(netFiles[i], TRUE);
    while ((net = chainNetRead(lf)) != NULL)
        {
        verbose(1, "read %s\n",net->name);
        cnWriteTables(net->name,net->fillList, alignFile, 1);
	chainNetFree(&net);
        }
    lineFileClose(&lf);
    }
fclose(alignFile);
if (!test)
    {
    loadDatabase(database, alignFileName, track);
    remove(alignFileName);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 4)
    usage();
noBin = optionExists("noBin");
oldTable = optionExists("oldTable");
warnFlag = optionExists("warn");
sqlTable = optionVal("sqlTable", NULL);
qPrefix = optionVal("qPrefix", NULL);
test = optionExists("test");
hgLoadNet(argv[1], argv[2], argc-3, argv+3);
return 0;
}
