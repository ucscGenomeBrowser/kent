/* joinerCheck - Parse and check joiner file. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"
#include "jksql.h"
#include "joiner.h"

static char const rcsid[] = "$Id: joinerCheck.c,v 1.12 2004/03/12 09:39:45 kent Exp $";

/* Variable that are set from command line. */
boolean parseOnly; 
char *fieldListIn;
char *fieldListOut;
char *identifier;
char *database;
boolean foreignKeys;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "joinerCheck - Parse and check joiner file\n"
  "usage:\n"
  "   joinerCheck file.joiner\n"
  "options:\n"
  "   -parseOnly just parse joiner file, don't check database.\n"
  "   -fieldListOut=file - List all fields in all databases to file.\n"
  "   -fieldListIn=file - Get list of fields from file rather than mysql.\n"
  "   -identifier=name - Just validate given identifier.\n"
  "   -database=name - Just validate given database.\n"
  "   -foreignKeys - Validate (foreign) keys.  Takes about an hour.\n"
  );
}

static struct optionSpec options[] = {
   {"parseOnly", OPTION_BOOLEAN},
   {"fieldListIn", OPTION_STRING},
   {"fieldListOut", OPTION_STRING},
   {"identifier", OPTION_STRING},
   {"database", OPTION_STRING},
   {"foreignKeys", OPTION_BOOLEAN},
   {NULL, 0},
};


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

static boolean fieldExists(struct hash *fieldHash,
	struct hash *dbChromHash,
	struct joinerSet *js, struct joinerField *jf)
/* Make sure field exists in at least one database. */
{
struct slName *db;

/* First try to find it as non-split in some database. */
for (db = jf->dbList; db != NULL; db = db->next)
    {
    char fieldName[512];
    safef(fieldName, sizeof(fieldName), "%s.%s.%s", 
    	db->name, jf->table, jf->field);
    if (hashLookup(fieldHash, fieldName))
        return TRUE;
    }

/* Next try to find it as split. */
for (db = jf->dbList; db != NULL; db = db->next)
    {
    struct slName *chrom, *chromList = hashFindVal(dbChromHash, db->name);
    char fieldName[512];
    for (chrom = chromList; chrom != NULL; chrom = chrom->next)
        {
	safef(fieldName, sizeof(fieldName), "%s.%s_%s.%s", 
	    db->name, chrom->name, jf->table, jf->field);
	if (hashLookup(fieldHash, fieldName))
	    return TRUE;
	}
    }

return FALSE;
}

static struct hash *getDbChromHash()
/* Return hash with chromosome name list for each database
 * that has a chromInfo table. */
{
struct sqlConnection *conn = sqlConnect("mysql");
struct slName *dbList = sqlGetAllDatabase(conn), *db;
struct hash *dbHash = newHash(10);
struct slName *chromList, *chrom;
sqlDisconnect(&conn);

for (db = dbList; db != NULL; db = db->next)
     {
     conn = sqlConnect(db->name);
     if (sqlTableExists(conn, "chromInfo"))
          {
	  struct sqlResult *sr;
	  char **row;
	  struct slName *chromList = NULL, *chrom;
	  sr = sqlGetResult(conn, "select chrom from chromInfo");
	  while ((row = sqlNextRow(sr)) != NULL)
	      {
	      chrom = slNameNew(row[0]);
	      slAddHead(&chromList, chrom);
	      }
	  sqlFreeResult(&sr);
	  slReverse(&chromList);
	  hashAdd(dbHash, db->name, chromList);
	  }
     sqlDisconnect(&conn);
     }
slFreeList(&dbList);
return dbHash;
}


void joinerValidateFields(struct joiner *joiner, struct hash *fieldHash,
	char *oneIdentifier)
/* Make sure that joiner refers to fields that exist at
 * least somewhere. */
{
struct joinerSet *js;
struct joinerField *jf;
struct slName *db;
struct hash *dbChromHash = getDbChromHash();

for (js=joiner->jsList; js != NULL; js = js->next)
    {
    if (oneIdentifier == NULL || sameString(oneIdentifier, js->name))
	{
	for (jf = js->fieldList; jf != NULL; jf = jf->next)
	    {
	    if (!fieldExists(fieldHash, dbChromHash, js, jf))
		 {
		 if (!js->expanded)
		     {
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
safef(query, sizeof(query), "select count(*) from %s", table);
return sqlQuickNum(conn, query);
}

struct sqlConnection *sqlWarnConnect(char *db)
/* Connect to database, or warn and return NULL. */
{
struct sqlConnection *conn = sqlMayConnect(db);
if (conn == NULL)
    warn("Couldn't connect to database %s", db);
return conn;
}

struct hash *readKeyHash(char *db, struct joiner *joiner, 
	struct joinerField *keyField)
/* Read key-field into hash.  Check for dupes if need be. */
{
struct sqlConnection *conn = sqlWarnConnect(db);
struct hash *keyHash = NULL;
if (conn == NULL)
    {
    return NULL;
    }
else if (!sqlTableExists(conn, keyField->table))
    {
    /* warn("Key table %s.%s doesn't exist", db, keyField->table); */
    }
else
    {
    int rowCount = sqlTableRows(conn, keyField->table);
    int hashSize = digitsBaseTwo(rowCount)+1;
    char query[256], **row;
    struct sqlResult *sr;
    int itemCount = 0;
    int dupeCount = 0;
    struct slName *chop;
    char *dupe = NULL;
    keyHash = hashNew(hashSize);
    safef(query, sizeof(query), "select %s from %s", 
    	keyField->field, keyField->table);
    uglyf("%s\n", query);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	char *id = row[0];
	for (chop = keyField->chopBefore; chop != NULL; chop = chop->next)
	    {
	    char *s = stringIn(chop->name, id);
	    if (s != NULL)
	         id = s + strlen(chop->name);
	    }
	for (chop = keyField->chopAfter; chop != NULL; chop = chop->next)
	    {
	    char *s = rStringIn(chop->name, id);
	    if (s != NULL)
	        *s = 0;
	    }
	if (hashLookup(keyHash, id))
	    {
	    if (!keyField->dupeOk)
	        {
		if (dupeCount == 0)
		    dupe = cloneString(id);
		++dupeCount;
		}
	    }
	else
	    {
	    hashAdd(keyHash, id, NULL);
	    ++itemCount;
	    }
	}
    sqlFreeResult(&sr);
    if (dupe != NULL)
	{
        warn("%d duplicates in %s.%s.%s including %s",
		dupeCount, db, keyField->table, keyField->field, dupe);
	freez(&dupe);
	}
    uglyf("%d items in %s.%s.%s\n", itemCount, db, keyField->table, keyField->field);
    }
sqlDisconnect(&conn);
return keyHash;
}

void doMinCheck(char *db, struct joiner *joiner, struct joinerSet *js, 
	struct hash *keyHash, struct joinerField *jf)
{
struct sqlConnection *conn = sqlMayConnect(db);
if (conn != NULL)
    {
    if (sqlTableExists(conn, jf->table))
	{
	char query[256], **row;
	struct sqlResult *sr;
	int total = 0, hits = 0, hitsNeeded;
	char *miss = NULL;
	safef(query, sizeof(query), "select %s from %s", jf->field, jf->table);
	uglyf("db %s: %s\n", db, query);
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    ++total;
	    if (hashLookup(keyHash, row[0]))
	        ++hits;
	    else
	        {
		if (miss == NULL)
		    miss = cloneString(row[0]);
		}
	    }
	sqlFreeResult(&sr);
	hitsNeeded = round(total * jf->minCheck);
	if (hits < hitsNeeded)
	    {
	    warn("%d of %d elements of %s.%s.%s are not in key line %d of %s\n"
	         "Example miss: %s",
	    	total - hits, total, db, jf->field, jf->table, jf->lineIx,
		joiner->fileName, miss);
	    }
	freez(&miss);
	}
    sqlDisconnect(&conn);
    }
}

void jsValidateKeysOnDb(struct joiner *joiner, struct joinerSet *js,
	char *db, struct hash *keyHash)
/* Validate keys pertaining to this database. */
{
struct joinerField *jf;
struct hash *localKeyHash = NULL;
struct joinerField *keyField;

if (js->isFuzzy)
    return;
if ((keyField = js->fieldList) == NULL)
    return;
if (keyHash == NULL)
    {
    char *keyDb;
    if (slNameInList(keyField->dbList, db))
	keyDb = db;
    else
	{
	if (slCount(keyField->dbList) == 1)
	    keyDb = keyField->dbList->name;
	else
	    {
	    warn("Error line %d of %s:\n"
		 "Key (first) field contains multiple databases\n"
		 "but not all databases in other fields."
		 , joiner->fileName, keyField->lineIx);
	    return;
	    }
	}
    keyHash = localKeyHash = readKeyHash(keyDb, joiner, keyField);
    }
if (keyHash != NULL)
    {
    for (jf = js->fieldList; jf != NULL; jf = jf->next)
	{
	if (jf != keyField && slNameInList(jf->dbList, db))
	    doMinCheck(db, joiner, js, keyHash, jf);
	}
    }
hashFree(&localKeyHash);
}

void jsValidateKeys(struct joiner *joiner, struct joinerSet *js, 
	char *preferredDb)
/* Validate keys on js.  If preferredDb is non-NULL then do it on
 * that database.  Otherwise do it on all databases. */
{
struct joinerField *keyField;
struct hash *keyHash = NULL;

/* If key is found in a single database then make hash here
 * rather than separately for each database. */
if ((keyField = js->fieldList) == NULL)
    return;
if (slCount(keyField->dbList) == 1)
    keyHash = readKeyHash(keyField->dbList->name, joiner, keyField);

/* Check key for database(s) */
if (preferredDb)
    jsValidateKeysOnDb(joiner, js, preferredDb, keyHash);
else
    {
    struct slName *db, *dbList = jsAllDb(js);
    for (db = dbList; db != NULL; db = db->next)
        jsValidateKeysOnDb(joiner, js, db->name, keyHash);
    slFreeList(&dbList);
    }
hashFree(&keyHash);
}

void joinerValidateKeys(struct joiner *joiner, 
	char *oneIdentifier, char *oneDatabase)
/* Make sure that joiner refers to fields that exist at
 * least somewhere. */
{
struct joinerSet *js;
int validations = 0;
for (js = joiner->jsList; js != NULL; js = js->next)
    {
    if (oneIdentifier == NULL || sameString(oneIdentifier, js->name))
	{
        jsValidateKeys(joiner, js, oneDatabase);
	++validations;
	}
    }
if (validations < 1 && oneIdentifier)
    errAbort("Identifier %s not found in %s", oneIdentifier, joiner->fileName);
}

struct hash *processFieldHash(char *inName, char *outName)
/* Read in field hash from file if inName is non-NULL, 
 * else read from database.  If outName is non-NULL, 
 * save it to file. */
{
struct hash *fieldHash;
struct hashEl *el;

if (inName != NULL)
    fieldHash = hashWordsInFile(inName, 18);
else
    fieldHash = sqlAllFields();
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

void joinerCheck(char *fileName)
/* joinerCheck - Parse and check joiner file. */
{
struct joiner *joiner = joinerRead(fileName);
if (!parseOnly)
    {
    struct hash *fieldHash = processFieldHash(fieldListIn, fieldListOut);
    joinerValidateFields(joiner, fieldHash, identifier);
    if (foreignKeys)
	joinerValidateKeys(joiner, identifier, database);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
parseOnly = optionExists("parseOnly");
fieldListIn = optionVal("fieldListIn", NULL);
fieldListOut = optionVal("fieldListOut", NULL);
identifier = optionVal("identifier", NULL);
database = optionVal("database", NULL);
foreignKeys = optionExists("foreignKeys");
joinerCheck(argv[1]);
return 0;
}
