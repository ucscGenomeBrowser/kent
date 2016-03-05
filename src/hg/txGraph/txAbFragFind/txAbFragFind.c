/* txAbFragFind - Search database for what are probably antibody fragments.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txAbFragFind - Search database for what are probably antibody fragments.\n"
  "usage:\n"
  "   txAbFragFind database outputAccessions\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void txAbFragFind(char *database, char *output)
/* txAbFragFind - Search database for what are probably antibody fragments.. */
{
FILE *f = mustOpen(output, "w");
struct sqlConnection *conn =  sqlConnect(database);
struct sqlResult *sr = sqlGetResult(conn, NOSQLINJ "select id,name from description");
char **row;
struct hash *descriptionIdHash = hashNew(18);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *id = row[0];
    char *description = row[1];
    tolowers(description);
    if (stringIn("immunoglobulin", description) && stringIn("chain", description))
        {
	if (stringIn("variable", description) || stringIn("vdj", description) || 
	    stringIn("vlj", description) || stringIn("vhdj", description) ||
	    stringIn("v region", description) || stringIn("vj region", description) ||
	    stringIn("v-region", description) || stringIn("v-j-c region", description) ||
	    stringIn("v-d-j", description) || stringIn("rearrange", description) ||
	    stringIn("v-d-t region", description) || stringIn("v-j region", description) || 
	    stringIn("v-d-c region", description) || stringIn("kappa chain (igk) mrna, vk", description) ||
	    stringIn("partial mrna for immunoglobulin", description) ||
	    stringIn("nonfunctional immunoglobulin", description) || 
	    stringIn("kappa light chain (igvk-a", description) ||
	    stringIn("heavy chain complementarity-determining region 3", description) ||
	    stringIn("chain mrna, partial cds", description) ||
	    endsWith(description, "immunoglobulin mu heavy chain-like mrna, partial sequence.") ||
	    endsWith(description, "immunoglobulin mu heavy chain mrna, partial sequence.") ||
	    endsWith(description, "nonfunctional immunoglobulin heavy chain mrna, partial sequence.") ||
	    endsWith(description, "fab mrna, partial cds.") ||
	    endsWith(description, "immunoglobulin e heavy chain (ige) mrna, partial cds."))
	    {
	    hashAdd(descriptionIdHash, id, NULL);
	    verbose(2, "adding %s\n", description);
	    }
	else
	    {
	    verbose(2, "skipping %s\n", description);
	    }
	}
    }
sqlFreeResult(&sr);
verbose(1, "Found %d descriptions that match\n", descriptionIdHash->elCount);

int totalCount = 0;
int matchCount = 0;
sr = sqlGetResult(conn, NOSQLINJ "select description,acc from gbCdnaInfo where type='mRNA'");
while ((row = sqlNextRow(sr)) != NULL)
    {
    ++totalCount;
    if (hashLookup(descriptionIdHash, row[0]))
	{
	fprintf(f, "%s\n", row[1]);
	++matchCount;
	}
    }
sqlFreeResult(&sr);
verbose(1, "Matched %d of %d accessions to antibody criteria\n", matchCount, totalCount);

sqlDisconnect(&conn);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
txAbFragFind(argv[1], argv[2]);
return 0;
}
