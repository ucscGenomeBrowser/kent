/* sqlUpdateRelated - Update a bunch of tables in a kind of careful way based out of tab separated 
 * files.  Handles foreign key and many-to-many relationships with a multitude of @ signs. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fieldedTable.h"
#include "csv.h"
#include "jksql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sqlUpdateRelated - Update a bunch of tables in a kind of careful way based out of tab \n"
  "separated files.  Handles foreign key and many-to-many relationships with a multitude\n"
  "of @ signs.  Currently only works with mysql cause going through jksql\n"
  "usage:\n"
  "   sqlUpdateRelated database tableFiles\n"
  "options:\n"
  "   -missOk - if set, tableFiles mentioned that don't exist are skipped rather than erroring\n"
  "   -uncsv - if set, run uncsv and just take first value for each field, needed sometimes\n"
  "            to deal with extra quotes from tagstorms and other sources\n"
  "The tableFiles are in a interesting and peculiar format.  The first line with the field name\n"
  "ends up controlling this program.  If a field starts with just a regular letter all is as\n"
  "you may expect,  the field just contains data to load.  However if the field starts with\n"
  "a special char, special things happen.  In particular\n"
  "   ? - indicates field is a conditional key field.  Record is only inserted if the value\n"
  "       for this field is not already present in table\n"
  "   ! - indicates this is update key field.  Record must already exist,  values in other fields\n"
  "       are updated.\n"
  "   @ - indicates a foreign key relationship - see source code until docs are in shape\n"
  "   @@ - indicates a many-to-many relationship - see source code until docs are in shape"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"missOk", OPTION_BOOLEAN},
   {"uncsv", OPTION_BOOLEAN},
   {NULL, 0},
};

struct foreignRef
/* What we need to handle a foreign key reference */
    {
    struct foreignRef *next;	    // Next in list
    char *nativeFieldName;	    // Name in the current table
    char *foreignTable;		    // Foreign table name
    char *foreignFindName;	    // Name to search in the destination table
    char *foreignKeyName;	    // Name of key we are fetching, usually just "id"
    char *outputVal;		    // Used as a place to hold the row value for later processing
    int nativeFieldIx;		    // Field index in native table
    char *foreignKey;		    // Actual foreign key - computed each row
    };

struct multiRef
/* What we need to handle a foreign key reference */
    {
    struct multiRef *next;	    // Next in list
    char *nativeFieldName;	    // Name in the current table
    char *nativeKeyName;	    // The name of the key in current table
    char *relationalTable;	    // Table that links the two together
    char *relationalNativeField;    // The field that specifies the record in current table
    char *relationalForeignField;   // The field that specifies the record in foreign table
    char *foreignTable;		    // Foreign table name
    char *foreignFindName;	    // Name to search in the destination table
    char *foreignKeyName;	    // Name of key we are fetching, usually just "id"
    int nativeFieldIx;		    // Field index in native table
    };

void addMultiRelation(struct sqlConnection *conn, struct multiRef *mRef, struct fieldedRow *fr,
    int nativeId, char *tabFile, struct dyString *csvScratch)
/* inCsv is a comma separated list of names that we should be able to locate in the foreign
 * table via mRef->foreignFindName.  We make up relationships for them in the relationalTable. */
{
char *inCsv = fr->row[mRef->nativeFieldIx];
char *nativeVal;
char *parsePos = inCsv;
struct dyString *sql = dyStringNew(0);
while ((nativeVal = csvParseNext(&parsePos, csvScratch)) != NULL)
    {
    char *escaped = sqlEscapeString(nativeVal);
    dyStringClear(sql);
    sqlDyStringPrintf(sql, "select %s from %s where %s=\"%s\"",
	    mRef->foreignKeyName, mRef->foreignTable, 
	    mRef->foreignFindName, escaped);
    char *foreignKey = sqlQuickString(conn, sql->string);
    verbose(2, "foreignKey for %s is %s\n", nativeVal, foreignKey);
    if (isEmpty(foreignKey))
	errAbort("No %s in table %s referenced line %d of %s",  nativeVal, mRef->foreignTable,
	    fr->id,  tabFile);
    
    // Alright, we got our native and foreign keys, let's insert a row in relationship table
    dyStringClear(sql);
    sqlDyStringPrintf(sql, "insert into %s (%s,%s) values (%d,%s)", mRef->relationalTable,
	mRef->relationalNativeField, mRef->relationalForeignField,
	nativeId, foreignKey);
    verbose(2, "relationship sql: %s\n", sql->string);
    sqlUpdate(conn, sql->string);
    freez(&escaped);
    }
dyStringFree(&sql);
}

void checkFieldExists(struct sqlConnection *conn, char *table, char *field, char *attyField)
/* Make sure field exists in table in database or print error message that includes
 * attyField */
{
if (sqlFieldIndex(conn, table, field) < 0)
    errAbort("No field %s in table %s used in %s", 
	field, table, attyField);
}

void checkTableExists(struct sqlConnection *conn, char *table, char *attyField)
/* Make sure table exists in database or print error message that includes
 * attyField */
{
if (!sqlTableExists(conn, table))
    errAbort("Table %s from %s doesn't exist", table, attyField);
}

void sqlUpdateViaTabFile(struct sqlConnection *conn, char *tabFile, char *tableName, boolean uncsv)
/* Interpret one tab-separated file */
{
// Load the tabFile 
struct fieldedTable *inTable = fieldedTableFromTabFile(tabFile, tabFile, NULL, 0);
verbose(2, "%d fields and %d rows in %s\n",  inTable->fieldCount, inTable->rowCount, tabFile);
char **inFields = inTable->fields;

// Loop through the fields creating field set for output table and parsing
// foreign and multi-multi fields into structures 
char *conditionalField = NULL;  // We might have one of these
int conditionalIx = -1;
boolean updateCondition = FALSE;    
struct foreignRef *foreignRefList = NULL;
struct multiRef *multiRefList = NULL;
int fieldIx;
for (fieldIx=0; fieldIx<inTable->fieldCount; ++fieldIx)
    {
    char *field = inFields[fieldIx];
    char firstChar = field[0];
    if (firstChar == '?' || firstChar == '!')
        {
	if (conditionalField != NULL)
	    errAbort("Multiple fields starting with a '?' or '!', There can only be one\n"
		"but both %s and %s exist\n", conditionalField, field+1);
	conditionalField = field;
	conditionalIx = fieldIx;
	updateCondition = (firstChar == '!');
	checkFieldExists(conn, tableName, field + 1, field);
	verbose(2, "conditionalField = %s, ix = %d\n", field, conditionalIx);
	}
    else if (firstChar == '@')  // Foreign keys are involved.  Will it get worse?
        {
	char *chopTemp = cloneString(field);
	if (field[1] == '@')  // Ugh, a multiRef.  Much to parse!
	    {
	    verbose(2, "multiRef field = %s\n", field);
	    char *pos = chopTemp + 2;  // Set up to be past @
	    int expectedCount = 8;
	    char *parts[expectedCount+1];	// More than we need
	    int partCount = chopByChar(pos, '@', parts, ArraySize(parts));
	    if (partCount != expectedCount)
	        {
		errAbort("Expecting %d @ separated fields in %s, got %d\n",
		    expectedCount, field, partCount);
		}
	    /* Makup up multiRef struct */
	    struct multiRef *mRef;
	    AllocVar(mRef);
	    mRef->nativeFieldName = parts[0];	    
	    mRef->nativeKeyName = parts[1];	    
	    mRef->relationalTable = parts[2];	    
	    mRef->relationalNativeField = parts[3];    
	    mRef->relationalForeignField = parts[4];   
	    mRef->foreignTable = parts[5];		    
	    mRef->foreignFindName = parts[6];	    
	    mRef->foreignKeyName = parts[7];	    
	    mRef->nativeFieldIx = fieldIx;

	    /* Check fields and tables exist */
	    checkFieldExists(conn, tableName, mRef->nativeKeyName, field);
	    checkTableExists(conn, mRef->relationalTable, field);
	    checkFieldExists(conn, mRef->relationalTable, mRef->relationalNativeField, field);
	    checkFieldExists(conn, mRef->relationalTable, mRef->relationalForeignField, field);
	    checkTableExists(conn, mRef->foreignTable, field);
	    checkFieldExists(conn, mRef->foreignTable, mRef->foreignFindName, field);
	    checkFieldExists(conn, mRef->foreignTable, mRef->foreignKeyName, field);

	    /* Everything checks out, add it to list */
	    slAddTail(&multiRefList, mRef);
	    }
	else
	    {
	    verbose(2, "foreignRef field = %s\n", field);
	    char *pos = chopTemp + 1;  // Set up to be past @
	    int expectedCount = 4;
	    char *parts[expectedCount+1];	// More than we need
	    int partCount = chopByChar(pos, '@', parts, ArraySize(parts));
	    if (partCount != expectedCount)
	        {
		errAbort("Expecting %d @ separated fields in %s, got %d\n",
		    expectedCount, field, partCount);
		}
	    /* Make up a foreignRef */
	    struct foreignRef *fRef;
	    AllocVar(fRef);
	    fRef->nativeFieldName = parts[0];
	    fRef->foreignTable = parts[1];
	    fRef->foreignFindName = parts[2];
	    fRef->foreignKeyName = parts[3];
	    fRef->nativeFieldIx = fieldIx;

	    /* Make sure all tables and fields exist */
	    checkFieldExists(conn, tableName, fRef->nativeFieldName, field);
	    checkTableExists(conn, fRef->foreignTable, field);
	    checkFieldExists(conn, fRef->foreignTable, fRef->foreignFindName, field);
	    checkFieldExists(conn, fRef->foreignTable, fRef->foreignKeyName, field);

	    slAddTail(&foreignRefList, fRef);
	    }
	}
    else
        {
	checkFieldExists(conn, tableName, field, field);
	}
    }
verbose(2, "Got %s conditional, %d foreignRefs, %d multiRefs\n", 
	naForNull(conditionalField), slCount(foreignRefList), slCount(multiRefList));

if (updateCondition)  // In update mode we can't handle fancy stuff
    {
    if (foreignRefList != NULL || multiRefList != NULL)
        errAbort("Can't handle foreign keys or multi-multi relations when doing ! updates");
    if (inTable->fieldCount < 2)
        errAbort("Need at least two fields in update mode");
    }

/* Now we loop through the input table and make the appropriate sql queries and inserts */
struct fieldedRow *fr;
struct dyString *sql = dyStringNew(0);
struct dyString *csvScratch = dyStringNew(0);
for (fr = inTable->rowList; fr != NULL; fr = fr->next)
    {
    char **row = fr->row;

    /* The case of the update (!) condition is special.  */
    if (updateCondition)
        {
	/* Make sure that the record we are updating exists for better
	 * error reporting.  There's a race condition that'll make a SQL error happen
	 * instead once in a million years. */
	dyStringClear(sql);
	char *rawVal = row[conditionalIx];
	char *uncsvVal = rawVal;
	if (uncsv)
	    uncsvVal = csvParseNext(&rawVal, csvScratch);
	char *conditionalEscaped = sqlEscapeString(uncsvVal);
	sqlDyStringPrintf(sql, "select count(*) from %s where %s='%s'",
	    tableName, conditionalField+1, conditionalEscaped);
	verbose(2, "%s\n", sql->string);
	if (sqlQuickNum(conn, sql->string) == 0)
	    errAbort("Trying to update %s in %s.%s, but it doesn't exist",
		uncsvVal, tableName, conditionalField+1);

	dyStringClear(sql);
	sqlDyStringPrintf(sql, "update %s set", tableName);
	boolean firstTime = TRUE;
	for (fieldIx=0; fieldIx < inTable->fieldCount; ++fieldIx)
	    {
	    if (fieldIx != conditionalIx)
		{
		char *rawVal = row[fieldIx];
		char *uncsvVal = rawVal;
		if (uncsv)
		    uncsvVal = csvParseNext(&rawVal, csvScratch);
		char *escaped = sqlEscapeString(uncsvVal);
		if (firstTime)
		    firstTime = FALSE;
		else
		    sqlDyStringPrintf(sql, ",");
		sqlDyStringPrintf(sql, " %s='%s'", inFields[fieldIx], escaped);
		freez(&escaped);
		}
	    }
	sqlDyStringPrintf(sql, " where %s='%s'", conditionalField+1, conditionalEscaped);
	verbose(2, "%s\n", sql->string);
	sqlUpdate(conn, sql->string);

	continue;	// We are done,  the rest of the loop is for inserts not updates
	}

    /* Deal with conditional field.  If we have one and the value we are trying to insert
     * already exists then just continue to next row. */
    if (conditionalField != NULL && sameString(conditionalField, inFields[conditionalIx]))
	{
	dyStringClear(sql);
	char *rawVal = row[conditionalIx];
	char *uncsvVal = rawVal;
	if (uncsv)
	    uncsvVal = csvParseNext(&rawVal, csvScratch);
	// Before we do more we see if the record already exists
	sqlDyStringPrintf(sql, "select count(*) from %s where %s='%s'",
	    tableName, conditionalField+1, uncsvVal);
	verbose(2, "%s\n", sql->string);
	if (sqlQuickNum(conn, sql->string) > 0)
	    continue;
	}

    /* Cope with foreign keys */
    struct foreignRef *fRef;
    for (fRef = foreignRefList; fRef != NULL; fRef = fRef->next)
        {
	char *rawVal = row[fRef->nativeFieldIx];
	char *uncsvVal = rawVal;
	if (uncsv)
	    uncsvVal = csvParseNext(&rawVal, csvScratch);
	char *val = emptyForNull(uncsvVal);
	char *escaped = sqlEscapeString(val);
	dyStringPrintf(sql, "\"%s\"",  escaped);
	dyStringClear(sql);
	sqlDyStringPrintf(sql, "select %s from %s where %s=\"%s\"",
	    fRef->foreignKeyName, fRef->foreignTable, 
	    fRef->foreignFindName, escaped);
	verbose(2, "query for foreignKey: %s\n", sql->string);
	fRef->foreignKey = sqlQuickString(conn, sql->string);
	if (isEmpty(fRef->foreignKey))
	    errAbort("No %s in table %s referenced line %d of %s",  val, fRef->foreignTable,
	        fr->id,  tabFile);
	row[fRef->nativeFieldIx] = fRef->foreignKey;
	freez(&escaped);
	}

    dyStringClear(sql);
    sqlDyStringPrintf(sql, "insert into %s (", tableName);
    boolean firstTime = TRUE;
    for (fieldIx=0; fieldIx < inTable->fieldCount; ++fieldIx)
        {
	char *field = inFields[fieldIx];
	char firstChar = field[0];

	if (firstChar == '@')
	    {
	    if (field[1] == '@')  // multi field
		{
		// Actually multi field variables don't get written, all lives in
		// the relationship table which we handle after the insert into main 
		// table.
		continue;
		}
	    else
		{
		char *startField = field + 1;  // skip over '@'
		char *endField = strchr(startField, '@');
		// This is parsed out so we know it works until someone rearranged code
		assert(endField != NULL);  
		field = cloneStringZ(startField, endField-startField);
		}
	    }

	// We already dealt with the question mark outside this loop
	if (firstChar == '?')
	    field += 1;


	if (firstTime)
	    firstTime = !firstTime;
	else
	    sqlDyStringPrintf(sql, ",");
	sqlDyStringPrintf(sql, "%s", field);
	}

    /* Now generate the values bit */
    sqlDyStringPrintf(sql, ") values (");
    firstTime = TRUE;
    for (fieldIx=0; fieldIx < inTable->fieldCount; ++fieldIx)
        {
	char *field = inFields[fieldIx];
	char firstChar = field[0];

	if (firstChar == '@')
	    {
	    if (field[1] == '@')  // multi field
		{
		continue;	// multi field output doesn't go into this table, just relationship
		}
	    else
	        field += 1;	// We val with the foreign key here, just skip over '@'
	    }
	if (firstChar == '?')
	    field += 1;
	if (firstTime)
	    firstTime = !firstTime;
	else
	    sqlDyStringPrintf(sql, ",");
	char *rawVal = row[fieldIx];
	char *uncsvVal = rawVal;
	if (uncsv)
	    uncsvVal = csvParseNext(&rawVal, csvScratch);
	char *val = emptyForNull(uncsvVal);
	char *escaped = sqlEscapeString(val);
	dyStringPrintf(sql, "\"%s\"",  escaped);
	freez(&escaped);
	}
    sqlDyStringPrintf(sql, ")");
    verbose(2, "update sql: %s\n", sql->string);
    sqlUpdate(conn, sql->string);
    int mainTableId = sqlLastAutoId(conn);

    /* Handle multi-multi stuff */
    struct multiRef *mRef;
    for (mRef = multiRefList; mRef != NULL; mRef = mRef->next)
        {
	addMultiRelation(conn, mRef, fr, mainTableId, tabFile, csvScratch);
	}

    /* Clean up strings allocated for field references */
    for (fRef = foreignRefList; fRef != NULL; fRef = fRef->next)
	freez(&fRef->foreignKey);
    }
dyStringFree(&sql);
dyStringFree(&csvScratch);
fieldedTableFree(&inTable);
}

void sqlUpdateRelated(char *database, char **inFiles, int inCount)
/* sqlUpdateRelated - Update a bunch of tables in a kind of careful way based out of tab 
 * separated files.  Handles foreign key and many-to-many relationships with a multitude 
 * of @ signs.. */
{
struct sqlConnection *conn = sqlConnect(database);
int fileIx;
boolean missOk = optionExists("missOk");
boolean  uncsv = optionExists("uncsv");
for (fileIx = 0; fileIx < inCount; ++fileIx)
    {
    char *inFile = inFiles[fileIx];
    if (missOk && !fileExists(inFile))
        continue;
    char *tableName = cloneString(inFile);
    chopSuffix(tableName);
    verbose(1, "Processing %s into %s table \n", inFile, tableName);
    sqlUpdateViaTabFile(conn, inFile, tableName, uncsv);
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
sqlUpdateRelated(argv[1], argv+2, argc-2);
return 0;
}
