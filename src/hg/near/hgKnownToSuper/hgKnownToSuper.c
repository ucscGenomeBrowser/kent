/* hgKnownToSuper - Load knownToSuperfamily table. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"
#include "hgRelate.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgKnownToSuper - Load knownToSuperfamily table\n"
  "usage:\n"
  "   hgKnownToSuper database org supFamAss.tab\n"
  "Where 'org' is a two letter abbreviation for the\n"
  "organism - hs for human, mm for mouse, rn for rat\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

char *tempDir = ".";

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *hashTwoColumn(struct sqlConnection *conn, char *query, boolean chop)
/* Make hash based on knownToEnsembl. */
{
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(16);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (chop)
        {
	chopSuffix(row[0]);
	chopSuffix(row[1]);
	}
    hashAdd(hash, row[0], cloneString(row[1]));
    }
sqlFreeResult(&sr);
return hash;
}


struct hash *ensPepToKnown(struct sqlConnection *conn, boolean chopVersion)
/* Create hash keyed by ensembl peptide with knownGene ID value. */
{
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(16);
struct hash *tnToPep = 
	hashTwoColumn(conn, NOSQLINJ "select transcript,protein from ensGtp", chopVersion);
sr = sqlGetResult(conn, NOSQLINJ "select name,value from knownToEnsembl");
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *protein;
    
    if (chopVersion)
        chopSuffix(row[1]);
    protein = hashFindVal(tnToPep, row[1]);
    if (protein != NULL)
        hashAdd(hash, protein, cloneString(row[0]));
    }
sqlFreeResult(&sr);
freeHashAndVals(&tnToPep);
return hash;
}

void createTable(struct sqlConnection *conn, char *tableName)
/* Create our name/value table, dropping if it already exists. */
{
struct dyString *dy = dyStringNew(0);
sqlDyStringPrintf(dy, 
"CREATE TABLE %s (\n"
"    gene varchar(255) not null,        # Known gene ID\n"
"    superfamily int not null,  # Superfamily ID\n"
"    start int not null,        # Start of superfamily domain in protein (0 based)\n"
"    end int not null,  # End (noninclusive) of superfamily domain\n"
"    eVal float not null,       # E value of superfamily assignment\n"
"              #Indices\n"
"    INDEX(gene(10)),\n"
"    INDEX(superfamily)\n"
")\n",  tableName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
}


void hgKnownToSuper(char *database, char *org, char *assFile)
/* hgKnownToSuper - Load knownToSuperfamily table. */
{
struct sqlConnection *conn = sqlConnect(database);
struct hash *pepToKnown = ensPepToKnown(conn, TRUE);
char *table = "knownToSuper";
FILE *f = hgCreateTabFile(tempDir, table);
struct lineFile *lf = lineFileOpen(assFile, TRUE);
boolean gotOrg = FALSE;
int outCount = 0;
char *row[6];

while (lineFileRow(lf, row))
    {
    if (sameString(row[0], org))
        {
	char *pepName = row[1];
	char *regions = row[3];
	char *eVal = row[4];
	char *supId = row[5];
	char *knownId = hashFindVal(pepToKnown, pepName);
	if (knownId != NULL)
	    {
	    char *region, *e;
	    int start,end;
	    /* Loop through comma-separated region string. */
	    for (region = regions; region != NULL; region = e)
	        {
		e = strchr(region, ',');
		if (e != NULL)
		    {
		    *e++ = 0;
		    if (e[0] == 0)
		        e = NULL;
		    }
		if (sscanf(region, "%d-%d", &start, &end) < 2)
		    errAbort("bad region %s line %d of %s", region, 
		    	lf->lineIx, lf->fileName);
		fprintf(f, "%s\t%s\t%d\t%d\t%s\n",
			knownId, supId, start-1, end, eVal);
		++outCount;
		}
	    }
	gotOrg = TRUE;
	}
    }
lineFileClose(&lf);
if (!gotOrg)
    errAbort("Looks like '%s' is not a recognized organism", org);
if (outCount <= 0)
    errAbort("No good records found in %s", assFile);
printf("%d records output\n", outCount);

/* Refresh connection in case things took a while. */
sqlDisconnect(&conn);
conn = sqlConnect(database);

/* Load up database. */
createTable(conn, table);
hgLoadTabFile(conn, tempDir, table, &f);
hgRemoveTabFile(tempDir, table);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
hgKnownToSuper(argv[1], argv[2], argv[3]);
return 0;
}
