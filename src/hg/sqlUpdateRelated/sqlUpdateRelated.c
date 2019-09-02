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
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
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

void sqlUpdateViaTabFile(struct sqlConnection *conn, char *tabFile, char *tableName)
/* Interpret one tab-separated file */
{
// Load the tabFile 
struct fieldedTable *inTable = fieldedTableFromTabFile(tabFile, tabFile, NULL, 0);
verbose(2, "%d fields and %d rows in %s\n",  inTable->fieldCount, inTable->rowCount, tabFile);
char **inFields = inTable->fields;

// Loop through the fields looking for special ones
char *conditionalField = NULL;  // We might have one of these
int conditionalIx = -1;
struct foreignRef *foreignRefList = NULL;
struct multiRef *multiRefList = NULL;
struct hash *foreignHash = hashNew(0), *multiHash = hashNew(0);
int fieldIx;
for (fieldIx=0; fieldIx<inTable->fieldCount; ++fieldIx)
    {
    char *field = inFields[fieldIx];
    char firstChar = field[0];
    if (firstChar == '?')
        {
	if (conditionalField != NULL)
	    errAbort("Multiple fields starting with a '?', There can only be one\n"
		"but both %s and %s exist\n", conditionalField, field+1);
	conditionalField = field;
	conditionalIx = fieldIx;
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
	    if (!sqlTableExists(conn, mRef->relationalTable))
	        errAbort("Table %s from %s doesn't exist", mRef->relationalTable, field);
	    if (!sqlTableExists(conn, mRef->foreignTable))
	        errAbort("Table %s from %s doesn't exist", mRef->foreignTable, field);
	    slAddTail(&multiRefList, mRef);
	    hashAdd(multiHash, field, mRef);
	    }
	else
	    {
	    verbose(2, "foreignRef field = %s\n", field);
	    char *pos = chopTemp + 2;  // Set up to be past @@
	    int expectedCount = 4;
	    char *parts[expectedCount+1];	// More than we need
	    int partCount = chopByChar(pos, '@', parts, ArraySize(parts));
	    if (partCount != expectedCount)
	        {
		errAbort("Expecting %d @ separated fields in %s, got %d\n",
		    expectedCount, field, partCount);
		}
	    struct foreignRef *fRef;
	    AllocVar(fRef);
	    fRef->nativeFieldName = parts[0];
	    fRef->foreignTable = parts[1];
	    fRef->foreignFindName = parts[2];
	    fRef->foreignKeyName = parts[3];
	    fRef->nativeFieldIx = fieldIx;
	    if (!sqlTableExists(conn, fRef->foreignTable))
	        errAbort("Table %s from %s doesn't exist", fRef->foreignTable, field);
	    slAddTail(&foreignRefList, fRef);
	    hashAdd(foreignHash, field, fRef);
	    }
	}
    }
verbose(2, "Got %s conditional, %d foreignRefs, %d multiRefs\n", 
	naForNull(conditionalField), slCount(foreignRefList), slCount(multiRefList));

/* Now we loop through the input table */
struct fieldedRow *fr;
struct dyString *sql = dyStringNew(0);
struct dyString *csvScratch = dyStringNew(0);
for (fr = inTable->rowList; fr != NULL; fr = fr->next)
    {
    char **row = fr->row;

    /* Deal with conditional field */
    if (conditionalField != NULL && sameString(conditionalField, inFields[conditionalIx]))
	{
	dyStringClear(sql);
	char *rawVal = row[conditionalIx];
	char *uncsvVal = csvParseNext(&rawVal, csvScratch);
	// Before we do more we see if the record already exists
	sqlDyStringPrintf(sql, "select count(*) from %s where %s='%s'",
	    tableName, conditionalField+1, uncsvVal);
	verbose(1, "%s\n", sql->string);
	if (sqlQuickNum(conn, sql->string) > 0)
	    continue;
	}

    /* Cope with foreign keys */
    struct foreignRef *fRef;
    for (fRef = foreignRefList; fRef != NULL; fRef = fRef->next)
        {
	char *origVal = row[fRef->nativeFieldIx];
	char *val = emptyForNull(csvParseNext(&origVal, csvScratch));
	char *escaped = sqlEscapeString(val);
	dyStringPrintf(sql, "\"%s\"",  escaped);
	dyStringClear(sql);
	sqlDyStringPrintf(sql, "select %s from %s where %s=\"%s\"",
	    fRef->foreignKeyName, fRef->foreignTable, 
	    fRef->foreignFindName, escaped);
	verbose(1, "query for foreignKey: %s\n", sql->string);
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
		// the relationship table
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
	    dyStringAppendC(sql, ',');
	dyStringAppend(sql, field);
	}

    /* Now generate the values bit */
    dyStringAppend(sql, ") values (");
    firstTime = TRUE;
    for (fieldIx=0; fieldIx < inTable->fieldCount; ++fieldIx)
        {
	char *field = inFields[fieldIx];
	char firstChar = field[0];

	if (firstChar == '@')
	    {
	    if (field[1] == '@')  // multi field
		continue;
	    else
	        field += 1;	// We val with the foreign key here, just skip over '@'
	    }
	if (firstChar == '?')
	    field += 1;
	if (firstTime)
	    firstTime = !firstTime;
	else
	    dyStringAppendC(sql, ',');
	char *origVal = row[fieldIx];
	char *val = emptyForNull(csvParseNext(&origVal, csvScratch));
	char *escaped = sqlEscapeString(val);
	dyStringPrintf(sql, "\"%s\"",  escaped);
	freez(&escaped);
	}
    dyStringAppendC(sql, ')');
    verbose(1, "update sql: %s\n", sql->string);
    sqlUpdate(conn, sql->string);

    /* Clean up strings allocated for field references */
    for (fRef = foreignRefList; fRef != NULL; fRef = fRef->next)
	freez(&fRef->foreignKey);
    }
dyStringFree(&sql);
dyStringFree(&csvScratch);
}

void sqlUpdateRelated(char *database, char **inFiles, int inCount)
/* sqlUpdateRelated - Update a bunch of tables in a kind of careful way based out of tab 
 * separated files.  Handles foreign key and many-to-many relationships with a multitude 
 * of @ signs.. */
{
struct sqlConnection *conn = sqlConnect(database);
int fileIx;
for (fileIx = 0; fileIx < inCount; ++fileIx)
    {
    char *inFile = inFiles[fileIx];
    char *tableName = cloneString(inFile);
    chopSuffix(tableName);
    verbose(1, "Processing %s into %s table \n", inFile, tableName);

    sqlUpdateViaTabFile(conn, inFile, tableName);
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
