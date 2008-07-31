/* hgLoadNetDist - Gene/Interaction Network Distance loader for GS 
   Galt Barber 2005-05-08
   This program uses as input the path-lengths output from hgNetDist
   and creates a table in the specified database, optionally remapping
   the ids with the specified query string.
*/

#include <stdio.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

#include "hdb.h"

static char const rcsid[] = "$Id: hgLoadNetDist.c,v 1.3.64.1 2008/07/31 02:24:45 markd Exp $";

char *sqlRemap=NULL;

static struct optionSpec options[] = {
   {"sqlRemap", OPTION_STRING},
   {NULL, 0},
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadNetDist - GS loader for interaction network path lengths.\n"
  "usage:\n"
  "   hgLoadNetDist input.tab database table\n"
  "This will read the input.tab file and load it into the database table\n"
  "specified, for use with Gene Sorter.\n"
  "Input .tab path-lengths file format is 3 columns: gene1 gene2 path-length.\n"
  "options:\n"
  "   -sqlRemap='select col1, col2 from table' "
  	"- specify query used to remap ids from col1 to col2 in input.tab.\n"
  );
}


struct hash *aliasHash = NULL;
struct hash *missingHash = NULL;


/* --- the fields needed from input tab file  ---- */

void createTable(struct sqlConnection *conn, char *tableName)
/* Create our name/value table, dropping if it already exists. */
{
struct dyString *dy = dyStringNew(512);
dyStringPrintf(dy,
"CREATE TABLE  %s (\n"
"    query varchar(255) not null,\n"
"    target varchar(255) not null,\n"
"    distance float,\n"
"#Indices\n"
"    INDEX(query(8))\n"
")\n",  tableName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
}




void fetchRemapInfo(char *db)
/* fetch id-remap as a hash using -sqlRemap="some sql" commandline option 
 * read all the gene aliases from database and put in aliasHash           
 */
{
struct sqlConnection* conn = NULL;
struct sqlResult *sr;
char **row;
/* it is possible for each id to have multiple remap values in hash */
if (sqlRemap == NULL) return;
verbose(2,"beginning processing sqlRemap query [%s] \n",sqlRemap);
aliasHash = newHash(8);
conn = hAllocConn(db);
sr = sqlGetResult(conn, sqlRemap);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(aliasHash, row[0], cloneString(row[1]));
    }
sqlFreeResult(&sr);

hFreeConn(&conn);
}

boolean handleMissing(struct hashEl *h, char *gene, struct hash *missingHash, FILE *missingFile)
/* Write out info on gene to missing file if necessary. Return TRUE if it's the
 * first time gene has gone missing. */
{
if (!h)  /* not found in alias Hash */
    {
    if (!hashLookup(missingHash, gene))
	{
	hashAdd(missingHash, gene, NULL);
	fprintf(missingFile, "%s\n", gene);
	return TRUE;
	}
    }
return FALSE;
}



void hgLoadNetDist(char *inTab, char *db, char *outTable)
{
char *tempDir = ".";
FILE *f = hgCreateTabFile(tempDir, outTable);

struct sqlConnection *hConn = sqlConnect(db);

FILE *missingFile=NULL;
int missingCount=0;

struct lineFile *lf=NULL;
char *row[3];
int rowCount=3;

if (sqlRemap)
    {
    fetchRemapInfo(db);
    missingHash = newHash(16);  
    missingFile = mustOpen("missing.tab","w");
    }

/* read edges from file */


lf=lineFileOpen(inTab, TRUE);

/* print final values, remapping if needed */

while (lineFileNextRowTab(lf, row, rowCount))
    {
    char *geneI = row[0];
    char *geneJ = row[1];
    char *dij = row[2];
    char *gi=NULL, *gj=NULL;
    if (sqlRemap)
	{ /* it is possible for each id to have multiple remap values in hash */
	struct hashEl *hi=NULL, *hj=NULL, *hjSave=NULL;
	hi = hashLookup(aliasHash,geneI);
	hj = hashLookup(aliasHash,geneJ);
	missingCount += handleMissing(hi, geneI, missingHash, missingFile);
	missingCount += handleMissing(hj, geneJ, missingHash, missingFile);
	hjSave = hj;
	/* do all combinations of i and j */	
	for(;hi;hi=hashLookupNext(hi))
	    {
	    gi = (char *)hi->val;
	    for(;hj;hj=hashLookupNext(hj))
		{
		gj = (char *)hj->val;
		fprintf(f,"%s\t%s\t%s\n",gi,gj,dij);
		}
	    hj = hjSave; /* reset it */
	    }
	}
    else
	{
	gi=geneI;
	gj=geneJ;
	fprintf(f,"%s\t%s\t%s\n",gi,gj,dij);
	}
    }

lineFileClose(&lf);
carefulClose(&f);    

if (sqlRemap)
    {
    carefulClose(&missingFile);
    if (missingCount == 0)
	unlink("missing.tab");
    else	    
    	printf("hgLoadNetDist %d id-remapping misses, see missing.tab\n", missingCount);
    }

createTable(hConn, outTable);
hgLoadTabFile(hConn, tempDir, outTable, &f);
hgRemoveTabFile(tempDir, outTable);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

sqlRemap = optionVal("sqlRemap", sqlRemap);

hgLoadNetDist(argv[1],argv[2],argv[3]);

return 0;
}


