/* NewSang6 - get some recently finished BACs by Sanger
 * on chromosome 6 to help make part of a test set. */
#include "common.h"
#include "jksql.h"

struct newlyFinished
    {
    struct newlyFinished *next;
    char *acc;
    char *map;
    boolean gotBoth;
    int oldPhase;
    };

int main(int argc, char *argv[])
{
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
struct newlyFinished *nfList = NULL, *nf;
char query[256];

/* Get new versions. */
conn = sqlConnect("h");
sr = sqlGetResult(conn, 
	"NOSQLINJ select bac.acc,cytoMap.name from bac,seq,cytoMap "
	"where bac.center = 2160 and bac.phase=3 "
	"and seq.gb_date >= '1999-12-23' and bac.chromosome = 3530972 "
	"and bac.id = seq.id "
	"and bac.cytoMap = cytoMap.id "
	"order by cytoMap.name ");
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(nf);
    nf->acc = cloneString(row[0]);
    nf->map = cloneString(row[1]);
    slAddHead(&nfList, nf);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
slReverse(&nfList);

/* See if old versions exist and what phase they are. */
conn = sqlConnect("hgap");
for (nf = nfList; nf != NULL; nf = nf->next)
    {
    sqlSafef(query, sizeof query, "select phase from bac where acc = '%s'", nf->acc);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	nf->gotBoth = TRUE;
	nf->oldPhase = sqlUnsigned(row[0]);
	}
    sqlFreeResult(&sr);
    }

for (nf = nfList; nf != NULL; nf = nf->next)
    {
    uglyf("%s %s %s %d\n", nf->acc, nf->map, 
    	(nf->gotBoth ? "TRUE" : "FALSE"), nf->oldPhase);
    }
}

