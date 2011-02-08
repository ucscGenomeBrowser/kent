/* tdbQuery - Query the trackDb system using SQL syntax.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "dystring.h"
#include "obscure.h"
#include "portable.h"
#include "errabort.h"
#include "trackDb.h"
#include "tdbRecord.h"
#include "ra.h"
#include "hdb.h"  /* Just for strict option. */
#include "rql.h"

static char const rcsid[] = "$Id: tdbQuery.c,v 1.31 2010/03/13 01:28:10 braney Exp $";

static char *clRoot = "~/kent/src/hg/makeDb/trackDb";	/* Root dir of trackDb system. */
static boolean clCheck = FALSE;		/* If set perform lots of checks on input. */
static boolean clStrict = FALSE;	/* If set only return tracks with actual tables. */

#define	RELEASE_ALPHA  (1 << 0)
#define	RELEASE_BETA   (1 << 1)
#define	RELEASE_PUBLIC (1 << 2)
static char *release = "alpha";
static unsigned releaseBit = RELEASE_ALPHA;

static boolean clNoBlank = FALSE;	/* If set suppress blank lines in output. */
static char *clRewrite = NULL;		/* Rewrite to given directory. */
static boolean clNoCompSub = FALSE;	/* If set don't do subtrack inheritence of fields. */

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
"   -release=alpha|beta|public\n"
"Include trackDb entries with this release tag only. Default is alpha.\n"
"   -noBlank\n"
"Don't print out blank lines separating records\n"
"   -noCompSub\n"
"Subtracks don't inherit fields from parents\n"
);
}


static struct optionSpec options[] = {
   {"root", OPTION_STRING},
   {"check", OPTION_BOOLEAN},
   {"strict", OPTION_BOOLEAN},
   {"release", OPTION_STRING},
   {"noBlank", OPTION_BOOLEAN},
   {"rewrite", OPTION_STRING},
   {"noCompSub", OPTION_BOOLEAN},
   {NULL, 0},
};

#define glKeyField "track"	 /* The field that has the record ID */

struct hash *glTagTypes = NULL;	/* Hash of tagTypes file keyed by tag. */
char glTagTypeFile[PATH_LEN];	/* File name of tagTypes.tab including dir. */

// Specialized wildHash could be added to hash.c, but will be so rarely used.
// It's purpose here is for wildCard tagTypes (e.g. "*Filter") which get
// loaded into an RA hash but require specialized hashFindVal to pick them up.
#define WILD_CARD_HASH_BIN "[wildCardHash]"
#define WILD_CARD_HASH_EMPTY "[]"
int wildExpressionCmp(const void *va, const void *vb)
/* Compare two slPairs. */
{
const struct slPair *a = *((struct slPair **)va);
const struct slPair *b = *((struct slPair **)vb);
return (strlen(a->name) - strlen(b->name));
}

struct slPair *wildHashMakeList(struct hash *hash)
/* Makes a sub hash containing a list of hash elements whose names contain wildcards ('*', '?').
   The sub hash will be put into WILD_CARD_HASH_BIN for use by wildHashLookup(). */
{
struct slPair *wildList = NULL;
struct hashEl* hel = NULL;
struct hashCookie cookie = hashFirst(hash);
while((hel = hashNext(&cookie)) != NULL)
    {
    if (strchr(hel->name,'*') != NULL || strchr(hel->name,'?') != NULL)
        slPairAdd(&wildList, hel->name, hel);
    }
if (wildList == NULL)
    slPairAdd(&wildList, WILD_CARD_HASH_EMPTY, NULL);// Note: adding an "empty" pair will prevent rebulding this list
else if (slCount(wildList) > 1)
    slSort(&wildList,wildExpressionCmp); // sort on length, so the most restrictive wildcard match goes first?

hashAdd(hash, WILD_CARD_HASH_BIN, wildList);
return wildList;
}

struct hashEl *wildHashLookup(struct hash *hash, char *name)
/* If wildcards are in hash, then look up var in "wildCardHash" bin. */
{
struct slPair *wild = hashFindVal(hash, WILD_CARD_HASH_BIN);
if (wild == NULL)  // Hasn't been made yet.
    wild = wildHashMakeList(hash);
if (wild == NULL
|| (slCount(wild) == 1 && sameString(wild->name,WILD_CARD_HASH_EMPTY)))
    return NULL; // Empty list means hash contains no names with wildcards

for ( ;wild != NULL; wild=wild->next)
    if (wildMatch(wild->name,name))
        return wild->val;

return NULL;
}

void *wildHashFindVal(struct hash *hash, char *name)
/* If wildcards are in hash, then look up var in "wildCardHash" bin. */
{
struct hashEl *hel = wildHashLookup(hash,name);
if (hel != NULL)
    return hel->val;
return NULL;
}

struct hashEl *hashLookupEvenInWilds(struct hash *hash, char *name)
/* Lookup hash el but if no exact match look for wildcards in hash and then match. */
{
struct hashEl *hel = hashLookup(hash, name);
if (hel == NULL)
    hel = wildHashLookup(hash, name);
return hel;
}

void *hashFindValEvenInWilds(struct hash *hash, char *name)
/* Find hash val but if no exact match look for wildcards in hash and then match. */
{
void *val = hashFindVal(hash, name);
if (val == NULL)
    val = wildHashFindVal(hash, name);
return val;
}

void recordLocationReport(struct tdbRecord *rec, FILE *out)
/* Write out where record ends. */
{
struct tdbFilePos *pos;
for (pos = rec->posList; pos != NULL; pos = pos->next)
    fprintf(out, "in track %s stanza starting line %d of %s\n", rec->key, pos->startLineIx, pos->fileName);
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

struct hash *readTagTypeHash(char *fileName)
/* Set up tagTypeHash and other stuff needed for checking. */
{
struct hash *hash = hashNew(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
while (lineFileNextReal(lf, &line))
    {
    struct slName *typeList = NULL;
    char *tag = nextWord(&line);
    char *word;
    while ((word = nextWord(&line)) != NULL)
	slNameAddHead(&typeList, word);
    hashAdd(hash, tag, typeList);
    }
lineFileClose(&lf);
return hash;
}

static boolean matchAnyWild(struct slName *wildList, char *s)
/* Return TRUE if s matches any wildcard in list. */
{
struct slName *wild;
for (wild = wildList; wild != NULL; wild = wild->next)
    {
    if (wildMatch(wild->name, s))
        return TRUE;
    }
return FALSE;
}

static void doRqlChecks(struct rqlStatement *rql)
/* Do additional checks on rql statement. */
{
/* Do checks that tags are all legitimate and with correct types. */
struct slName *field;
for (field = rql->fieldList; field != NULL; field = field->next)
    {
    if (!anyWild(field->name))
	if (!hashLookupEvenInWilds(glTagTypes, field->name))
	    errAbort("Field %s in query doesn't exist in %s.", field->name, glTagTypeFile);
    }
struct slName *var;
for (var = rql->whereVarList; var != NULL; var = var->next)
    {
    if (!hashLookupEvenInWilds(glTagTypes, var->name))
        errAbort(
	   "Tag %s doesn't exist. Maybe you mispelled a variable or forgot to put quotes around\n"
	   "a word? Maybe %s is hosed?.",
	    var->name, glTagTypeFile);
    }
}

struct dbPath
/* A database directory and path. */
    {
    struct dbPath *next;
    char *db;
    char *dir;
    };

static struct dbPath *getDbPathList(char *root)
/* Get list of all "database" directories with any trackDb.ra files two under us. */
{
struct dbPath *pathList = NULL, *path;
struct fileInfo *org, *orgList = listDirX(root, "*", TRUE);

/* If in strict mode avoid looking up databases that aren't in mysql. */
struct hash *dbStrictHash = NULL;
if (clStrict)
    {
    struct sqlConnection *conn = sqlConnect("mysql");
    struct slName *db, *dbList = sqlGetAllDatabase(conn);
    dbStrictHash = hashNew(0);
    for (db = dbList; db != NULL; db = db->next)
        hashAdd(dbStrictHash, db->name, NULL);
    sqlDisconnect(&conn);
    slFreeList(&dbList);
    }


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
		    char *s = strrchr(db->name, '/');
		    assert(s != NULL);
		    char *fileOnly = s+1;
		    if (dbStrictHash == NULL || hashLookup(dbStrictHash, fileOnly) != NULL)
			{
			AllocVar(path);
			path->db = cloneString(fileOnly);
			path->dir = cloneString(db->name);
			slAddHead(&pathList, path);
			}
		    }
		}
	    }
	slFreeList(&dbList);
	}
    }
slFreeList(&orgList);
slReverse(&pathList);
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

unsigned buildReleaseBits(struct tdbRecord *record, char *rel)
/* unpack the comma separated list of possible release tags */
{

if (rel == NULL)
    return RELEASE_ALPHA |  RELEASE_BETA |  RELEASE_PUBLIC;

unsigned bits = 0;
while(rel)
    {
    char *end = strchr(rel, ',');

    if (end)
	*end = 0;
    rel = trimSpaces(rel);

    if (sameString(rel, "alpha"))
	bits |= RELEASE_ALPHA;
    else if (sameString(rel, "beta"))
	bits |= RELEASE_BETA;
    else if (sameString(rel, "public"))
	bits |= RELEASE_PUBLIC;
    else
	errAbort("Tracks must have a release combination of alpha, beta, and public on line %d of %s",
		tdbRecordLineIx(record), tdbRecordFileName(record));

    if (end)
	*end++ = ',';
    rel = end;
    }

return bits;
}

boolean recordMatchesRelease( struct tdbRecord *record, unsigned currentReleaseBit)
/* Return TRUE if record is compatible with release. */
{
unsigned bits;
char *release = NULL;
struct tdbField *releaseField = tdbRecordField(record, "release");

if (releaseField != NULL)
    release = releaseField->val;

bits = buildReleaseBits(record, release);
if (bits & currentReleaseBit)
    return TRUE;

return FALSE;
}

boolean compatibleReleases(char *a, char *b)
/* Return TRUE if either a or b is null, or if a and b are the same. */
{
return a == NULL || b == NULL || sameString(a, b);
}

boolean sameKeyCompatibleRelease(struct tdbRecord *a, struct tdbRecord *b)
/* Return TRUE if a and b have the same key and compatible releases. */
{
return sameString(a->key, b->key) &&
	compatibleReleases(tdbRecordFieldVal(a, "release"), tdbRecordFieldVal(b, "release"));
}

struct tdbRecord *filterOnRelease( struct tdbRecord *list, unsigned currentReleaseBit)
/* Return release-filtered version of list. */
{
struct tdbRecord *newList = NULL;
struct tdbRecord *record, *next;
for (record = list; record != NULL; record = next)
    {
    next = record->next;
    if (recordMatchesRelease(record, currentReleaseBit))
        {
	slAddHead(&newList, record);
	}
    }
slReverse(&newList);
return newList;
}

static void addReleaseTag(struct tdbRecord *record, struct lineFile *lf,
    char *releaseTag)
/* make sure there is no existing release tag, and add one if not */
{
struct tdbField *field, *last = NULL;
for (field = record->fieldList; field != NULL; last = field, field = field->next)
    {
    if (sameString(field->name, "release"))
        errAbort("Release tag in stanza with include release override line %d of %s",
		tdbRecordLineIx(record), lf->fileName);

    }
assert(last != NULL);

struct tdbField *releaseField;

AllocVar(releaseField);
last->next = releaseField;
releaseField->name = cloneString("release");
releaseField->val = cloneString(releaseTag);
}

static void checkDupeFields(struct tdbRecord *record, struct lineFile *lf)
/* Make sure that each field in record is unique. */
{
struct hash *uniqHash = hashNew(0);
struct tdbField *field;
for (field = record->fieldList; field != NULL; field = field->next)
    {
    if (hashLookup(uniqHash, field->name))
        errAbort("Duplicate tag %s in record starting line %d of %s", field->name,
		tdbRecordLineIx(record), lf->fileName);
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
		doAbort = compatibleReleases(oldRelease, newRelease);
		}
	    if (doAbort)
		{
		char *oldRelease = tdbRecordFieldVal(oldRecord, "release");
		char *newRelease = tdbRecordFieldVal(record, "release");
		if (newRelease == NULL && oldRelease != NULL)
		    {
		    errAbort("Have release tag for track %s at line %d of %s, but not "
		    	     "at line %d of %s",
			     key, oldPos->startLineIx, oldPos->fileName,
			     newPos->startLineIx, newPos->fileName);
		    }
		else if (oldRelease == NULL && newRelease != NULL)
		    {
		    errAbort("Have release tag for track %s at line %d of %s, but not "
		    	     "at line %d of %s",
			     key, newPos->startLineIx, newPos->fileName,
			     oldPos->startLineIx, oldPos->fileName);
		    }
		else
		    {
		    if (sameString(oldPos->fileName, newPos->fileName))
			{
			errAbort("Duplicate tracks %s starting lines %d and %d of %s",
			    key, oldPos->startLineIx, newPos->startLineIx, oldPos->fileName);
			}
		    else
			errAbort("Duplicate tracks %s starting lines %d of %s and %d of %s",
			    key, oldPos->startLineIx, oldPos->fileName,
			    newPos->startLineIx, newPos->fileName);
		    }
		}
	    }
	hashAdd(uniqHash, key, record);
	}
    }
hashFree(&uniqHash);
}

static void recurseThroughIncludes(char *fileName, struct lm *lm,
	struct hash *circularHash,  struct tdbRecord **pRecordList,
        char *releaseTag)
/* Recurse through include files. */
{
struct tdbRecord *record;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
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
	       errAbort("Non-include tag %s in an include stanza starting line %d of %s",
		    field->name, tdbRecordLineIx(record), lf->fileName);
	       }
	    char dir[PATH_LEN];
	    splitPath(lf->fileName, dir, NULL, NULL);
	    char includeName[PATH_LEN];
            char *words[5];
            int count = chopLine(field->val, words);
            if (count > 2)
                errAbort("Too many words on include line at line %d of %s",
		     tdbRecordLineIx(record), lf->fileName);

	    char *relPath = words[0];
            char *subRelease = NULL;
            if (count == 2)
                {
                subRelease = cloneString(words[1]);
                if (!trackDbCheckValidRelease(subRelease))
                    errAbort("Include with bad release tag %s at line %d of %s",
                        subRelease, tdbRecordLineIx(record), lf->fileName);
                }

            if (subRelease && releaseTag && !sameString(subRelease, releaseTag))
                errAbort("Include with release %s included from include with release %s at line %d of %s",
		     subRelease, releaseTag, tdbRecordLineIx(record),
                     lf->fileName);

	    safef(includeName, sizeof(includeName), "%s%s", dir, relPath);
	    if (hashLookup(circularHash, includeName))
		{
		errAbort("Including file %s in an infinite loop line %d of %s",
			includeName, tdbRecordLineIx(record), lf->fileName);
		}
	    recurseThroughIncludes(includeName, lm, circularHash, pRecordList,
                subRelease);
	    }
	}
    else
	{
	checkDupeFields(record, lf);
        if (releaseTag)
            addReleaseTag(record, lf, releaseTag);
	if (record->key != NULL)
	    {
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
recurseThroughIncludes(fileName, lm, circularHash, &recordList, NULL);
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

static int parentChildFileDistance(struct tdbRecord *parent, struct tdbRecord *child)
/* Return distance of two records.  If they're in different files the
 * distance gets pretty big.  Would be flaky on records split across
 * different files, hence the ad-hoc in the name.  Not worth implementing
 * something that handles this though with the hope that the parent/child
 * relationship will become indentation rather than ID based. */
{
struct tdbFilePos *parentFp = parent->posList, *childFp = child->posList;
if (!sameString(parentFp->fileName, childFp->fileName))
    return BIGNUM/2;
int distance = childFp->startLineIx - parentFp->startLineIx;
if (distance < 0)
    return BIGNUM/4 - distance;
return distance;
}

static struct tdbRecord *findParent(struct tdbRecord *rec,
	char *parentFieldName, struct hash *hash)
/* Find parent record if possible.  This is a bit complicated by wanting to
 * match parents and children from the same release if possible.  Our
 * strategy is to just ignore records from the wrong release. */
{
char *release = tdbRecordFieldVal(rec, "release");
if (clNoCompSub)
    return NULL;
struct tdbField *parentField = tdbRecordField(rec, parentFieldName);
if (parentField == NULL)
    return NULL;
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
    if (compatibleReleases(release, tdbRecordFieldVal(parent, "release")))
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

recordWarn(rec, "parent %s of %s release %s doesn't exist", parentName, rec->key, naForNull(release));
return NULL;
}

static void linkUpParents(struct tdbRecord *list, char *parentField, unsigned currentReleaseBit)
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
    struct tdbRecord *parent = findParent(rec, parentField, hash);
    if (parent != NULL)
	{
	rec->parent = parent;
	rec->olderSibling = parent->children;
	parent->children = rec;
	}
    }

hashFree(&hash);
}


struct tdbRecord *tdbsForDbPath(struct dbPath *p, struct lm *lm,
	char *parentField, unsigned currentReleaseBit)
/* Assemble recordList for given database.  This looks at the root/organism/assembly
 * levels.  It returns a list of records. */
{
struct hash *recordHash = hashNew(0);
struct slName *fileLevelList = dbPathToFiles(p), *fileLevel;
struct tdbRecord *recordList = NULL;
for (fileLevel = fileLevelList; fileLevel != NULL; fileLevel = fileLevel->next)
    {
    char *fileName = fileLevel->name;
    struct tdbRecord *fileRecords = readStartingFromFile(fileName, lm);
    verbose(2, "Read %d records starting from %s\n", slCount(fileRecords), fileName);
    fileRecords = filterOnRelease(fileRecords, currentReleaseBit);
    verbose(2, "After filterOnRelease %d records\n", slCount(fileRecords));
    linkUpParents(fileRecords, parentField, currentReleaseBit);
    checkDupeKeys(fileRecords, TRUE);
    struct tdbRecord *record, *nextRecord;
    for (record = fileRecords; record != NULL; record = nextRecord)
	{
	nextRecord = record->next;
	char *key = record->key;
	struct tdbRecord *oldRecord = hashFindVal(recordHash, key);
	if (oldRecord != NULL && sameKeyCompatibleRelease(record, oldRecord))
	    {
	    if (!record->override)
		{
		oldRecord->fieldList = record->fieldList;
		oldRecord->posList = record->posList;
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
hashFree(&recordHash);
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
	unsigned currentReleaseBit, struct lm *lm)
/* Go through list.  If an element has a parent field, then fill in non-existent fields from
 * parent. */
{
linkUpParents(list, parentField, currentReleaseBit);

/* Scan through doing inheritance. */
struct tdbRecord *rec;
for (rec = list; rec != NULL; rec = rec->next)
    {
    struct tdbRecord *parent;
    for (parent = rec->parent; parent != NULL; parent = parent->parent)
	{
	if (!clNoCompSub)
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
if (!clNoBlank)
    fprintf(out, "\n");
}


static boolean tableExistsInSelfOrOffspring(char *db, struct tdbRecord *record,
	int level, struct slRef *parent)
/* Return TRUE if table corresponding to track exists in database db.  If a parent
 * track look for tables in kids too. */
{
if ( hTableOrSplitExists(db, record->key))
    return TRUE;
struct tdbRecord *child;
if (level > 5)
    {
    struct slRef *ancestor;
    struct dyString *err = dyStringNew(0);
    dyStringPrintf(err, "Heirarchy too deep from %s", record->key);
    for (ancestor=parent; ancestor != NULL; ancestor = ancestor->next)
        {
	struct tdbRecord *a = ancestor->val;
	dyStringPrintf(err, " to %s", a->key);
	}
    recordAbort(record, "%s", err->string);
    }
struct slRef me;
me.next = parent;
me.val = record;
for (child = record->children; child != NULL; child = child->olderSibling)
    {
    if (tableExistsInSelfOrOffspring(db, child, level+1, &me))
        return TRUE;
    }
return FALSE;
}

static int countAncestors(struct tdbRecord *r)
/* Return 0 if has no parent, 1 if has a parent, 2 if it has a grandparent, etc. */
{
int count = 0;
struct tdbRecord *p;
for (p = r->parent; p != NULL; p = p->parent)
    count += 1;
return count;
}

static struct tdbRecord *closestTdbAboveLevel(struct tdbRecord *tdbList,
	struct tdbFilePos *childPos, int parentDepth)
/* Find parent at given depth that comes closest to (but before) childPos. */
{
struct tdbRecord *parent, *closestParent = NULL;
int closestDistance = BIGNUM;
for (parent = tdbList; parent != NULL; parent = parent->next)
    {
    if (countAncestors(parent) <= parentDepth)
	{
	struct tdbFilePos *pos;
	for (pos = parent->posList; pos != NULL; pos = pos->next)
	    {
	    if (sameString(pos->fileName, childPos->fileName))
		{
		int distance = childPos->startLineIx - pos->startLineIx;
		if (distance > 0)
		    {
		    if (distance < closestDistance)
			{
			closestDistance = distance;
			closestParent = parent;
			}
		    }
		}
	    }
	}
    }
return closestParent;
}

static void checkChildUnderNearestParent(struct tdbRecord *recordList, struct tdbRecord *child)
/* Make sure that parent record occurs before child, and that indeed it is the
 * closest parent before the child. */
{
struct tdbRecord *parent = child->parent;
int parentDepth = countAncestors(parent);

/* We do the check for each file the child is in */
struct tdbFilePos *childFp, *parentFp;
for (childFp = child->posList; childFp != NULL; childFp = childFp->next)
    {
    /* Find parentFp that is in this file if any. */
    for (parentFp = parent->posList; parentFp != NULL; parentFp = parentFp->next)
        {
	if (sameString(parentFp->fileName, childFp->fileName))
	    {
	    if (parentFp->startLineIx > childFp->startLineIx)
	        errAbort("Child before parent in %s\n"
		         "Child (%s) at line %d, parent (%s) at line %d",
			 childFp->fileName, child->key, childFp->startLineIx,
			 parent->key, parentFp->startLineIx);
	    struct tdbRecord *closestParent = closestTdbAboveLevel(recordList, childFp,
	    	parentDepth);
	    assert(closestParent != NULL);
	    if (closestParent != parent)
	        errAbort("%s comes between parent (%s) and child (%s) in %s\n"
		         "Parent at line %d, child at line %d.",
			 closestParent->key, parent->key, child->key, childFp->fileName,
			 parentFp->startLineIx, childFp->startLineIx);
	    }
	}
    }
}

static void doRecordChecks(struct tdbRecord *recordList, struct lm *lm)
/* Do additional checks on records. */
{
/* Check fields against tagType.tag. */
struct tdbRecord *record;
for (record = recordList; record != NULL; record = record->next)
    {
    struct tdbField *typeField = tdbRecordField(record, "type");
    char *fullType = (typeField != NULL ? typeField->val : record->key);
    char *type = lmCloneFirstWord(lm, fullType);
    struct tdbField *field;
    for (field = record->fieldList; field != NULL; field = field->next)
        {
	struct slName *typeList = hashFindValEvenInWilds(glTagTypes, field->name);
	if (typeList == NULL)
	    {
	    recordAbort(record,
	    	"Tag '%s' not found in %s.\nIf it's not a typo please add %s to that file.  "
		"The tag is",
	    	field->name, glTagTypeFile, field->name);
	    }
	if (!matchAnyWild(typeList, type))
	    {
	    recordAbort(record,
	    	"Tag '%s' not allowed for tracks of type '%s'.  Please add it to supported types\n"
		"in %s if this is not a mistake.  The tag is",
	    	field->name, type, glTagTypeFile);
	    }
	}
    }

/* Additional child/parent checks. */
for (record = recordList; record != NULL; record = record->next)
    {
    if (record->parent != NULL)
        checkChildUnderNearestParent(recordList, record);
    }
}

void tdbQuery(char *sql)
/* tdbQuery - Query the trackDb system using SQL syntax.. */
{
/* Load in hash of legitimate tags. */
safef(glTagTypeFile, sizeof(glTagTypeFile), "%s/%s", clRoot, "tagTypes.tab");
glTagTypes = readTagTypeHash(glTagTypeFile);

/* Parse out sql statement. */
struct lineFile *lf = lineFileOnString("query", TRUE, cloneString(sql));
struct rqlStatement *rql = rqlStatementParse(lf);
lineFileClose(&lf);
doRqlChecks(rql);

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
    struct tdbRecord *recordList = tdbsForDbPath(p, lm, "parent", releaseBit);


    verbose(2, "Composed %d records from %s\n", slCount(recordList), db);
    inheritFromParents(recordList, "parent", "noInherit", releaseBit, lm);
    linkUpParents(recordList, "parent", releaseBit);
    checkDupeKeys(recordList, FALSE);

    if (clCheck)
        doRecordChecks(recordList, lm);

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
	    dyStringPrintf(fileString, " %s %d", fp->fileName, fp->startLineIx);
	fileField = tdbFieldNew("filePos", fileString->string, lm);
	slAddTail(&record->fieldList, fileField);


	if (rqlStatementMatch(rql, record, lm))
	    {
	    if (!clStrict || tableExistsInSelfOrOffspring(p->db, record, 1, NULL))
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
    }
dyStringFree(&fileString);

if (sameString(rql->command, "count"))
    printf("%d\n", matchCount);

rqlStatementFree(&rql);
}

struct slName *hashPair(char *raFile, char *keyField, char *valField, struct hash **retValHash,
	struct hash **retCommentHash)
/* Read two fields out of a ra file.  For records that have both fields put them into
 * a hash with the logical keys and values, which is returned.  Return list of keys in order. */
{
struct slName *list = NULL;
struct hash *hash = hashNew(0), *commentHash = hashNew(0);
struct lineFile *lf = lineFileMayOpen(raFile, TRUE);
struct dyString *dy = dyStringNew(0);
if (lf != NULL)
    {
    while (raSkipLeadingEmptyLines(lf, dy))
	{
	char *theKey = NULL, *theVal = NULL;
	char *name, *val;
	while (raNextTagVal(lf, &name, &val, NULL))
	    {
	    if (sameString(name, keyField))
	       theKey = lmCloneString(hash->lm, val);
	    else if (sameString(name, valField))
	       theVal = lmCloneString(hash->lm, val);
	    }
	if (theKey && theVal)
	    {
	    hashAdd(hash, theKey, theVal);
	    hashAdd(commentHash, theKey, lmCloneString(hash->lm, dy->string));
	    slNameAddHead(&list, theKey);
	    }
	}
    lineFileClose(&lf);
    }
dyStringFree(&dy);
*retValHash = hash;
*retCommentHash = commentHash;
slReverse(&list);
return list;
}

void overrideOrWriteSelf(char *orig, char *name, char *val,
	char *tagName, char *keyVal, struct hash *hash, struct lineFile *lf, FILE *f)
/* Write out name/val pair. */
{
if (keyVal == NULL)
    errAbort("%s tag before track tag line %d of %s", tagName, lf->lineIx, lf->fileName);
char *newVal = hashFindVal(hash, keyVal);
if (newVal != NULL)
    {
    int leadingSpaces = skipLeadingSpaces(orig) - orig;
    if (orig[0] == '#')
        errAbort("Rats, internal comments, can't deal with it.");
    mustWrite(f, orig, leadingSpaces);
    fprintf(f, "%s %s\n", name, newVal);
    hashRemove(hash, keyVal);
    }
else
    fprintf(f, "%s", orig);
}

void rewriteWithVisAndPriUpdates(char *tIn, char *pIn, char *vIn, char *tOut)
/* Write tIn to tOut applying modifications in pIn and pOut */
{
struct hash *pHash, *pTextHash;
struct slName *pList = hashPair(pIn, "track", "priority", &pHash, &pTextHash);
struct hash *vHash, *vTextHash;
struct slName *vList = hashPair(vIn, "track", "visibility", &vHash, &vTextHash);
struct lineFile *lf = lineFileOpen(tIn, TRUE);
FILE *f = mustOpen(tOut, "w");
struct dyString *dy = dyStringNew(0);
while (raSkipLeadingEmptyLines(lf, dy))
    {
    fprintf(f, "%s", dy->string);
    dyStringClear(dy);
    char *keyVal = NULL;
    char *name, *val;
    while (raNextTagVal(lf, &name, &val, dy))
        {
	if (sameString(glKeyField, name))
	    {
	    fprintf(f, "%s", dy->string);
	    keyVal = cloneString(val);
	    }
	else if (sameString("priority", name))
	    {
	    overrideOrWriteSelf(dy->string, name, val, "priority", keyVal, pHash, lf, f);
	    }
	else if (sameString("visibility", name))
	    {
	    overrideOrWriteSelf(dy->string, name, val, "visibility", keyVal, vHash, lf, f);
	    }
	else
	    {
	    fprintf(f, "%s", dy->string);
	    }
	dyStringClear(dy);
	}
    fprintf(f, "%s", dy->string);
    }
struct slName *p, *v;
boolean pWroteHead = FALSE;
for (p = pList; p != NULL; p = p->next)
    {
    char *key = p->name;
    char *pri = hashFindVal(pHash, key);
    if (pri != NULL)
        {
	if (!pWroteHead)
	    {
	    fprintf(f, "\n#Overrides from priority.ra\n\n");
	    pWroteHead = TRUE;
	    }
	char *text = hashFindVal(pTextHash, key);
	fprintf(f, "%s", text);
	char *vis = hashFindVal(vHash, key);
	if (vis != NULL)
	    {
	    char *vText = hashFindVal(vTextHash, key);
	    fprintf(f, "%s", vText);
	    }
	fprintf(f, "track %s override\n", key);
	fprintf(f, "priority %s\n", pri);
	if (vis != NULL)
	    fprintf(f, "visibility %s\n", vis);
	fprintf(f, "\n");
	}
    }
boolean vWroteHead = FALSE;
for (v = vList; v != NULL; v = v->next)
    {
    char *key = v->name;
    char *vis = hashFindVal(vHash, key);
    if (vis != NULL)
        {
	char *pri = hashFindVal(pHash, key);
	if (pri == NULL)  /* Already wrote this above if has both. */
	    {
	    char *text = hashFindVal(vTextHash, key);
	    if (!vWroteHead)
		{
		fprintf(f, "\n#Overrides from visibility.ra\n\n");
		vWroteHead = TRUE;
		}
	    fprintf(f, "%s", text);
	    fprintf(f, "track %s override\n", key);
	    fprintf(f, "visibility %s\n", vis);
	    fprintf(f, "\n");
	    }
	}
    }
dyStringFree(&dy);
carefulClose(&f);
lineFileClose(&lf);
}

static void rewriteInsideSubdir(char *outDir, char *inDir, char *subDir,
	char *trackFile, char *visFile, char *priFile)
/* Do some sort of rewrite on one subdirectory. */
{
char tIn[PATH_LEN], vIn[PATH_LEN], pIn[PATH_LEN];
safef(tIn, sizeof(tIn), "%s/%s", inDir, trackFile);
safef(vIn, sizeof(vIn), "%s/%s", inDir, visFile);
safef(pIn, sizeof(pIn), "%s/%s", inDir, priFile);
if (fileExists(tIn))
     {
     if (fileExists(pIn) || fileExists(vIn))
	 {
	 char tOut[PATH_LEN];
	 safef(tOut, sizeof(tOut), "%s/%s", outDir, trackFile);
	 makeDirsOnPath(outDir);
	 rewriteWithVisAndPriUpdates(tIn, pIn, vIn, tOut);
	 }
     }
}

void doRewrite(char *outDir, char *inDir, char *trackFile, char *visFile, char *priFile)
/* Do some sort of rewrite on entire system. */
{
struct fileInfo *org, *orgList = listDirX(inDir, "*", FALSE);
for (org = orgList; org != NULL; org = org->next)
    {
    if (org->isDir)
	{
	char inOrgDir[PATH_LEN], outOrgDir[PATH_LEN];
	safef(inOrgDir, sizeof(inOrgDir), "%s/%s", inDir, org->name);
	safef(outOrgDir, sizeof(outOrgDir), "%s/%s", outDir, org->name);
	rewriteInsideSubdir(outOrgDir, inOrgDir, org->name, trackFile, visFile, priFile);
	struct fileInfo *db, *dbList = listDirX(inOrgDir, "*", FALSE);
	for (db = dbList; db != NULL; db = db->next)
	    {
	    if (db->isDir)
	        {
		char inDbDir[PATH_LEN], outDbDir[PATH_LEN];
		safef(inDbDir, sizeof(inDbDir), "%s/%s", inOrgDir, db->name);
		safef(outDbDir, sizeof(outDbDir), "%s/%s", outOrgDir, db->name);
		rewriteInsideSubdir(outDbDir, inDbDir, db->name, trackFile, visFile, priFile);
		}
	    }
	}
    }
}

unsigned getReleaseBit(char *release)
/* make sure that the tag is a legal release */
{
if (sameString(release, "alpha"))
    return RELEASE_ALPHA;

if (sameString(release, "beta"))
    return RELEASE_BETA;

if (sameString(release, "public"))
    return RELEASE_PUBLIC;

errAbort("release must be alpha, beta, or public");

return 0;  /* make compiler happy */
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clRoot = simplifyPathToDir(optionVal("root", clRoot));
clCheck = optionExists("check");
clStrict = optionExists("strict");
release = optionVal("release", release);
releaseBit = getReleaseBit(release);
clNoBlank = optionExists("noBlank");
clRewrite = optionVal("rewrite", clRewrite);
clNoCompSub = optionExists("noCompSub");
if (clRewrite)
    {
    doRewrite(clRewrite, clRoot, "trackDb.ra", "visibility.ra", "priority.ra");
    }
else
    {
    if (argc != 2)
	usage();
    tdbQuery(argv[1]);
    }
return 0;
}
