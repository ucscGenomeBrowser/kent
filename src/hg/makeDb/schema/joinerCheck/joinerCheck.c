/* joinerCheck - Parse and check joiner file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"
#include "jksql.h"
#include "joiner.h"

static char const rcsid[] = "$Id: joinerCheck.c,v 1.10 2004/03/12 06:40:32 kent Exp $";

/* Variable that are set from command line. */
boolean parseOnly; 
char *fieldListIn;
char *fieldListOut;
char *identifier;

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
  "   -gdbCheck=db - Do additional validation on given database.\n"
  );
}

static struct optionSpec options[] = {
   {"parseOnly", OPTION_BOOLEAN},
   {"fieldListIn", OPTION_STRING},
   {"fieldListOut", OPTION_STRING},
   {"identifier", OPTION_STRING},
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


void joinerValidateOnDbs(struct joinerSet *jsList, struct hash *fieldHash,
	char *oneIdentifier, char *fileName)
/* Make sure that joiner refers to fields that exist at
 * least somewhere. */
{
struct joinerSet *js;
struct joinerField *jf;
struct slName *db;
struct hash *dbChromHash = getDbChromHash();

for (js=jsList; js != NULL; js = js->next)
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
			js->name, jf->lineIx, fileName);
		     }
		 }
	    }
	}
    }
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
    joinerValidateOnDbs(joiner->jsList, fieldHash, identifier, fileName);
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
joinerCheck(argv[1]);
return 0;
}
