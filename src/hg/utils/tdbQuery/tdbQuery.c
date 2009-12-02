/* tdbQuery - Query the trackDb system using SQL syntax.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "obscure.h"
#include "portable.h"
#include "tdbRecord.h"
#include "rql.h"

static char const rcsid[] = "$Id: tdbQuery.c,v 1.3 2009/12/02 06:47:27 kent Exp $";

static char *clRoot = "~/kent/src/hg/makeDb/trackDb";	/* Root dir of trackDb system. */
static char *clFile = NULL;		/* a .ra file to use instead of trackDb system. */
static boolean clCheck = FALSE;		/* If set perform lots of checks on input. */
static boolean clStrict = FALSE;	/* If set only return tracks with actual tables. */
static boolean clAlpha = FALSE;		/* If set include release alphas, exclude release beta. */

void usage()
/* Explain usage and exit. */
{
errAbort(
"tdbQuery - Query the trackDb system using SQL syntax.\n"
"Usage:\n"
"    tdbQuery sqlStatement\n"
"Where the SQL statement is enclosed in quotations to avoid the shell interpreting it.\n"
"Only a very restricted subset of a single SQL statement (select) is supported.   Examples:\n"
"    tdbQuery \"select count(*) from hg18\"\n"
"counts all of the tracks in hg18 and prints the results to stdout\n"
"   tdbQuery \"select count(*) from *\"\n"
"counts all tracks in all databases.\n"
"   tdbQuery \"select  track,shortLabel from hg18 where type like 'bigWig%%'\"\n"
"prints to stdout a a two field .ra file containing just the track and shortLabels of bigWig \n"
"type tracks in the hg18 version of trackDb.\n"
"   tdbQuery \"select * from hg18 where track='knownGene' or track='ensGene'\"\n"
"prints the hg18 knownGene and ensGene track's information to stdout.\n"
"   tdbQuery \"select *Label from mm9\"\n"
"prints all fields that end in 'Label' from the mm9 trackDb.\n"
"OPTIONS:\n"
"   -root=/path/to/trackDb/root/dir\n"
"Sets the root directory of the trackDb.ra directory hierarchy to be given path.\n"
"   -file=someFile.ra\n"
"The file option  makes the system use the given .ra file instead of the trackDb.ra directory \n"
"hierarchy\n"
"   -check\n"
"Check that trackDb is internally consistent.  Prints diagnostic output to stderr and aborts if \n"
"there's problems.\n"
"   -strict\n"
"Mimic -strict option on hgTrackDb. Suppresses tracks where corresponding table does not exist."
"   -alpha\n"
"Do checking on release alpha (and not release beta) tracks\n"
);
}


static struct optionSpec options[] = {
   {"root", OPTION_STRING},
   {"file", OPTION_STRING},
   {"check", OPTION_BOOLEAN},
   {"strict", OPTION_BOOLEAN},
   {"alpha", OPTION_BOOLEAN},
   {NULL, 0},
};

struct dbPath
/* A database directory and path. */
    {
    struct dbPath *next;
    char *db;
    char *dir;
    };

static struct dbPath *getDbPathList(char *rootDir)
/* Get list of all "database" directories with any trackDb.ra files two under us. */
{
char *root = simplifyPathToDir(rootDir);
struct dbPath *pathList = NULL, *path;
struct fileInfo *org, *orgList = listDirX(root, "*", TRUE);
for (org = orgList; org != NULL; org = org->next)
    {
    if (org->isDir)
        {
	struct fileInfo *db, *dbList = listDirX(org->name, "*", TRUE);
	for (db = dbList; db != NULL; db = db->next)
	    {
	    if (db->isDir)
	        {
		char trackDbPath[PATH_LEN];
		safef(trackDbPath, sizeof(trackDbPath), "%s/trackDb.ra", db->name);
		if (fileExists(trackDbPath))
		    {
		    AllocVar(path);
		    path->dir = cloneString(db->name);
		    char *s = strrchr(db->name, '/');
		    assert(s != NULL);
		    path->db = cloneString(s+1);
		    slAddHead(&pathList, path);
		    }
		}
	    }
	slFreeList(&dbList);
	}
    }
slFreeList(&orgList);
slReverse(&pathList);
freez(&root);
return pathList;
}


static struct slName *dbPathToFiles(struct dbPath *p)
/* Convert dbPath to a list of files. */
{
struct slName *pathList = NULL;
char *dbDir = p->dir;
char *relPaths = "../../trackDb.ra ../trackDb.ra trackDb.ra"; 
char *buf = cloneString(relPaths);
char *line = buf, *word;
while ((word = nextWord(&line)) != NULL)
    {
    char relDir[PATH_LEN], relFile[PATH_LEN], relSuffix[PATH_LEN];
    splitPath(word, relDir, relFile, relSuffix);
    char dir[PATH_LEN];
    safef(dir, sizeof(dir), "%s/%s", dbDir, relDir);
    char *path = simplifyPathToDir(dir);
    char pattern[PATH_LEN];
    safef(pattern, sizeof(pattern), "%s%s", relFile, relSuffix);
    struct fileInfo *fi, *fiList = listDirX(path, pattern, TRUE);
    for (fi = fiList; fi != NULL; fi = fi->next)
	slNameAddHead(&pathList, fi->name);
    freeMem(path);
    slFreeList(&fiList);
    }
freeMem(buf);
slReverse(&pathList);
return pathList;
}

struct dbPath *dbPathFind(struct dbPath *list, char *db)
/* Return element on list corresponding to db, or NULL if it doesn't exist. */
{
struct dbPath *p;
for (p=list; p != NULL; p = p->next)
    if (sameString(p->db, db))
        break;
return p;
}

boolean filterOnRelease(struct tdbRecord *record, boolean alpha, struct lineFile *lf)
/* Look for a release tag, and return FALSE if it doesn't match alpha status. */
{
struct tdbField *releaseField = tdbRecordField(record, "release");
if (releaseField == NULL)
    return TRUE;
char *release = releaseField->val;
if (sameString(release, "alpha"))
    return alpha;
else if (sameString(release, "beta"))
    return !alpha;
else
    {
    errAbort("Unrecognized release value %s in stanza ending %d of %s", 
    	release, lf->lineIx, lf->fileName);
    return FALSE;
    }
}

static void checkDupeFields(struct tdbRecord *record, struct lineFile *lf)
/* Make sure that each field in record is unique. */
{
struct hash *uniqHash = hashNew(0);
struct tdbField *field;
for (field = record->fieldList; field != NULL; field = field->next)
    {
    if (hashLookup(uniqHash, field->name))
        errAbort("Duplicate tag %s in record ending line %d of %s", field->name,
		lf->lineIx, lf->fileName);
    hashAdd(uniqHash, field->name, NULL);
    }
hashFree(&uniqHash);
}

static void checkDupeKeys(struct tdbRecord *recordList)
/* Make sure that there are no duplicate records (with keys) */
{
struct tdbRecord *record;
struct hash *uniqHash = hashNew(0);
for (record = recordList; record != NULL; record = record->next)
    {
    if (record->key != NULL)
	{
	struct tdbRecord *oldRecord = hashFindVal(uniqHash, record->key);
	if (oldRecord != NULL)
	    {
	    struct tdbFilePos *oldPos = oldRecord->posList;
	    struct tdbFilePos *newPos = record->posList;
	    if (sameString(oldPos->fileName, newPos->fileName))
		errAbort("Duplicate tracks %s ending lines %d and %d of %s",
		    oldRecord->key, oldPos->lineIx, newPos->lineIx, oldPos->fileName);
	    else
		errAbort("Duplicate tracks %s ending lines %d of %s and %d of %s",
		    oldRecord->key, oldPos->lineIx, oldPos->fileName, 
		    newPos->lineIx, newPos->fileName);
	    }
	hashAdd(uniqHash, record->key, record);
	}
    }
hashFree(&uniqHash);
}

static void recurseThroughIncludes(char *fileName, struct lm *lm, 
	struct hash *circularHash,  struct tdbRecord **pRecordList)
/* Recurse through include files. */
{
struct tdbRecord *record;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
while ((record = tdbRecordReadOne(lf, "track", lm)) != NULL)
    {
    struct tdbField *firstField = record->fieldList;
    if (sameString(firstField->name, "include"))
	{
	struct tdbField *field;
	for (field = firstField; field != NULL; field = field->next)
	    {
	    if (!sameString(field->name, "include"))
	       {
	       errAbort("Non-include tag %s in an include stanza ending line %d of %s", 
		    field->name, lf->lineIx, lf->fileName);
	       }
	    char *relPath = field->val;
	    char dir[PATH_LEN], name[FILENAME_LEN], extension[FILEEXT_LEN];
	    splitPath(lf->fileName, dir, NULL, NULL);
	    char includeName[PATH_LEN];
	    safef(includeName, sizeof(includeName), "%s%s", dir, relPath);
	    if (hashLookup(circularHash, includeName))
		{
		errAbort("Including file %s in an infinite loop line %d of %s", 
			includeName, lf->lineIx, lf->fileName);
		}
	    recurseThroughIncludes(includeName, lm, circularHash, pRecordList);
	    }
	}
    else
	{
	checkDupeFields(record, lf);
	if (record->key != NULL)
	    {
	    if (filterOnRelease(record, clAlpha, lf))
		{
		record->posList = tdbFilePosNew(lm, fileName, lf->lineIx);
		slAddHead(pRecordList, record);
		}
	    }
	}
    }
lineFileClose(&lf);
}

struct tdbRecord *readStartingFromFile(char *fileName, struct lm *lm)
/* Read in records from file and any files included from it. */
{
struct tdbRecord *recordList = NULL;
struct hash *circularHash = hashNew(0);
recurseThroughIncludes(fileName, lm, circularHash, &recordList);
hashAdd(circularHash, fileName, NULL);
hashFree(&circularHash);
slReverse(&recordList);
checkDupeKeys(recordList);
return recordList;
}

static void mergeRecords(struct tdbRecord *old, struct tdbRecord *record, char *key, struct lm *lm)
/* Merge record into old,  updating any old fields with new record values. */
{
struct tdbField *field;
for (field = record->fieldList; field != NULL; field = field->next)
    {
    if (!sameString(field->name, key))
	{
	struct tdbField *oldField = tdbRecordField(old, field->name);
	if (oldField != NULL)
	    oldField->val = field->val;
	else
	    {
	    lmAllocVar(lm, oldField);
	    oldField->name = field->name;
	    oldField->val = field->val;
	    slAddTail(&old->fieldList, oldField);
	    }
	}
    }
old->posList = slCat(old->posList, record->posList);
}

void tdbQuery(char *sql)
/* tdbQuery - Query the trackDb system using SQL syntax.. */
{
/* Parse out sql statement. */
struct lineFile *lf = lineFileOnString("query", TRUE, cloneString(sql));
struct rqlStatement *rql = rqlStatementParse(lf);
lineFileClose(&lf);

/* Figure out list of databases to work on. */
struct slRef *dbOrderList = NULL, *dbOrder;
struct dbPath *db, *dbList = getDbPathList(clRoot);
struct slName *t;
for (t = rql->tableList; t != NULL; t = t->next)
    {
    for (db = dbList; db!= NULL; db = db->next)
        {
	if (wildMatch(t->name, db->db))
	    refAdd(&dbOrderList, db);
	}
    }
uglyf("%d databases in from clause\n", slCount(dbOrderList));

/* Loop through each database. */
for (dbOrder = dbOrderList; dbOrder != NULL; dbOrder = dbOrder->next)
    {
    struct lm *lm = lmInit(0);
    struct dbPath *p = dbOrder->val;
    struct slName *fileLevelList = dbPathToFiles(p), *fileLevel;

    /* Assemble recordList and record hash from the root/organism/assembly levels */
    struct hash *recordHash = hashNew(0);
    struct tdbRecord *recordList = NULL;
    for (fileLevel = fileLevelList; fileLevel != NULL; fileLevel = fileLevel->next)
        {
	char *fileName = fileLevel->name;
	struct tdbRecord *fileRecords = readStartingFromFile(fileName, lm);
	uglyf("Read %d records starting from %s\n", slCount(fileRecords), fileName);
	struct tdbRecord *record, *nextRecord;
	for (record = fileRecords; record != NULL; record = nextRecord)
	    {
	    nextRecord = record->next;
	    char *key = record->key;
	    struct tdbRecord *oldRecord = hashFindVal(recordHash, key);
	    if (oldRecord != NULL)
	        {
		if (!record->override)
		    {
		    oldRecord->fieldList = record->fieldList;
		    oldRecord->posList = record->posList;
		    oldRecord->settingsByView = record->settingsByView;
		    oldRecord->subGroups = record->subGroups;
		    oldRecord->view = record->view;
		    oldRecord->viewHash = record->viewHash;
		    }
		else
		    mergeRecords(oldRecord, record, key, lm);
		}
	    else
		{
		hashAdd(recordHash, record->key, record);
		slAddHead(&recordList, record);
		}
	    }
	}
    slReverse(&recordList);
    uglyf("Composed %d records from %s\n", slCount(recordList), p->db);


    lmCleanup(&lm);
    }

rqlStatementFree(&rql);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
clRoot = optionVal("root", clRoot);
clFile = optionVal("file", clFile);
clCheck = optionExists("check");
clStrict = optionExists("strict");
clAlpha = optionExists("alpha");
tdbQuery(argv[1]);
return 0;
}
