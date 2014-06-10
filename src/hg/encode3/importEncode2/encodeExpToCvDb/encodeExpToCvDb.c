/* encodeExpToCvDb - Convert encode experiments table to a table more suitable for cvDb. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "obscure.h"
#include "encode/encodeExp.h"
#include "mdb.h"

/* Databases and tables we'll be using. Might move to command line options someday. */
char *cvDb = "encode2Meta";
char *expDb = "hgFixed";
char *metaDbs[] = {"hg19", "mm9"};
char *expTable = "encodeExp";
char *metaTable = "metaDb";
char *cvDbPrefix = "";  /* Prefix to our tables in cvDb database. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encodeExpToCvDb - Convert encode experiments table to a table more suitable for cvDb\n"
  "usage:\n"
  "   encodeExpToCvDb exp.tab series.tab results.tab django.py\n"
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
/* Required initial fields in experiments table. */
    {
    {"id",	    "IntegerField(primary_key=True)"},
    {"updateTime",  "CharField(max_length=40)"},   // replace with dateTime soon I hope
    {"series",      "CharField(max_length=50)"},
    {"accession",   "CharField(unique=True, max_length=16)"},
    {"organism",    "ForeignKey(Organism, db_column='organism')"},
    {"lab",	    "ForeignKey(Lab, db_column='lab')"},
    {"dataType",    "ForeignKey(DataType, db_column='dataType')"},
    {"cellType",    "ForeignKey(CellType, db_column='cellType', blank=True, null=True)"},
    };

char *expOptionalFields[] = 
/* Optional later fields in experiment table, and also tables in cvDb. 
 * The order of this array is the same as the order of the fields in experiment table. */
    {
    "age",
    "antibody",
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
sqlSafef(query, sizeof(query), "select id from %s%s where term = '%s'", cvDbPrefix, table, term);
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

struct mdbObj *getMdbList(char *databases[], int databaseCount)
/* Get list of metaDb objects for database. */
{
struct mdbObj *mdbList = NULL;
int i;
for (i=0; i<databaseCount; ++i)
    {
    /* Grab list of all metaDb obj. */
    char *database = databases[i];
    struct sqlConnection *conn = sqlConnect(database);
    struct mdbObj *oneList = mdbObjsQueryAll(conn, metaTable);
    verbose(2, "%d objects in %s.%s\n", slCount(mdbList), database, metaTable);
    mdbList = slCat(mdbList, oneList);
    sqlDisconnect(&conn);
    }
return mdbList;
}

struct hash *mdbHashKeyedByExpId(struct mdbObj *mdbList)
/* Return a hash of mdbObjs keyed by the expId field (interpreted as a string 
 * rather than a number).  Note in general there will be multiple
 * values for one key in this hash. */
{
struct hash *hash = hashNew(18);
struct mdbObj *mdb;
for (mdb = mdbList; mdb != NULL; mdb = mdb->next)
    {
    char *id = mdbLookupField(mdb, "expId");
    if (id != NULL)
	hashAdd(hash, id, mdb);
    }
return hash;
}


struct series
/* Represents a series of experiments of the same type done in the same lab. */
    {
    struct series *next;
    char *name;	    /* Usually something like wgEncodeLabType */
    char *dataType; /* Something like 'ChipSeq' */
    char *grantee;  /* Who owns the series. */
    };

struct series *seriesFromMdb(struct mdbObj *mdb, char *name)
/* Make up a series based on mdb - looping through it's vars looking for ones we want. */
{
struct mdbVar *v;
struct series *series;
AllocVar(series);
series->name = cloneString(name);
for (v = mdb->vars; v != NULL; v = v->next)
    {
    /* Look up table and term and change table name if need be */
    char *var = v->var;
    char *val = v->val;
    if (sameString(var, "dataType")) 
	 series->dataType = cloneString(val);
    else if (sameString(var, "grant"))
         series->grantee = cloneString(val);
    }
return series;
}

void writeSeriesList(char *fileName, struct series *list)
/* Write out list to file in tab separated format, adding initial id field. */
{
FILE *f = mustOpen(fileName, "w");
int id = 0;
struct series *series;
for (series = list; series != NULL; series = series->next)
    {
    fprintf(f, "%d\t%s\t%s\t%s\n", ++id, series->name,
        emptyForNull(series->dataType), emptyForNull(series->grantee));
    }
carefulClose(&f);
}


void writeMdbListAsResults(struct mdbObj *mdbList, char *fileName)
/* Write selected fields in list in tab-separated result format to file. */
{
FILE *f = mustOpen(fileName, "w");
struct mdbObj *mdb;
int id = 0;
for (mdb = mdbList; mdb != NULL; mdb = mdb->next)
    {
    char *experiment = NULL;
    char *replicate = NULL;
    char *objType = NULL;
    char *fileName = NULL;
    char *md5sum = NULL;
    char *tableName = NULL;
    char *view = NULL;
    char *dateSubmitted = NULL;
    char *dateResubmitted = NULL;
    char *dateUnrestricted = NULL;
    struct mdbVar *v;
    for (v = mdb->vars; v != NULL; v = v->next)
	{
	char *var = v->var, *val = v->val;
	if (sameString(var, "expId"))
	    experiment = val;
	else if (sameString(var, "replicate"))
	    replicate = val;
	else if (sameString(var, "objType"))
	    objType = val;
	else if (sameString(var, "fileName"))
	    {
	    fileName = val;
	    /* Do surgery on file name, cutting off second comma separated index file. */
	    char *comma = strchr(fileName, ',');
	    if (comma != NULL)
	        *comma = 0;
	    }
	else if (sameString(var, "md5sum"))
	    {
	    md5sum = val;
	    char *comma = strchr(fileName, ',');
	    if (comma != NULL)
	        *comma = 0;
	    }
	else if (sameString(var, "tableName"))
	    tableName = val;
	else if (sameString(var, "view"))
	    view = val;
	else if (sameString(var, "dateSubmitted"))
	    dateSubmitted = val;
	else if (sameString(var, "dateResubmitted"))
	    dateResubmitted = val;
	else if (sameString(var, "dateUnrestricted"))
	    dateUnrestricted = val;
	}
    if (objType != NULL && !sameString(objType, "composite"))
	{
	if (experiment != NULL)
	    {
	    fprintf(f, "%d\t", ++id);
	    fprintf(f, "%s\t", emptyForNull(experiment));
	    fprintf(f, "%s\t", emptyForNull(replicate));
	    fprintf(f, "%s\t", emptyForNull(view));
	    fprintf(f, "%s\t", emptyForNull(objType));
	    fprintf(f, "%s\t", emptyForNull(fileName));
	    fprintf(f, "%s\t", emptyForNull(md5sum));
	    fprintf(f, "%s\t", emptyForNull(tableName));
	    fprintf(f, "%s\t", emptyForNull(dateSubmitted));
	    fprintf(f, "%s\t", emptyForNull(dateResubmitted));
	    fprintf(f, "%s\n", emptyForNull(dateUnrestricted));
	    }
	else
	    {
	    if (!startsWith("wgEncodeUmassWengTfbsValid", mdb->obj))	// These validation files not associated with encode experiment
		warn("No experiment for %s", mdb->obj);
	    }
	}
    }
carefulClose(&f);
}

void encodeExpToTab(char *outExp, char *outSeries, char *outResults)
/* encodeExpToCvDb - Convert encode experiments table to a table more suitable for cvDb. */
{
struct hash *optHash = optionalFieldsHash();
struct mdbObj *mdbList = getMdbList(metaDbs, ArraySize(metaDbs));
struct hash *mdbHash = mdbHashKeyedByExpId(mdbList);
struct hash *seriesHash = hashNew(0);
struct series *seriesList = NULL;
verbose(1, "read %d mdb objects from %s in %d databases\n", mdbHash->elCount, metaTable, 
    (int)ArraySize(metaDbs));
struct sqlConnection *expDbConn = sqlConnect(expDb);
struct sqlConnection *cvDbConn = sqlConnect(cvDb);
char query[256];
sqlSafef(query, sizeof(query), "select * from %s", expTable);
struct sqlResult *sr = sqlGetResult(expDbConn, query);
FILE *f = mustOpen(outExp, "w");
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
	 if (sameString(table, "antibody")) // Deal with antibody special case
	    {
	    if (sameString(term, "Control") || sameString(term, "Input") 
	    || sameString(term, "RevXlinkChromatin") || sameString(term, "ripInput"))
		{
		table = "control";
		}
	    }

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

    /* If we've got a composite, then make up a series record. */
    if (composite != NULL)
        {
	assert(mdb != NULL);
	struct series *series = hashFindVal(seriesHash, composite);
	if (series == NULL)
	    {
	    series = seriesFromMdb(mdb, composite);
	    hashAdd(seriesHash, composite, series);
	    slAddHead(&seriesList, series);
	    }
	}

    if (ee->accession != NULL)
	{
	/* Write out required fields.  Order of required fields
	 * here needs to follow order in expRequiredFields. */
	fprintf(f, "%u", ee->ix);
	fprintf(f, "\t%s", ee->updateTime);
	fprintf(f, "\t%s", naForNull(composite));
	fprintf(f, "\t%s", ee->accession);
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
    }

/* Write out series list to a separate file. */
slReverse(&seriesList);
writeSeriesList(outSeries, seriesList);

/* Write out results to a separate file. */
writeMdbListAsResults(mdbList, outResults);

/* Clean up and go home. */
carefulClose(&f);
sqlFreeResult(&sr);
sqlDisconnect(&expDbConn);
sqlDisconnect(&cvDbConn);
}

void encodeExpToDjango(char *outFile)
/* Write out Django model declaration for encodeExp into outfile. */
{
FILE *f = mustOpen(outFile, "w");

/* Write series model. */
fprintf(f, "\n");
fprintf(f, "class Series(models.Model):\n");
fprintf(f, "    \"\"\"\n");
fprintf(f, "    Represents a series of experiments of the same type done for\n");
fprintf(f, "    the same project.\n");
fprintf(f, "    \"\"\"\n");
fprintf(f, "    term = models.CharField(max_length=50)\n");
fprintf(f, "    dataType = models.CharField(max_length=40)\n");
fprintf(f, "    grantee = models.CharField(max_length=255)\n");
fprintf(f, "\n");
fprintf(f, "    class Meta:\n");
fprintf(f, "        db_table = '%s%s'\n", cvDbPrefix, "series");
fprintf(f, "\n");
fprintf(f, "    def __unicode__(self):\n");
fprintf(f, "        return self.term\n");
fprintf(f, "\n");

/* Write experiment model. */
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
    fprintf(f, "    %s = models.ForeignKey(%c%s, db_column='%s', blank=True, null=True)\n", 
	    name, toupper(name[0]), name+1, name);
    }
fprintf(f, "\n");
fprintf(f, "    class Meta:\n");
fprintf(f, "        db_table = '%s%s'\n", cvDbPrefix, "experiment");
fprintf(f, "\n");
fprintf(f, "    def __unicode__(self):\n");
fprintf(f, "        return self.accession\n");
fprintf(f, "\n");


/* Write results model. */
fprintf(f, "\n");
fprintf(f, "class Result(models.Model):\n");
fprintf(f, "    \"\"\"\n");
fprintf(f, "    A result of an experiment - generally either a data file or a\n");
fprintf(f, "    database table. Intermediate as well as final results may be found\n");
fprintf(f, "    here.  Some results may be replicated a number of times\n");
fprintf(f, "    \"\"\"\n");
fprintf(f, "    experiment = models.ForeignKey(Experiment, db_column='experiment')\n");
fprintf(f, "    replicate = models.CharField(max_length=50, blank=True)\n");
fprintf(f, "    view = models.CharField(max_length=50)\n");
fprintf(f, "    objType = models.CharField(max_length=50)\n");
fprintf(f, "    fileName = models.CharField(max_length=255)\n");
fprintf(f, "    md5sum = models.CharField(max_length=255)\n");
fprintf(f, "    tableName = models.CharField(max_length=100, blank=True)\n");
fprintf(f, "    dateSubmitted = models.CharField(max_length=40)\n");
fprintf(f, "    dateResubmitted = models.CharField(max_length=40, blank=True)\n");
fprintf(f, "    dateUnrestricted = models.CharField(max_length=40)\n");
fprintf(f, "\n");
fprintf(f, "    class Meta:\n");
fprintf(f, "        db_table = '%s%s'\n", cvDbPrefix, "results");
fprintf(f, "\n");
fprintf(f, "    def __unicode__(self):\n");
fprintf(f, "        return self.fileName\n");
fprintf(f, "\n");

carefulClose(&f);
}

void encodeExpToCvDb(char *outExp, char *outSeries, char *outResults, char *outDjango)
/* encodeExpToCvDb - Convert encode experiments table to a table more suitable for cvDb. */
{
encodeExpToTab(outExp, outSeries, outResults);
encodeExpToDjango(outDjango);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
encodeExpToCvDb(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

