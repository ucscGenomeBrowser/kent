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

static char const rcsid[] = "$Id: hgLoadNetDist.c,v 1.1 2007/03/29 23:30:23 galt Exp $";

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
printf("beginning processing sqlRemap query [%s] \n",sqlRemap);
aliasHash = newHash(8);
hSetDb(db);
conn = hAllocConn();
sr = sqlGetResult(conn, sqlRemap);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(aliasHash, row[0], cloneString(row[1]));
    }
sqlFreeResult(&sr);

hFreeConn(&conn);
}


void hgLoadNetDist(char *inTab, char *db, char *outTable)
{
char *tempDir = ".";
FILE *f = hgCreateTabFile(tempDir, outTable);

struct sqlConnection *hConn = sqlConnect(db);

FILE *missingF=NULL;
int missingCount=0;

struct lineFile *lf=NULL;
char *row[3];
int rowCount=3;

if (sqlRemap)
    {
    fetchRemapInfo(db);
    missingHash = newHash(16);  
    missingF = mustOpen("missing.tab","w");
    }

/* read edges from file */


lf=lineFileOpen(inTab, TRUE);

/* print final dyn prg array values */

while (lineFileNextRowTab(lf, row, rowCount))
    {
    char *geneI = row[0];
    char *geneJ = row[1];
    char *dij = row[2];
    char *gi=NULL, *gj=NULL;
    if (sqlRemap)
	{ /* it is possible for each id to have multiple remap values in hash */
	struct hashEl *hi=NULL, *hj=NULL, *hMissing=NULL, *hij=NULL;
	int z;
	char *missingKey = NULL;
	hi = hashLookup(aliasHash,geneI);
	hj = hashLookup(aliasHash,geneJ);
	for(z=0;z<2;z++)  /* do both i and j */
	    {
	    if (z==0)
		{
		missingKey = geneI;
		hij = hi;
		}
	    else
		{
		missingKey = geneJ;
		hij = hj;
		}
	    if (!hij) 
		{
		hMissing = hashLookup(missingHash,missingKey);
		if (!hMissing)
		    {
		    ++missingCount;
		    hashAdd(missingHash, missingKey, NULL);
		    fprintf(missingF, "%s\n", missingKey);
		    }
		}
	    }		    
	for(;hi;hi=hashLookupNext(hi))
	    {
	    gi = (char *)hi->val;
	    for(;hj;hj=hashLookupNext(hj))
		{
		gj = (char *)hj->val;
		fprintf(f,"%s\t%s\t%s\n",gi,gj,dij);
		}
	    hj = hashLookup(aliasHash,geneJ); /* reset it */
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
    fclose(missingF);
    if (missingCount == 0)
	{
	unlink("missing.tab");
	}
    else	    
    	printf("hgLoadNetDist %d id-remapping misses!  see missing.tab\n", missingCount);
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


