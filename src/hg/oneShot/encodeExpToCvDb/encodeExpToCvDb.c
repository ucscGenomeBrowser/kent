/* encodeExpToCvDb - Convert encode experiments table to a table more suitable for cvDb. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "obscure.h"
#include "encode/encodeExp.h"
#include "mdb.h"

/* Databases and tables we'll be using. Might move to command line options someday. */
char *cvDb = "kentDjangoTest";
char *expDb = "hgFixed";
char *metaDb = "hg19";
char *expTable = "encodeExp";
char *metaTable = "metaDb";
char *cvDbPrefix = "cvDb_";  /* Prefix to our tables in cvDb database. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encodeExpToCvDb - Convert encode experiments table to a table more suitable for cvDb\n"
  "usage:\n"
  "   encodeExpToCvDb exp.tab exp.django exp.sql\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct field
/* A database field we are tracking. */
    {
    char *name;	      /* Follows rules for C names: alnum, starting with a letter. */
    char *djangoType; /* Snippet of Django code to describe type. */
    };

struct field expRequiredFields[] =
/* Required fields in experiments table. */
    {
    {"id",	    "IntegerField(primary_key=True)"},
    {"updateTime",  "CharField(max_length=40)"},   // replace with dateTime soon I hope
    {"series",      "CharField(max_length=50)"},
    {"accession",   "CharField(unique=True, max_length=16)"},
    {"organism",    "ForeignKey(Organism, db_column='organism')"},
    {"lab",	    "ForeignKey(Lab, db_column='lab')"},
    {"dataType",    "ForeignKey(DataType, db_column='dataType')"},
    {"cellType",    "ForeignKey(CellType, db_column='cellType')"},
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
safef(query, sizeof(query), "select id from %s%s where term = '%s'", cvDbPrefix, table, term);
return sqlQuickNum(conn, query);
}

static char *mdbLookupField(struct mdbObj *mdb, char *field)
/* Lookup a field in a mdbObj. */
{
if (sameString(field, "obj"))
    {
    return mdb->obj;
    }
else
    {
    struct mdbVar *var =  hashFindVal(mdb->varHash, field);
    if (var == NULL)
	return NULL;
    else
	return var->val;
    }
}


struct hash *mdbHashKeyedByExpId(char *database)
/* Return a hash of mdbObjs keyed by the expId field (interpreted as a string 
 * rather than a number).  Note in general there will be multiple
 * values for one key in this hash. */
{
/* Grab list of all metaDb obj. */
struct sqlConnection *conn = sqlConnect(database);
struct mdbObj *mdb, *mdbList = mdbObjsQueryAll(conn, metaTable);
verbose(2, "%d objects in %s.%s\n", slCount(mdbList), database, metaTable);
sqlDisconnect(&conn);

struct hash *hash = hashNew(18);
for (mdb = mdbList; mdb != NULL; mdb = mdb->next)
    {
    char *id = mdbLookupField(mdb, "expId");
    if (id != NULL)
	hashAdd(hash, id, mdb);
    }
return hash;
}

void encodeExpToTab(char *outFile)
/* encodeExpToCvDb - Convert encode experiments table to a table more suitable for cvDb. */
{
struct hash *optHash = optionalFieldsHash();
struct hash *mdbHash = mdbHashKeyedByExpId(metaDb);
verbose(1, "read %d mdb objects from %s.%s\n", mdbHash->elCount, metaDb, metaTable);
struct sqlConnection *expDbConn = sqlConnect(expDb);
struct sqlConnection *cvDbConn = sqlConnect(cvDb);
char query[256];
safef(query, sizeof(query), "select * from %s", expTable);
struct sqlResult *sr = sqlGetResult(expDbConn, query);
FILE *f = mustOpen(outFile, "w");
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    /* Read in database structure. */
    struct encodeExp *ee = encodeExpLoad(row);
    
    /* Much of the data we're processing comes from lists of the form 
     * "a=aVal b=bVal c=cVal." We'll convert these to id's in the appropriate 
     * tables and store the IDs in the optCol array declared below.  */
    int optColCount = ArraySize(expOptionalFields);
    int optCol[optColCount];
    int i;
    for (i=0; i<optColCount; ++i)
	optCol[i] = 0;

    /* Convert var=val string in encodeExp.expVars into list of slPairs, and loop through it. */
    struct slPair *varList = slPairListFromString(ee->expVars, TRUE);
    struct slPair *var;
    for (var = varList; var != NULL; var = var->next)
	 {
	 /* Figure out name of table and the term within that table. */
	 char *table = var->name;
	 char *term = var->val;
	 if (sameString(table, "antibody")) 
	     table = "ab";

	 /* If it looks like we have a valid table and term, store result in
	  * optCol array we'll output soon. */
	 struct hashEl *hel;
	 if ((hel = hashLookup(optHash, table)) != NULL)
	     {
	     int id = lookupId(cvDbConn, table, term);
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

    /* Now we want to process metaDb, which has some info encodeExp does not. */
    char *composite = NULL;
    char ixAsString[16];
    safef(ixAsString, sizeof(ixAsString), "%d", ee->ix);
    struct mdbObj *mdb = hashFindVal(mdbHash, ixAsString);
    if (mdb != NULL)
	{
	struct mdbVar *v;
	for (v = mdb->vars; v != NULL; v = v->next)
	    {
	    /* Look up table and term and change table name if need be */
	    char *table = v->var;
	    char *term = v->val;
	    if (sameString(table, "antibody")) 
		 table = "ab";
	    else if (sameString(table, "grant"))
	         table = "grantee";

	    /* Squirrel away the ever-important composite term for later. */
	    if (sameString("composite", table))
	         composite = term;

	    struct hashEl *hel;
	    if ((hel = hashLookup(optHash, table)) != NULL)
		{
		int optColIx = ptToInt(hel->val);
		if (optCol[optColIx] == 0)  // Only use mdb if encodeExp has no data.
		    {
		    int id = lookupId(cvDbConn, table, term);
		    optCol[optColIx] = id;
		    }
		}
	    }
	}

    /* Write out required fields.  Order of required fields
     * here needs to follow order in expRequiredFields. */
    fprintf(f, "%u", ee->ix);
    fprintf(f, "\t%s", ee->updateTime);
    fprintf(f, "\t%s", composite);
    fprintf(f, "\t%s", emptyForNull(ee->accession));
    fprintf(f, "\t%d", lookupId(cvDbConn, "organism", ee->organism));
    fprintf(f, "\t%d", lookupId(cvDbConn, "lab", ee->lab));
    fprintf(f, "\t%d", lookupId(cvDbConn, "dataType", ee->dataType));
    fprintf(f, "\t%d", lookupId(cvDbConn, "cellType", ee->cellType));

    /* Now write out optional fields. */
    for (i=0; i<optColCount; ++i)
	fprintf(f, "\t%d", optCol[i]);

    /* End output record. */
    fprintf(f, "\n");
    }
carefulClose(&f);
sqlFreeResult(&sr);
sqlDisconnect(&expDbConn);
sqlDisconnect(&cvDbConn);
}

void encodeExpToDjango(char *outFile)
/* Write out Django model declaration for encodeExp into outfile. */
{
FILE *f = mustOpen(outFile, "w");
fprintf(f, "\n");
fprintf(f, "class Experiment(models.Model):\n");
fprintf(f, "    \"\"\"\n");
fprintf(f, "    A defined set of conditions for an experiment.  There may be\n");
fprintf(f, "    multiple replicates of an experiment, but they are all done\n");
fprintf(f, "    under the same conditions.  Often many experiments are done in\n");
fprintf(f, "    a 'Series' under sets of conditions that vary in defined ways\n");
fprintf(f, "    Some experiments may be designated controls for the series.\n ");
fprintf(f, "    \"\"\"\n");
int i;
for (i=1; i<ArraySize(expRequiredFields); ++i)  // Start at one so django makes id field itself
    {
    struct field *field = &expRequiredFields[i];
    fprintf(f, "    %s = models.%s\n", field->name, field->djangoType);
    }
for (i=0; i<ArraySize(expOptionalFields); ++i)
    {
    char *name = expOptionalFields[i];
    fprintf(f, "    %s = models.ForeignKey(%c%s, db_column='%s', blank=True)\n", 
	    name, toupper(name[0]), name+1, name);
    }
fprintf(f, "\n");
fprintf(f, "    class Meta:\n");
fprintf(f, "        db_table = '%s%s'\n", cvDbPrefix, "experiment");
fprintf(f, "\n");
fprintf(f, "    def __unicode__(self):\n");
fprintf(f, "        return self.accession\n");
fprintf(f, "\n");
carefulClose(&f);
}

void encodeExpToCvDb(char *outTab, char *outDjango, char *outSql)
/* encodeExpToCvDb - Convert encode experiments table to a table more suitable for cvDb. */
{
encodeExpToTab(outTab);
encodeExpToDjango(outDjango);
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

