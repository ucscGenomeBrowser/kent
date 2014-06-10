/* joinerCheck - Parse and check joiner file. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"
#include "jksql.h"
#include "joiner.h"


/* Variable that are set from command line. */
char *fieldListIn;
char *fieldListOut;
char *identifier;
char *database;
boolean foreignKeys;	/* "keys" command line variable. */
boolean checkTimes;	/* "times" command line variable. */
boolean checkFields;	/* "fields" command line variable. */
boolean tableCoverage;
boolean dbCoverage;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "joinerCheck - Parse and check joiner file\n"
  "usage:\n"
  "   joinerCheck file.joiner\n"
  "options:\n"
  "   -fields - Check fields in joiner file exist, faster with -fieldListIn\n"
  "      -fieldListOut=file - List all fields in all databases to file.\n"
  "      -fieldListIn=file - Get list of fields from file rather than mysql.\n"
  "   -keys - Validate (foreign) keys.  Takes about an hour.\n"
  "   -tableCoverage - Check that all tables are mentioned in joiner file\n"
  "   -dbCoverage - Check that all databases are mentioned in joiner file\n"
  "   -times - Check update times of tables are after tables they depend on\n"
  "   -all - Do all tests: -fields -keys -tableCoverage -dbCoverage -times\n"
  "   -identifier=name - Just validate given identifier.\n"
  "                    Note only applies to keys and fields checks.\n"
  "   -database=name - Just validate given database.\n"
  "                    Note only applies to keys and times checks.\n"
  "   -verbose=N - use verbose to diagnose difficulties. N = 2, 3 or 4 to\n"
  "              - show increasing level of detail for some functions."
  );
}

static struct optionSpec options[] = {
   {"fieldListIn", OPTION_STRING},
   {"fieldListOut", OPTION_STRING},
   {"identifier", OPTION_STRING},
   {"database", OPTION_STRING},
   {"keys", OPTION_BOOLEAN},
   {"times", OPTION_BOOLEAN},
   {"fields", OPTION_BOOLEAN},
   {"tableCoverage", OPTION_BOOLEAN},
   {"dbCoverage", OPTION_BOOLEAN},
   {"all", OPTION_BOOLEAN},
   {NULL, 0},
};

/* Other globals */
struct hash *allDbHash;

static void printField(struct joinerField *jf, FILE *f)
/* Print out field info to f. */
{
struct slName *db;
for (db = jf->dbList; db != NULL; db = db->next)
    {
    if (db != jf->dbList)
        fprintf(f, ",");
    fprintf(f, "%s", db->name);
    }
fprintf(f, ".%s.%s", jf->table, jf->field);
}

void checkOneDependency(struct joiner *joiner,
	struct joinerDependency *dep, struct sqlConnection *conn, char *dbName)
/* Check out one dependency in one database. */
{
char *tableToCheck = dep->table->table;
if (sqlWildcardIn(tableToCheck))
    {
    errAbort("Can't handle wildCards in dependency tables line %d of %s",
    	dep->lineIx, joiner->fileName);
    }
if (slNameInList(dep->table->dbList, dbName) 
	&& sqlTableExists(conn, tableToCheck))
    {
    time_t tableTime = sqlTableUpdateTime(conn, tableToCheck);
    struct joinerTable *dependsOn;
    for (dependsOn = dep->dependsOnList; dependsOn != NULL; 
	dependsOn = dependsOn->next)
	{
	if (slNameInList(dependsOn->dbList, dbName))
	    {
	    if (!sqlTableExists(conn, dependsOn->table))
		{
		warn("Error: %s.%s doesn't exist line %d of %s",
		    dbName, dependsOn->table, 
		    dep->lineIx, joiner->fileName);
		}
	    else
		{
		time_t depTime = sqlTableUpdateTime(conn, dependsOn->table);
		if (depTime > tableTime)
		    {
		    char *depStr = sqlUnixTimeToDate(&depTime, FALSE);
		    char *tableStr = sqlUnixTimeToDate(&tableTime, FALSE);
		
		    warn("Error: %s.%s updated after %s.%s line %d of %s",
			dbName, dependsOn->table, dbName, tableToCheck,
			dep->lineIx, joiner->fileName);
		    warn("\t%s vs. %s", depStr, tableStr);
		    freeMem(depStr);
		    freeMem(tableStr);
		    }
		}
	    }
	}
    }
}

void joinerCheckDependencies(struct joiner *joiner, char *specificDb)
/* Check time stamps on dependent tables. */
{
struct hashEl *db, *dbList = hashElListHash(joiner->databasesChecked);
for (db = dbList; db != NULL; db = db->next)
    {
    if (specificDb == NULL || sameString(specificDb, db->name))
        {
	struct sqlConnection *conn = sqlMayConnect(db->name);
	if (conn != NULL)	/* We've already warned on this NULL */
	    {
	    struct joinerDependency *dep;
	    for (dep = joiner->dependencyList; dep != NULL; dep = dep->next)
	        {
		checkOneDependency(joiner, dep, conn, db->name);
		}
	    sqlDisconnect(&conn);
	    }
	}
    }
slFreeList(&dbList);
}

struct slName *getTablesForField(struct sqlConnection *conn, 
	char *splitPrefix, char *table, char *splitSuffix)
/* Get tables that match field. */
{
struct slName *list = NULL, *el;
if (splitPrefix != NULL || splitSuffix != NULL)
    {
    char query[256], **row;
    struct sqlResult *sr;
    sqlSafef(query, sizeof(query), "show tables like '%s%s%s'", 
    	emptyForNull(splitPrefix), table, emptyForNull(splitSuffix));
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	el = slNameNew(row[0]);
	slAddHead(&list, el);
	}
    sqlFreeResult(&sr);
    slReverse(&list);
    }
if (list == NULL)
    {
    if (sqlTableExists(conn, table))
	list = slNameNew(table);
    }
return list;
}

static boolean fieldExists(struct hash *fieldHash,
	struct joinerSet *js, struct joinerField *jf)
/* Make sure field exists in at least one database. */
{
struct slName *db;
boolean gotIt = FALSE;
for (db = jf->dbList; db != NULL && !gotIt; db = db->next)
    {
    if (hashLookup(allDbHash, db->name))
	{
	struct sqlConnection *conn = sqlConnect(db->name);
	struct slName *table, *tableList = getTablesForField(conn,
			    jf->splitPrefix, jf->table, jf->splitSuffix);
	char fieldName[512];
	sqlDisconnect(&conn);
	for (table = tableList; table != NULL; table = table->next)
	    {
	    safef(fieldName, sizeof(fieldName), "%s.%s.%s", 
		db->name, table->name, jf->field);
	    if (hashLookup(fieldHash, fieldName))
		{
		gotIt = TRUE;
		break;
		}
	    }
	slFreeList(&tableList);
	}
    else
        {
	/* Warn that database doesn't exist.  Just warn once per database. */
	static struct hash *uniqHash;
	if (uniqHash == NULL) 
	    uniqHash = hashNew(8);
	if (!hashLookup(uniqHash, db->name))
	    {
	    hashAdd(uniqHash, db->name, NULL);
	    warn("Database %s doesn't exist", db->name);
	    }
	}
    }
return gotIt;
}

static void joinerValidateFields(struct joiner *joiner, struct hash *fieldHash,
	char *oneIdentifier)
/* Make sure that joiner refers to fields that exist at
 * least somewhere. */
{
struct joinerSet *js;
struct joinerField *jf;

for (js=joiner->jsList; js != NULL; js = js->next)
    {
    if (oneIdentifier == NULL || wildMatch(oneIdentifier, js->name))
	{
	for (jf = js->fieldList; jf != NULL; jf = jf->next)
	    {
	    if (!fieldExists(fieldHash, js, jf))
		 {
		 if (!js->expanded)
		     {
		     fprintf(stderr, "Error: ");
		     printField(jf, stderr);
		     fprintf(stderr, " not found in %s line %d of %s\n",
			js->name, jf->lineIx, joiner->fileName);
		     }
		 }
	    }
	}
    }
}

struct slName *jsAllDb(struct joinerSet *js)
/* Get list of all databases referred to by set. */
{
struct slName *list = NULL, *db;
struct joinerField *jf;
for (jf = js->fieldList; jf != NULL; jf = jf->next)
    {
    for (db = jf->dbList; db != NULL; db = db->next)
	slNameStore(&list, db->name);
    }
return list;
}

int sqlTableRows(struct sqlConnection *conn, char *table)
/* REturn number of rows in table. */
{
char query[256];
sqlSafef(query, sizeof(query), "select count(*) from %s", table);
return sqlQuickNum(conn, query);
}

int totalTableRows(struct sqlConnection *conn, struct slName *tableList)
/* Return total number of rows in all tables in list. */
{
int rowCount = 0;
struct slName *table;
for (table = tableList; table != NULL; table = table->next)
    {
    rowCount += sqlTableRows(conn, table->name);
    }
return rowCount;
}

struct sqlConnection *sqlWarnConnect(char *db)
/* Connect to database, or warn and return NULL. */
{
struct sqlConnection *conn = sqlMayConnect(db);
if (conn == NULL)
    warn("Error: Couldn't connect to database %s", db);
return conn;
}

static char *doChopsAndUpper(struct joinerField *jf, char *id)
/* Return chopped version of id.  (This may insert a zero into s).
 * Also upper case it. */
{
struct slName *chop;
for (chop = jf->chopBefore; chop != NULL; chop = chop->next)
    {
    char *s = stringIn(chop->name, id);
    if (s != NULL)
	 id = s + strlen(chop->name);
    }
for (chop = jf->chopAfter; chop != NULL; chop = chop->next)
    {
    char *s = rStringIn(chop->name, id);
    if (s != NULL)
	*s = 0;
    }
touppers(id);
return id;
}

struct keyHitInfo
/* Information on how many times a key is hit. */
    {
    struct keyHitInfo *next;
    char *name;	/* Name - not allocated here. */
    int count;	/* Hit count. */
    };

void keyHitInfoClear(struct keyHitInfo *khiList)
/* Clear out counts in list. */
{
struct keyHitInfo *khi;
for (khi = khiList; khi != NULL; khi = khi->next)
    khi->count = 0;
}

struct hash *readKeyHash(char *db, struct joiner *joiner, 
	struct joinerField *keyField, struct keyHitInfo **retList)
/* Read key-field into hash.  Check for dupes if need be. */
{
struct sqlConnection *conn = sqlWarnConnect(db);
struct hash *keyHash = NULL;
struct keyHitInfo *khiList = NULL, *khi;
if (conn == NULL)
    {
    return NULL;
    }
else
    {
    struct slName *table;
    struct slName *tableList = getTablesForField(conn,keyField->splitPrefix,
    						 keyField->table, 
						 keyField->splitSuffix);
    int rowCount = totalTableRows(conn, tableList);
    int hashSize = digitsBaseTwo(rowCount)+1;
    char query[256], **row;
    struct sqlResult *sr;
    int itemCount = 0;
    int dupeCount = 0;
    char *dupe = NULL;

    if (rowCount > 0)
	{
	if (hashSize > hashMaxSize)
	    hashSize = hashMaxSize;
	keyHash = hashNew(hashSize);
	for (table = tableList; table != NULL; table = table->next)
	    {
	    sqlSafef(query, sizeof(query), "select %s from %s", 
		keyField->field, table->name);
	    sr = sqlGetResult(conn, query);
	    while ((row = sqlNextRow(sr)) != NULL)
		{
		char *id = doChopsAndUpper(keyField, row[0]);
		if (hashLookup(keyHash, id))
		    {
		    if (keyField->unique)
			{
			if (keyField->exclude == NULL || 
				!slNameInList(keyField->exclude, id))
			    {
			    if (dupeCount == 0)
				dupe = cloneString(id);
			    ++dupeCount;
			    }
			}
		    }
		else
		    {
		    AllocVar(khi);
		    hashAddSaveName(keyHash, id, khi, &khi->name);
		    slAddHead(&khiList, khi);
		    ++itemCount;
		    }
		}
	    sqlFreeResult(&sr);
	    }
	if (dupe != NULL)
	    {
	    warn("Error: %d duplicates in %s.%s.%s including '%s'",
		    dupeCount, db, keyField->table, keyField->field, dupe);
	    freez(&dupe);
	    }
	verbose(2, " %s.%s.%s - %d unique identifiers\n", 
		db, keyField->table, keyField->field, itemCount);
	}
    slFreeList(&tableList);
    }
sqlDisconnect(&conn);
*retList = khiList;
return keyHash;
}

static void addHitMiss(struct hash *keyHash, char *id, struct joinerField *jf,
	int *pHits, char **pMiss, int *pTotal)
/* Record info about one hit or miss to keyHash */
{
id = doChopsAndUpper(jf, id);
if (jf->exclude == NULL || !slNameInList(jf->exclude, id))
    {
    struct keyHitInfo *khi;
    if ((khi = hashFindVal(keyHash, id)) != NULL)
	{
	*pHits += 1;
	khi->count += 1;
	}
    else
	{
	if (*pMiss == NULL)
	    *pMiss = cloneString(id);
	}
    *pTotal += 1;
    }
}

void checkUniqueAndFull(struct joiner *joiner, struct joinerSet *js, 
	char *db, struct joinerField *jf, struct joinerField *keyField,
	struct keyHitInfo *khiList)
/* Make sure that unique and full fields really are so. */
{
struct keyHitInfo *khi;
int keyCount = 0, missCount = 0, doubleCount = 0;
char *missExample = NULL, *doubleExample = NULL;

for (khi = khiList; khi != NULL; khi = khi->next)
    {
    ++keyCount;
    if (khi->count == 0)
	 {
         ++missCount;
	 missExample = khi->name;
	 }
    else if (khi->count > 1)
	{
        ++doubleCount;
	doubleExample = khi->name;
	}
    }
if (jf->full && missCount != 0)
    warn("Error: %d of %d elements (%2.3f%%) of key %s.%s are not in 'full' %s.%s.%s "
	 "line %d of %s\n"
	 "    Example miss: %s"
	, missCount, keyCount
	, (100.0 * missCount) / keyCount
	, keyField->table, keyField->field
	, db, jf->table, jf->field
	, jf->lineIx, joiner->fileName, missExample);
if (jf->unique && doubleCount != 0)
    warn("Error: %d of %d elements (%2.3f%%) of %s.%s.%s are not unique line %d of %s\n"
	 "    Example: %s"
	, doubleCount, keyCount
	, (100.0 * doubleCount) / keyCount
	, db, jf->table, jf->field
	, jf->lineIx, joiner->fileName, doubleExample);
}

void doKeyChecks(char *db, struct joiner *joiner, struct joinerSet *js, 
	struct hash *keyHash, struct keyHitInfo *khiList,
	struct joinerField *keyField, struct joinerField *jf)
/* Check that at least minimum is covered, and that full and
 * unique tags are respected. */
{
struct sqlConnection *conn = sqlMayConnect(db);
if (conn != NULL)
    {
    boolean okFlag = FALSE;
    int total = 0, hits = 0, hitsNeeded;
    char *miss = NULL;
    struct slName *table;
    struct slName *tableList = getTablesForField(conn,jf->splitPrefix,
    						 jf->table, jf->splitSuffix);
    keyHitInfoClear(khiList);
    for (table = tableList; table != NULL; table = table->next)
	{
	char query[256], **row;
	struct sqlResult *sr;
	sqlSafef(query, sizeof(query), "select %s from %s", 
		jf->field, table->name);
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    if (jf->separator == NULL)
		{
		addHitMiss(keyHash, row[0], jf, &hits, &miss, &total);
		}
	    else 
	        {
		/* Do list. */
		struct slName *el, *list;
		int ix;
		list = slNameListFromString(row[0], jf->separator[0]);
		for (el = list, ix=0; el != NULL; el = el->next, ++ix)
		    {
		    char *id = el->name;
		    char buf[16];
		    if (jf->indexOf)
			{
		        safef(buf, sizeof(buf), "%d", ix);
			id = buf;
			}
		    addHitMiss(keyHash, id, jf, &hits, &miss, &total);
		    }
		slFreeList(&list);
		}
	    }
	sqlFreeResult(&sr);
	}
    if (tableList != NULL)
	{
	okFlag = FALSE;
	if (hits==total) okFlag = TRUE;
	if (jf->minCheck < 0.999999)	/* Control for rounding error */
	    hitsNeeded = round(total * jf->minCheck);
	else
	    hitsNeeded = total;
	if (jf->minCheck < 1.0 && hits >= hitsNeeded) okFlag = TRUE;
	verbose(1, " %s.%s.%s - hits %d of %d (%2.3f%%)%s\n", db, jf->table, jf->field, hits, total,
		(100.0 * hits) / total,
		okFlag ? " ok" : js->isFuzzy ? " fuzzy" : "");
	if (hits < hitsNeeded && !js->isFuzzy)
	    {
	    warn("Error: %d of %d elements (%2.3f%%) of %s.%s.%s are not in key %s.%s "
		 "line %d of %s\n"
		 "Example miss: %s"
		, total - hits, total, (100.0 * (total-hits))/total, db, jf->table, jf->field
		, keyField->table, keyField->field
		, jf->lineIx, joiner->fileName, miss);
	    }

	if (jf->unique || jf->full)
	    checkUniqueAndFull(joiner, js, db, jf, keyField, khiList);
	}
    freez(&miss);
    slFreeList(&tableList);
    sqlDisconnect(&conn);
    }
}

boolean dbInAnyField(struct joinerField *fieldList, char *db)
/* Return TRUE if database is used in any field. */
{
struct joinerField *field;
for (field = fieldList; field != NULL; field = field->next)
    {
    if (slNameInList(field->dbList, db))
        return TRUE;
    }
return FALSE;
}

void jsValidateKeysOnDb(struct joiner *joiner, struct joinerSet *js,
	char *db, struct hash *keyHash, struct keyHitInfo *khiList)
/* Validate keys pertaining to this database. */
{
struct joinerField *jf;
struct hash *localKeyHash = NULL;
struct keyHitInfo *localKhiList = NULL;
struct joinerField *keyField;

if ((keyField = js->fieldList) == NULL)
    return;
if (keyHash == NULL)
    {
    char *keyDb;
    if (slNameInList(keyField->dbList, db))
	{
	keyDb = db;
	}
    else if (!dbInAnyField(js->fieldList->next, db))
        {
	return;
	}
    else
	{
	if (slCount(keyField->dbList) == 1)
	    keyDb = keyField->dbList->name;
	else
	    {
	    struct slName *lastDb = slLastEl(keyField->dbList);
	    keyDb = lastDb->name;
	    verbose(1, " note - using %s database as reference for identifier %s\n",
	    	keyDb, js->name);
	    }
	}
    keyHash = localKeyHash = readKeyHash(keyDb, joiner, keyField, 
    	&localKhiList);
    khiList = localKhiList;
    }
if (keyHash != NULL)
    {
    for (jf = js->fieldList; jf != NULL; jf = jf->next)
	{
	if (jf != keyField && slNameInList(jf->dbList, db))
	    {
	    doKeyChecks(db, joiner, js, keyHash, khiList, keyField, jf);
	    }
	}
    }
hashFree(&localKeyHash);
slFreeList(&localKhiList);
}

void jsValidateKeys(struct joiner *joiner, struct joinerSet *js, 
	char *preferredDb)
/* Validate keys on js.  If preferredDb is non-NULL then do it on
 * that database.  Otherwise do it on all databases. */
{
struct joinerField *keyField;
struct hash *keyHash = NULL;
struct keyHitInfo *khiList = NULL;

/* If key is found in a single database then make hash here
 * rather than separately for each database. */
if ((keyField = js->fieldList) == NULL)
    return;
if (slCount(keyField->dbList) == 1)
    {
    if ((preferredDb == NULL) || (sameString(keyField->dbList->name, preferredDb)))
	keyHash = readKeyHash(keyField->dbList->name, joiner, keyField, &khiList);
    else
        return;
    }

/* Check key for database(s) */
if (preferredDb)
    jsValidateKeysOnDb(joiner, js, preferredDb, keyHash, khiList);
else
    {
    struct slName *db, *dbList = jsAllDb(js);
    for (db = dbList; db != NULL; db = db->next)
        jsValidateKeysOnDb(joiner, js, db->name, keyHash, khiList);
    slFreeList(&dbList);
    }
hashFree(&keyHash);
}

void joinerValidateKeys(struct joiner *joiner, 
	char *oneIdentifier, char *oneDatabase)
/* Validate all keys in joiner.  If oneDatabase is non-NULL then do it on
 * that database.  Otherwise do it on all databases. */
{
struct joinerSet *js;
int validations = 0;
verbose(1, "Checking keys on database %s\n", oneDatabase);
for (js = joiner->jsList; js != NULL; js = js->next)
    {
    verbose(2, "identifier %s\n", js->name);
    if (oneIdentifier == NULL || wildMatch(oneIdentifier, js->name))
	{
        jsValidateKeys(joiner, js, oneDatabase);
	++validations;
	}
    }
if (validations < 1 && oneIdentifier)
    errAbort("Identifier %s not found in %s", oneIdentifier, joiner->fileName);
}

static struct hash *joinerAllFields(struct joiner *joiner)
/* Get hash of all fields in database.table.field format.
 * Similar to sqlAllFields(void) function in jksql.c, but different
 *	method of creating the list of databases to check.
 */
{
struct hash *fullHash = hashNew(18);
struct hashEl *db, *dbList = hashElListHash(joiner->databasesChecked);
for (db = dbList; db != NULL; db = db->next)
    {
    verbose(3, "joinerAllFields: extracting fields from database: '%s'\n",
	db->name);
    sqlAddDatabaseFields(db->name, fullHash);
    }
slFreeList(&dbList);
return fullHash;
}

static struct hash *processFieldHash(struct joiner *joiner, char *inName,
    char *outName)
/* Read in field hash from file if inName is non-NULL, 
 * else read from database.  If outName is non-NULL, 
 * save it to file.  */
{
struct hash *fieldHash;

if (inName != NULL)
    fieldHash = hashWordsInFile(inName, 18);
else
    fieldHash = joinerAllFields(joiner);
if (outName != NULL)
    {
    struct hashEl *el, *list = hashElListHash(fieldHash);
    FILE *f = mustOpen(outName, "w");
    slSort(&list, hashElCmp);
    for (el = list; el != NULL; el = el->next)
	fprintf(f, "%s\n", el->name);
    slFreeList(&list);
    carefulClose(&f);
    }
return fieldHash;
}

void reportErrorList(struct slName **pList, char *message)
/* Report error on list of string references. */
/* Convert a list of string references to a comma-separated
 * slName.  Free up refList. */
{
if (*pList != NULL)
    {
    struct dyString *dy = dyStringNew(0);
    struct slName *name, *list;
    slReverse(pList);
    list = *pList;
    dyStringAppend(dy, list->name);
    for (name = list->next; name != NULL; name = name->next)
	{
        dyStringAppendC(dy, ',');
	dyStringAppend(dy, name->name);
	}
    warn("Error: %s %s", dy->string, message);
    slFreeList(pList);
    dyStringFree(&dy);
    }
}


static void joinerCheckDbCoverage(struct joiner *joiner)
/* Complain about databases that aren't in databasesChecked or ignored. */
{
struct slName *missList = NULL, *miss;
struct slName *db, *dbList = sqlListOfDatabases();

/* Keep a list of databases that aren't in either hash. */
for (db = dbList; db != NULL; db = db->next)
    {
    if (!hashLookup(joiner->databasesChecked, db->name) 
        && !hashLookup(joiner->databasesIgnored, db->name))
	{
	miss = slNameNew(db->name);
	slAddHead(&missList, miss);
	}
    }

/* If necessary report (in a single message) database not in joiner file. */
reportErrorList(&missList, "not in databasesChecked or databasesIgnored");
}

static void addTablesLike(struct hash *hash, struct sqlConnection *conn,
	char *spec)
/* Add tables like spec to hash. */
{
if (sqlWildcardIn(spec))
    {
    struct sqlResult *sr;
    char query[512], **row;
    sqlSafef(query, sizeof(query), 
	    "show tables like '%s'", spec);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	hashAdd(hash, row[0], NULL);
    sqlFreeResult(&sr);
    }
else
    {
    hashAdd(hash, spec, NULL);
    }
}


static struct hash *getCoveredTables(struct joiner *joiner, char *db, 
	struct sqlConnection *conn)
/* Get list of tables covered in database. */
{
struct hash *hash = hashNew(0);
struct joinerIgnore *ig;
struct slName *spec;
struct joinerSet *js;
struct joinerField *jf;

/* First put in all the ignored tables. */
for (ig = joiner->tablesIgnored; ig != NULL; ig = ig->next)
    {
    if (slNameInList(ig->dbList, db))
        {
	for (spec = ig->tableList; spec != NULL; spec = spec->next)
	    {
	    verbose(3,"ignoreTable: '%s'\n", spec->name);
	    addTablesLike(hash, conn, spec->name);
	    }
	}
    }

/* Now put in tables that are in one of the identifiers. */
for (js = joiner->jsList; js != NULL; js = js->next)
    {
    for (jf = js->fieldList; jf != NULL; jf = jf->next)
        {
	if (slNameInList(jf->dbList, db))
	    {
	    char spec[512];
	    safef(spec, sizeof(spec), "%s%s%s",
	        emptyForNull(jf->splitPrefix), jf->table,
		emptyForNull(jf->splitSuffix));
	    addTablesLike(hash, conn, spec);
	    verbose(4,"ident: '%s', table: '%s'\n", js->name, spec);
	    }
	}
    }
return hash;
}

void joinerCheckTableCoverage(struct joiner *joiner, char *specificDb)
/* Check that all tables either are part of an identifier or
 * are in the tablesIgnored statements. */
{
struct slName *miss, *missList = NULL;
struct hashEl *dbList, *db;

dbList = hashElListHash(joiner->databasesChecked);
for (db = dbList; db != NULL; db = db->next)
    {
    if (specificDb == NULL || sameString(db->name, specificDb))
	{
	struct sqlConnection *conn = sqlMayConnect(db->name);
	if (conn == NULL)
	    warn("Error: database %s doesn't exist", db->name);
	else
	    {
	    struct slName *table;
	    struct slName *tableList = sqlListTables(conn);
	    struct hash *hash = getCoveredTables(joiner, db->name, conn);
	    for (table = tableList; table != NULL; table = table->next)
		{
		if (!hashLookup(hash, table->name))
		    {
		    char fullName[256];
		    safef(fullName, sizeof(fullName), "%s.%s", 
			    db->name, table->name);
		    miss = slNameNew(fullName);
		    slAddHead(&missList, miss);
		    }
		else
		    verbose(2,"tableCovered: '%s'\n", table->name);
		}
	    slFreeList(&tableList);
	    freeHash(&hash);
	    reportErrorList(&missList, "tables not in .joiner file");
	    }
	sqlDisconnect(&conn);
	}
    }
slFreeList(&dbList);
}

void joinerCheck(char *fileName)
/* joinerCheck - Parse and check joiner file. */
{
struct joiner *joiner = joinerRead(fileName);
/* verify specified database is in all.joiner */
if (database)
    {
    if (hashLookup(joiner->databasesIgnored, database))
	errAbort("specified database '%s' is on list of databasesIgnored", database);
    if (!hashLookup(joiner->databasesChecked, database))
	errAbort("specified database '%s' is not listed in all.joiner", database);
    }
if (dbCoverage)
    joinerCheckDbCoverage(joiner);
if (tableCoverage)
    joinerCheckTableCoverage(joiner, database);
if (checkTimes)
    joinerCheckDependencies(joiner, database);
if (checkFields)
    {
    struct hash *fieldHash;
    fieldHash = processFieldHash(joiner, fieldListIn, fieldListOut);
    joinerValidateFields(joiner, fieldHash, identifier);
    }
if (foreignKeys)
    joinerValidateKeys(joiner, identifier, database);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
fieldListIn = optionVal("fieldListIn", NULL);
fieldListOut = optionVal("fieldListOut", NULL);
identifier = optionVal("identifier", NULL);
database = optionVal("database", NULL);
checkFields = optionExists("fields");
foreignKeys = optionExists("keys");
dbCoverage = optionExists("dbCoverage");
tableCoverage = optionExists("tableCoverage");
checkTimes = optionExists("times");
if (optionExists("all"))
    {
    checkFields = foreignKeys = dbCoverage = tableCoverage = checkTimes = TRUE;
    }
allDbHash = sqlHashOfDatabases();
if (database)
    {
    if (! hashLookup(allDbHash, database))
	errAbort("specified database '%s' not available", database);
    }
if (! (checkFields|foreignKeys|dbCoverage|tableCoverage|checkTimes))
    {
    warn("*** Error: need to specify one of the options:\n"
	"\tfields, keys, tableCoverage, dbCoverage, times or all\n");
    usage();
    }
joinerCheck(argv[1]);
return 0;
}
