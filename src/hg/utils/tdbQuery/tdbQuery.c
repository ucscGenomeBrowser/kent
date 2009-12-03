/* tdbQuery - Query the trackDb system using SQL syntax.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "obscure.h"
#include "portable.h"
#include "errabort.h"
#include "tdbRecord.h"
#include "hdb.h"  /* Just for strict option. */
#include "rql.h"

static char const rcsid[] = "$Id: tdbQuery.c,v 1.13 2009/12/03 20:28:58 kent Exp $";

static char *clRoot = "~/kent/src/hg/makeDb/trackDb";	/* Root dir of trackDb system. */
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
"Sets the root directory of the trackDb.ra directory hierarchy to be given path. By default\n"
"this is ~/kent/src/hg/makeDb/trackDb.\n"
"   -check\n"
"Check that trackDb is internally consistent.  Prints diagnostic output to stderr and aborts if \n"
"there's problems.\n"
"   -strict\n"
"Mimic -strict option on hgTrackDb. Suppresses tracks where corresponding table does not exist.\n"
"   -alpha\n"
"Do checking on release alpha (and not release beta) tracks"
);
}


static struct optionSpec options[] = {
   {"root", OPTION_STRING},
   {"check", OPTION_BOOLEAN},
   {"strict", OPTION_BOOLEAN},
   {"alpha", OPTION_BOOLEAN},
   {NULL, 0},
};

#define glKeyField "track"	 /* The field that has the record ID */
#define glParentField "subTrack" /* The field that points to the parent. */

void recordLocationReport(struct tdbRecord *rec, FILE *out)
/* Write out where record ends. */
{
struct tdbFilePos *pos;
for (pos = rec->posList; pos != NULL; pos = pos->next)
    fprintf(out, "in track %s stanza ending line %d of %s\n", rec->key, pos->lineIx, pos->fileName);
}

void recordWarn(struct tdbRecord *rec, char *format, ...)
/* Issue a warning message. */
{
va_list args;
va_start(args, format);
vaWarn(format, args);
va_end(args);
recordLocationReport(rec, stderr);
}

void recordAbort(struct tdbRecord *rec, char *format, ...)
/* Issue a warning message. */
{
va_list args;
va_start(args, format);
vaWarn(format, args);
va_end(args);
recordLocationReport(rec, stderr);
noWarnAbort();
}

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

boolean recordMatchesRelease(struct tdbRecord *record, boolean alpha)
/* Return TRUE if record is compatible with release.  Pass alpha TRUE for
 * alpha release, else will do beta release.  Records with no release
 * tag are compatible with either release. */
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
    recordAbort(record, "Unrecognized release value %s", release);
    return FALSE;
    }
}

struct tdbRecord *filterOnRelease(struct tdbRecord *list, boolean alpha)
/* Return release-filtered version of list. */
{
struct tdbRecord *newList = NULL;
struct tdbRecord *record, *next;
for (record = list; record != NULL; record = next)
    {
    next = record->next;
    if (recordMatchesRelease(record, alpha))
        {
	slAddHead(&newList, record);
	}
    }
slReverse(&newList);
return newList;
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

static struct tdbField *findFieldInSelfOrParents(struct tdbRecord *record, char *fieldName)
/* Find field if it exists in self or ancestors. */
{
struct tdbRecord *p;
for (p = record; p != NULL; p = p->parent)
    {
    struct tdbField *field = tdbRecordField(p, fieldName);
    if (field != NULL)
        return field;
    }
return NULL;
}

static char *findFieldValInSelfOrParents(struct tdbRecord *record, char *fieldName)
/* Find value of given field if it exists in self or ancestors.  Return NULL if
 * field does not exist. */
{
struct tdbField *field = findFieldInSelfOrParents(record, fieldName);
return (field != NULL ? field->val : NULL);
}

static void checkDupeKeys(struct tdbRecord *recordList, boolean checkRelease)
/* Make sure that there are no duplicate records (with keys) */
{
struct tdbRecord *record;
struct hash *uniqHash = hashNew(0);
for (record = recordList; record != NULL; record = record->next)
    {
    char *key = record->key;
    if (key != NULL)
	{
	struct hashEl *hel;
	for (hel = hashLookup(uniqHash, key); hel != NULL; hel = hashLookupNext(hel))
	    {
	    struct tdbRecord *oldRecord = hel->val;
	    struct tdbFilePos *oldPos = oldRecord->posList;
	    struct tdbFilePos *newPos = record->posList;
	    boolean doAbort = TRUE;
	    if (checkRelease)
		{
		doAbort = FALSE;
		char *oldRelease = findFieldValInSelfOrParents(oldRecord, "release");
		char *newRelease = findFieldValInSelfOrParents(record, "release");
		if (oldRelease == NULL || newRelease == NULL)
		    doAbort = TRUE;
		else
		    {
		    if (sameString(oldRelease, newRelease))
		       doAbort = TRUE;
		    }
		}
	    if (doAbort)
		{
		char *oldRelease = NULL;
		struct tdbField *oldField = tdbRecordField(oldRecord, "release");
		if (oldField) oldRelease = oldField->val;
		char *newRelease = NULL;
		struct tdbField *newField = tdbRecordField(record, "release");
		if (newField) newRelease = newField->val;
		if (newRelease == NULL && oldRelease != NULL)
		    {
		    errAbort("Have release tag for track %s at line %d of %s, but not "
		    	     "at line %d of %s", 
			     key, oldPos->lineIx, oldPos->fileName,
			     newPos->lineIx, newPos->fileName);
		    }
		else if (oldRelease == NULL && newRelease != NULL)
		    {
		    errAbort("Have release tag for track %s at line %d of %s, but not "
		    	     "at line %d of %s", 
			     key, newPos->lineIx, newPos->fileName,
			     oldPos->lineIx, oldPos->fileName);
		    }
		else
		    {
		    if (sameString(oldPos->fileName, newPos->fileName))
			{
			errAbort("Duplicate tracks %s ending lines %d and %d of %s",
			    key, oldPos->lineIx, newPos->lineIx, oldPos->fileName);
			}
		    else
			errAbort("Duplicate tracks %s ending lines %d of %s and %d of %s",
			    key, oldPos->lineIx, oldPos->fileName, 
			    newPos->lineIx, newPos->fileName);
		    }
		}
	    }
	hashAdd(uniqHash, key, record);
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
char *lmFileName = lmCloneString(lm, fileName);
while ((record = tdbRecordReadOne(lf, glKeyField, lm)) != NULL)
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
	    char dir[PATH_LEN];
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
	    record->posList = tdbFilePosNew(lm, lmFileName, lf->lineIx);
	    slAddHead(pRecordList, record);
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

static void overrideFieldFromFile(struct tdbRecord *recordList, 
	char *raFile, char *fieldName, struct lm *lm)
/* Look for raFile in assembly and organism directories in that order.  Use the first
 * file that you find to override the given field inside of recordList. */
{
/* Build up hash of recordList. */
struct hash *hash = hashNew(0);
struct tdbRecord *record;
for (record = recordList; record != NULL; record = record->next)
    hashAdd(hash, record->key, record);

struct lineFile *lf = lineFileOpen(raFile, TRUE);
while ((record = tdbRecordReadOne(lf, glKeyField, lm)) != NULL)
    {
    struct tdbRecord *oldRecord = hashFindVal(hash, record->key);
    if (oldRecord == NULL)
        {
	continue;
	}
    if (slCount(record->fieldList) != 2)
        {
	errAbort("Expecting just two fields, track and %s, got %d in record ending line %d of %s",
		fieldName, slCount(record->fieldList), lf->lineIx, lf->fileName);
	}
    struct tdbField *field = tdbRecordField(record, fieldName);
    if (field == NULL)
        {
	errAbort("Missing %s tag in record ending line %d of %s", fieldName,
		lf->lineIx, lf->fileName);
	}
    mergeRecords(oldRecord, record, glKeyField, lm);
    }
lineFileClose(&lf);
hashFree(&hash);
}

static void overrideFieldFromFileOnPath(struct tdbRecord *recordList, struct dbPath *p,
	char *raFile, char *field, struct lm *lm)
/* Look for raFile in assembly and organism directories in that order.  Use the first
 * file that you find to override the given field inside of recordList. */
{
/* Find raFile. */
char path[PATH_LEN];
safef(path, sizeof(path), "%s/%s", p->dir, raFile);
if (!fileExists(path))
    {
    char orgDir[PATH_LEN];
    splitPath(p->dir, orgDir, NULL, NULL);
    safef(path, sizeof(path), "%s%s", orgDir, raFile);
    if (!fileExists(path))
        return;		/* Nothing to do. */
    }

overrideFieldFromFile(recordList, path, field, lm);
}

static void overridePrioritiesAndVisibilities(struct tdbRecord *recordList, struct dbPath *p,
	struct lm *lm)
/* Look for visibility.ra and priority.ra files and layer them onto recordList. */
{
overrideFieldFromFileOnPath(recordList, p, "visibility.ra", "visibility", lm);
overrideFieldFromFileOnPath(recordList, p, "priority.ra", "priority", lm);
}

static int parentChildFileDistance(struct tdbRecord *parent, struct tdbRecord *child)
/* Return distance of two records.  If they're in different files the
 * distance gets pretty big.  Would be flaky on records split across
 * different files, hence the ad-hoc in the name.  Not worth implementing
 * somthing that handles this though with the hope that the parent/child
 * relationship will become indentation rather than ID based. */
{
struct tdbFilePos *parentFp = parent->posList, *childFp = child->posList;
if (!sameString(parentFp->fileName, childFp->fileName))
    return BIGNUM/2;
int distance = childFp->lineIx - parentFp->lineIx;
if (distance < 0)
    return BIGNUM/4 - distance;
return distance;
}

static struct tdbRecord *findParent(struct tdbRecord *rec, 
	char *parentFieldName, struct hash *hash, boolean alpha)
/* Find parent record if possible.  This is a bit complicated by wanting to
 * match parents and children from the same release if possible.  Our
 * strategy is to just ignore records from the wrond release. */
{
struct tdbField *parentField = tdbRecordField(rec, parentFieldName);
if (parentField == NULL)
    return NULL;
#ifdef OLD
if (!recordMatchesRelease(rec, alpha))
    return NULL;
#endif /* OLD */
char *parentLine = parentField->val;
int len = strlen(parentLine);
char buf[len+1];
strcpy(buf, parentLine);
char *parentName = firstWordInLine(buf);
struct hashEl *hel;
boolean gotParentSomeRelease = FALSE;
struct tdbRecord *closestParent = NULL;
int closestDistance = BIGNUM;
for (hel = hashLookup(hash, parentName); hel != NULL; hel = hashLookupNext(hel))
    {
    gotParentSomeRelease = TRUE;
    struct tdbRecord *parent = hel->val;
#ifdef OLD
    if (recordMatchesRelease(parent, alpha))
#endif /* OLD */
	{
	int distance = parentChildFileDistance(parent, rec);
	if (distance < closestDistance)
	    {
	    closestParent = parent;
	    closestDistance = distance;
	    }
	}
    }
if (closestParent != NULL)
    return closestParent;

/* If we haven't matched so far, it could be that the release tag is set in the parent
 * but not in us, and the parent is not our release parent.  In this case we go ahead
 * and return the out-of-release parent, so we can inherit the out-of-release release
 * tag, so we get filtered out! */
struct tdbField *releaseField = tdbRecordField(rec, "release");
if (gotParentSomeRelease && releaseField == NULL)
     {
     struct tdbRecord *parent = hashFindVal(hash, parentName);
     assert(parent != NULL);
     return parent;
     }
recordWarn(rec, "%s is a subTrack of %s, but %s doesn't exist", rec->key,
    parentField->val, parentField->val);
return NULL;
}

static void linkUpParents(struct tdbRecord *list, char *parentField, boolean alpha)
/* Link up records according to parent/child relationships. */
{
/* Zero out children, parent, and older sibling fields, since going to recalculate
 * them and need lists to start out empty. */
struct tdbRecord *rec;
for (rec = list; rec != NULL; rec = rec->next)
    rec->parent = rec->olderSibling = rec->children = NULL;

/* Build up hash of records indexed by key field. */
struct hash *hash = hashNew(0);
for (rec = list; rec != NULL; rec = rec->next)
    {
    if (rec->key != NULL)
	hashAdd(hash, rec->key, rec);
    }

/* Scan through linking up parents. */
for (rec = list; rec != NULL; rec = rec->next)
    {
    struct tdbRecord *parent = findParent(rec, parentField, hash, alpha);
    if (parent != NULL)
	{
	rec->parent = parent;
	rec->olderSibling = parent->children;
	parent->children = rec;
	}
    }

hashFree(&hash);
}


struct tdbRecord *tdbsForDbPath(struct dbPath *p, struct lm *lm, struct hash *recordHash,
	char *parentField, boolean alpha)
/* Assemble recordList for given database.  This looks at the root/organism/assembly
 * levels.  It returns a list, and fills in a hash (which should be passed in empty)
 * of the records keyed by record->key. */
{
struct slName *fileLevelList = dbPathToFiles(p), *fileLevel;
struct tdbRecord *recordList = NULL;
for (fileLevel = fileLevelList; fileLevel != NULL; fileLevel = fileLevel->next)
    {
    char *fileName = fileLevel->name;
    struct tdbRecord *fileRecords = readStartingFromFile(fileName, lm);
    verbose(2, "Read %d records starting from %s\n", slCount(fileRecords), fileName);
    linkUpParents(fileRecords, parentField, alpha);
    checkDupeKeys(fileRecords, TRUE);
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
		mergeRecords(oldRecord, record, glKeyField, lm);
	    }
	else
	    {
	    hashAdd(recordHash, record->key, record);
	    slAddHead(&recordList, record);
	    }
	}
    }
slReverse(&recordList);

return recordList;
}

static void mergeParentRecord(struct tdbRecord *record, struct tdbRecord *parent, 
	struct lm *lm)
/* Merge in parent record.  This only updates fields that are in parent but not record. */
{
struct tdbField *parentField;
for (parentField= parent->fieldList; parentField!= NULL; parentField= parentField->next)
    {
    struct tdbField *oldField = tdbRecordField(record, parentField->name);
    if (oldField == NULL)
        {
	struct tdbField *newField;
	lmAllocVar(lm, newField);
	newField->name = parentField->name;
	newField->val = parentField->val;
	slAddTail(&record->fieldList, newField);
	}
    }
}


static void inheritFromParents(struct tdbRecord *list, char *parentField, char *noInheritField,
	boolean alpha, struct lm *lm)
/* Go through list.  If an element has a parent field, then fill in non-existent fields from
 * parent and from view with settings defined in parent. */
{
linkUpParents(list, parentField, alpha);

/* Scan through doing inheritance. */
struct tdbRecord *rec;
for (rec = list; rec != NULL; rec = rec->next)
    {
    /* First inherit from view. */
    char *viewName = rec->view;
    if (viewName != NULL)
        {
	struct slPair *view;
	if (rec->parent == NULL)
	     {
	     verbose(2, "%s has view %s but no parent\n", rec->key, viewName);
	     continue;
	     }
	for (view = rec->parent->settingsByView; view != NULL; view = view->next)
	    {
	    if (sameString(view->name, viewName))
		break;
	    else 
	        {
		if (rec->parent->viewHash != NULL)
		    {
		    char *alias = hashFindVal(rec->parent->viewHash, viewName);
		    if (alias != NULL)
			if (sameString(view->name, alias))
			    break;
		    }
		}
	    }
	if (view != NULL)
	    {
	    struct slPair *setting;
	    for (setting = view->val; setting != NULL; setting = setting->next)
	        {
		struct tdbField *oldField = tdbRecordField(rec, setting->name);
		if (oldField == NULL)
		    {
		    struct tdbField *newField;
		    lmAllocVar(lm, newField);
		    newField->name = lmCloneString(lm, setting->name);
		    newField->val = lmCloneString(lm, setting->val);
		    slAddTail(&rec->fieldList, newField);
		    }
		}
	    }
	else 
	    {
	    verbose(3, "view %s not in parent settingsByView of %s\n", viewName, rec->key);
	    }
	}

    /* Then inherit from parents. */
    struct tdbRecord *parent;
    for (parent = rec->parent; parent != NULL; parent = parent->parent)
	{
	mergeParentRecord(rec, parent, lm);
	}
    }
}

static char *lookupField(void *record, char *key)
/* Lookup a field in a tdbRecord. */
{
struct tdbRecord *tdb = record;
struct tdbField *field = tdbRecordField(tdb, key);
if (field == NULL)
    return NULL;
else
    return field->val;
}

static boolean rqlStatementMatch(struct rqlStatement *rql, struct tdbRecord *tdb,
	struct lm *lm)
/* Return TRUE if where clause and tableList in statement evaluates true for tdb. */
{
struct rqlParse *whereClause = rql->whereClause;
if (whereClause == NULL)
    return TRUE;
else
    {
    struct rqlEval res = rqlEvalOnRecord(whereClause, tdb, lookupField, lm);
    res = rqlEvalCoerceToBoolean(res);
    return res.val.b;
    }
}

static void rqlStatementOutput(struct rqlStatement *rql, struct tdbRecord *tdb, 
	char *addFileField, FILE *out)
/* Output fields  from tdb to file.  If addFileField is non-null add a new
 * field with this name at end of output. */
{
struct slName *fieldList = rql->fieldList, *field;
for (field = fieldList; field != NULL; field = field->next)
    {
    struct tdbField *r;
    boolean doWild = anyWild(field->name);
    for (r = tdb->fieldList; r != NULL; r = r->next)
        {
	boolean match;
	if (doWild)
	    match = wildMatch(field->name, r->name);
	else
	    match = (strcmp(field->name, r->name) == 0);
	if (match)
	    fprintf(out, "%s %s\n", r->name, r->val);
	}
    }
fprintf(out, "\n");
}

static boolean tableExistsInSelfOrOffspring(char *db, struct tdbRecord *record)
/* Return TRUE if table corresponding to track exists in database db.  If a parent
 * track look for tables in kids too. */
{
if ( hTableOrSplitExists(db, record->key))
    return TRUE;
struct tdbRecord *child;
for (child = record->children; child != NULL; child = child->next)
    {
    if (tableExistsInSelfOrOffspring(db, child))
        return TRUE;
    }
return FALSE;
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
verbose(2, "%d databases in from clause\n", slCount(dbOrderList));

/* Loop through each database. */
int matchCount = 0;
struct dyString *fileString = dyStringNew(0);  /* Buffer for file field. */
for (dbOrder = dbOrderList; dbOrder != NULL; dbOrder = dbOrder->next)
    {
    struct lm *lm = lmInit(0);
    struct dbPath *p = dbOrder->val;
    char *db = p->db;
    struct hash *recordHash = hashNew(0);
    struct tdbRecord *recordList = tdbsForDbPath(p, lm, recordHash, "subTrack", clAlpha);

    verbose(2, "Composed %d records from %s\n", slCount(recordList), db);
    inheritFromParents(recordList, "subTrack", "noInherit", clAlpha, lm);
    recordList = filterOnRelease(recordList, clAlpha);
    verbose(2, "After filterOnRelease %d records\n", slCount(recordList));
    checkDupeKeys(recordList, FALSE);

    overridePrioritiesAndVisibilities(recordList, p, lm);

    struct tdbRecord *record;
    boolean doSelect = sameString(rql->command, "select");
    for (record = recordList; record != NULL; record = record->next)
	{
	/* Add "db" field, making sure it doesn't already exist. */
	struct tdbField *dbField = tdbRecordField(record, "db");
	if (dbField != NULL)
	    recordAbort(record, "using reserved field 'db'");
	dbField = tdbFieldNew("db", db, lm);
	slAddHead(&record->fieldList, dbField);

	/* Add "filePos" field, making sure it doesn't already exist. */
	struct tdbField *fileField = tdbRecordField(record, "filePos");
	if (fileField != NULL)
	    recordAbort(record, "using reserved field 'filePos'");
	struct tdbFilePos *fp;
	dyStringClear(fileString);
	for (fp = record->posList; fp != NULL; fp = fp->next)
	    dyStringPrintf(fileString, " %s %d", fp->fileName, fp->lineIx);
	fileField = tdbFieldNew("filePos", fileString->string, lm);
	slAddTail(&record->fieldList, fileField);


	if (rqlStatementMatch(rql, record, lm))
	    {
	    if (!clStrict || tableExistsInSelfOrOffspring(p->db, record))
		{
		matchCount += 1;
		if (doSelect)
		    {
		    rqlStatementOutput(rql, record, "file", stdout);
		    }
		if (rql->limit >= 0 && matchCount >= rql->limit)
		    break;
		}
	    }
	}
    lmCleanup(&lm);
    hashFree(&recordHash);
    }
dyStringFree(&fileString);

if (sameString(rql->command, "count"))
    printf("%d\n", matchCount);

rqlStatementFree(&rql);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
clRoot = optionVal("root", clRoot);
clCheck = optionExists("check");
clStrict = optionExists("strict");
clAlpha = optionExists("alpha");
tdbQuery(argv[1]);
return 0;
}
