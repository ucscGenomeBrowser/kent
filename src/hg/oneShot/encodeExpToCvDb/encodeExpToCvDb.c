/* encodeExpToCvDb - Convert encode experiments table to a table more suitable for cvDb. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "obscure.h"
#include "encode/encodeExp.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encodeExpToCvDb - Convert encode experiments table to a table more suitable for cvDb\n"
  "usage:\n"
  "   encodeExpToCvDb db expTable outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

char *expRequiredFields[] =
/* Required fields in experiments table. */
    {
    "id",	  /* Not a table, just a field. */
    "updateTime", /* Also just a field. */
    "accession",  /* Also just a field. */
    "organism",	  /* Required field. */
    "lab",	  /* Required field. */
    "dataType",	  /* Required field. */
    "cellType",	  /* Required field. */
    };

char *expOptionalFields[] = 
/* Fields in experiment table, and for the most part also tables in cvDb. 
 * The order of this array is the same as the order of the fields in experiment table. */
    {
    "ab",
    "age",
    "attic",
    "category",
    "control",
    "fragSize",
    "grantee",
    "insertLength",
    "localization",
    "mapAlgorithm",
    "objStatus",
    "phase",
    "platform",
    "promoter",
    "protocol",
    "readType",
    "region",
    "restrictionEnzyme",
    "rnaExtract",
    "seqPlatform",
    "sex",
    "strain",
    "tissueSourceType",
    "treatment",
    "typeOfTerm",
    "version",
    "view",
    };

struct hash *optionalFieldsHash()
/* Return hash of terms we have tables for. */
{
struct hash *hash = hashNew(8);
int i;
for (i=0; i<ArraySize(expOptionalFields); ++i)
    hashAdd(hash, expOptionalFields[i], intToPt(i));
return hash;
}

int lookupId(struct sqlConnection *conn, char *table, char *term)
/* Return ID of term in table. */
{
char query[512];
safef(query, sizeof(query), "select id from cvDb_%s where term = '%s'", table, term);
return sqlQuickNum(conn, query);
}

void encodeExpToCvDb(char *db, char *expTable, char *outDir)
/* encodeExpToCvDb - Convert encode experiments table to a table more suitable for cvDb. */
{
struct hash *optHash = optionalFieldsHash();
struct sqlConnection *conn = sqlConnect(db), *conn2 = sqlConnect("kentDjangoTest");
char query[256];
safef(query, sizeof(query), "select * from %s", expTable);
struct sqlResult *sr = sqlGetResult(conn, query);
FILE *f = mustOpen(outDir, "w");
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    /* Read in database structure and print out required fields.  Order of required fields
     * here needs to follow order in expRequiredFields. */
    struct encodeExp *ee = encodeExpLoad(row);
    fprintf(f, "%u", ee->ix);
    fprintf(f, "\t%s", ee->updateTime);
    fprintf(f, "\t%s", emptyForNull(ee->accession));
    fprintf(f, "\t%d", lookupId(conn2, "organism", ee->organism));
    fprintf(f, "\t%d", lookupId(conn2, "lab", ee->lab));
    fprintf(f, "\t%d", lookupId(conn2, "dataType", ee->dataType));
    fprintf(f, "\t%d", lookupId(conn2, "cellType", ee->cellType));

    /* Now we'll want to store optional fields,  which are contained in ee->expVars
     * as var=val pairs. We'll convert these to id's in the appropriate tables,
     * and store the IDs in the optCol array declared below.  Once we loop through
     * all the var=val pairs we'll write out the array. */
    int optColCount = ArraySize(expOptionalFields);
    int optCol[optColCount];
    int i;
    for (i=0; i<optColCount; ++i)
	optCol[i] = 0;

    /* Convert var=val string into list of slPairs, and loop through it. */
    struct slPair *varList = slPairListFromString(ee->expVars, TRUE);
    struct slPair *var;
    for (var = varList; var != NULL; var = var->next)
	 {
	 char *table = var->name;
	 char *term = var->val;
	 if (sameString(table, "antibody")) 
	     table = "ab";
	 struct hashEl *hel;
	 if ((hel = hashLookup(optHash, table)) != NULL)
	     {
	     int id = lookupId(conn2, table, term);
	     if (id == 0)
		  {
	          warn("No id in cvDb for %s=%s\n", table, term);
		  continue;
		  }
	     int optColIx = ptToInt(hel->val);
	     optCol[optColIx] = id;
	     }
	 else
	     verbose(2, "%s %s ?\n", table, term);
	 }

    /* Now write out optional values. */
    for (i=0; i<optColCount; ++i)
	fprintf(f, "\t%d", optCol[i]);
    fprintf(f, "\n");
    }
carefulClose(&f);
sqlFreeResult(&sr);
sqlDisconnect(&conn);
sqlDisconnect(&conn2);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
encodeExpToCvDb(argv[1], argv[2], argv[3]);
return 0;
}

