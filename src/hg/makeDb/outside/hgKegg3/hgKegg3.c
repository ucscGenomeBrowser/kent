/* hgKegg3 - creates keggPathway.tab and keggMapDesc.tab files for KG links to KEGG Pathway Map */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgKegg3 - creates keggPathway.tab and keggMapDesc.tab files for KG links to KEGG Pathway Map"
  "usage:\n"
  "   hgKegg3 kgTempDb roDb\n"
  "      kgTempDb is the KG build temp database name\n"
  "      roDb is the read only genome database name\n"
  "example: hgKegg3 kgMm6ATemp mm6\n");
}

char *table = (char *)NULL;	/*name of table containing knownGenes or other gene prediction*/
static struct optionSpec options[] = {
   {"table", OPTION_STRING},
   {NULL, 0},
};

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn3;
char query[256], query3[256];
struct sqlResult *sr, *sr3;
char **row, **row3;

FILE *o1, *o2;

char *locusID;	/* LocusLink ID */

char *kgTempDbName, *roDbName; 
char cond_str[200];
char *kgId;
char *mapID;
char *desc;

optionInit(&argc, argv, options);
if (argc != 3)  usage();
kgTempDbName    = argv[1];
roDbName 	= argv[2];

conn = hAllocConn();
conn3= hAllocConn();

o1 = fopen("j.dat",  "w");
o2 = fopen("jj.dat", "w");
    
table = optionVal("table", "knownGene");
sprintf(query, "select name from %s.%s", roDbName, table);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
while (row != NULL)
    {
    kgId = row[0];
	
    sprintf(cond_str, "mrna='%s'", kgId);
    locusID = sqlGetField(conn3, "entrez", "geneIdMrna", "geneId", cond_str);
    if (locusID != NULL)
	{
        sprintf(query3, "select * from %s.keggList where locusID = '%s'", kgTempDbName, locusID);
        sr3 = sqlGetResult(conn3, query3);
        while ((row3 = sqlNextRow(sr3)) != NULL)
            {
            mapID   = row3[1];
	    desc    = row3[2];
	    fprintf(o1, "%s\t%s\t%s\n", kgId, locusID, mapID);
	    fprintf(o2, "%s\t%s\n", mapID, desc);
	    row3 = sqlNextRow(sr3);
            }
        sqlFreeResult(&sr3);
	}
    else
        {
	/* printf("%s not found in Entrez.\n", kgId);fflush(stdout);*/
        if (differentString(table, "knownGene"))
            {
            sprintf(cond_str, "name='%s'", kgId);
            locusID = sqlGetField(conn3, roDbName, table, "name2", cond_str);
            sprintf(query3, "select * from %s.keggList where locusID = '%s'", kgTempDbName, kgId);
            sr3 = sqlGetResult(conn3, query3);
            while ((row3 = sqlNextRow(sr3)) != NULL)
                {
                mapID   = row3[1];
                desc    = row3[2];
                fprintf(o1, "%s\t%s\t%s\n", kgId, locusID, mapID);
                fprintf(o2, "%s\t%s\n", mapID, desc);
                row3 = sqlNextRow(sr3);
                }
            sqlFreeResult(&sr3);
            }
        }
    row = sqlNextRow(sr);
    }

fclose(o1);
fclose(o2);
hFreeConn(&conn);

system("cat j.dat|sort|uniq >keggPathway.tab");
system("cat jj.dat|sort|uniq >keggMapDesc.tab");
system("rm j.dat");
system("rm jj.dat");
return(0);
}

