/* gadPos - generate genomic positions for GAD entries */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "options.h"

// Using options just to get -verbose:
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gadPos -  find genomic positions for GAD entries based on gene symbol\n"
  "usage:\n"
  "   gadPos db outFile\n"
  "      db is the genome database that must contain updated gadAll table,\n"
  "      known{Gene,Canonical,Isoforms}, kg{Xref,Alias}, and ref{Gene,Link}.\n"
  "      Each geneSymbol with association=Y in gadAll is looked up first in \n"
  "      knownCanonical; if not found then refGene; if not found, then kgAlias.\n"
  "      If db contains wgEncodeGencode{Pseudogene,Comp}, they will be searched\n"
  "      if geneSymbol still has not been found.\n"
  "      outFile is the output file name for GAD positions (bed 4 format)\n"
  "example: gadPos hg17 stdout | uniq > gadPos.tab\n");
}

boolean printBed4FromQueryAndName(struct sqlConnection *conn, char *query, char *name,
				  FILE *outF)
/* If there are results from query, assume that they are {chrom, chromStart, chromEnd},
 * print BED4 with those coords and name, and return TRUE; */
{
boolean found = FALSE;
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    found = TRUE;
    fprintf(outF, "%s\t%s\t%s\t%s\n", row[0], row[1], row[2], name);
    }
sqlFreeResult(&sr);
return found;
}

boolean findInKnownCanonical(struct sqlConnection *conn, char *geneSymbol, FILE *outF)
/* For each knownGene cluster for this geneSymbol, print output using the
 * cluster bounds as {chrom,chromStart,chromEnd} and return TRUE. */
{
boolean found = FALSE;
char query[1024];
sqlSafef(query, sizeof(query), "select distinct clusterId from kgXref x, knownIsoforms i "
		    "where x.geneSymbol='%s' and i.transcript=x.kgId", geneSymbol);
struct slName *id, *clusterIds = sqlQuickList(conn, query);
for (id = clusterIds;  id != NULL;  id = id->next)
    {
    sqlSafef(query, sizeof(query), "select k.chrom, min(txStart), max(txEnd) "
	  "from knownGene k, knownIsoforms i, knownCanonical c "
	  "where i.clusterId=%s and i.transcript=k.name "
	  "and c.clusterId=i.clusterId and k.chrom=c.chrom",
	  id->name);
    found |= printBed4FromQueryAndName(conn, query, geneSymbol, outF);
    }
slFreeList(&clusterIds);
return found;
}

boolean findInRefGene(struct sqlConnection *conn, char *geneSymbol, FILE *outF)
/* If there is a refGene for geneSymbol, print output using the
 * refGene coords as {chrom,chromStart,chromEnd} and return TRUE. */
{
char query[1024];
sqlSafef(query, sizeof(query), "select chrom, txStart, txEnd from refGene rg, refLink rl "
      "where rl.name = '%s' and rl.mrnaAcc = rg.name", geneSymbol);
return printBed4FromQueryAndName(conn, query, geneSymbol, outF);
}

boolean findInKgAlias(struct sqlConnection *conn, char *geneSymbol, FILE *outF)
/* If geneSymbol can be found in kgAlias, print output using the
 * knownGene coords as {chrom,chromStart,chromEnd} and return TRUE. */
{
char query[1024];
sqlSafef(query, sizeof(query), "select distinct(chrom), min(txStart), max(txEnd) "
      "from knownGene, kgAlias where alias='%s' and name=kgId "
      "group by chrom", geneSymbol);
return printBed4FromQueryAndName(conn, query, geneSymbol, outF);
}

boolean findInGencode(struct sqlConnection *conn, char *geneSymbol, FILE *outF)
/* If Gencode tables are present and geneSymbol can be found in them,
 * use those coords and return TRUE. */
{
#define PSEUDOGENE_TABLE "wgEncodeGencodePseudoGeneV14"
#define COMP_TABLE "wgEncodeGencodeCompV14"
boolean found = FALSE;
char query[1024];
if (sqlTableExists(conn, PSEUDOGENE_TABLE))
    {
    sqlSafef(query, sizeof(query), "select chrom, txStart, txEnd from %s where name2 = '%s'",
	  PSEUDOGENE_TABLE, geneSymbol);
    found = printBed4FromQueryAndName(conn, query, geneSymbol, outF);
    }
if (!found && sqlTableExists(conn, COMP_TABLE))
    {
    sqlSafef(query, sizeof(query), "select chrom, txStart, txEnd from %s where name2 = '%s'",
	  COMP_TABLE, geneSymbol);
    found = printBed4FromQueryAndName(conn, query, geneSymbol, outF);
    }
return found;
}

void gadPos(char *db, char *outFileName)
/* Try to get genomic positions for GAD gene symbols from knownGene, refGene,
 * kgAlias and Gencode V14 in that order. */
{
FILE *outF = mustOpen(outFileName, "w");
struct sqlConnection *conn = hAllocConn(db);

/* loop over all gene symbols in GAD */	
struct slName *geneSymbols = sqlQuickList(conn,
			       "NOSQLINJ select distinct geneSymbol from gadAll where association='Y'");
struct slName *symbol;
int kcCount = 0, rgCount = 0, kaCount = 0, gcCount = 0, missingCount = 0;
for (symbol = geneSymbols;  symbol != NULL;  symbol = symbol->next)
    {
    if (findInKnownCanonical(conn, symbol->name, outF))
	kcCount++;
    else if (findInRefGene(conn, symbol->name, outF))
	rgCount++;
    else if (findInKgAlias(conn, symbol->name, outF))
	kaCount++;
    else if (findInGencode(conn, symbol->name, outF))
	gcCount++;
    else
	{
	verbose(2, "No result for gene symbol '%s'\n", symbol->name);
	missingCount++;
	}
    }
verbose(1, "Found in knownCanonical: %d\n", kcCount);
verbose(1, "Found in refGene: %d\n", rgCount);
verbose(1, "Found in kgAlias: %d\n", kaCount);
verbose(1, "Found in Gencode: %d\n", gcCount);
verbose(1, "Not found: %d\n", missingCount);

hFreeConn(&conn);
carefulClose(&outF);
}


int main(int argc, char *argv[])
{
optionInit(&argc, argv, optionSpecs);

if (argc != 3) usage();
char *db   = argv[1];
char *outFileName = argv[2];

gadPos(db, outFileName);

return(0);
}

