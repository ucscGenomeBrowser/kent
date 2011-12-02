/* tdbRewriteRemoveUnused - Remove stanzas that have no table that exists in any existing database.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "dystring.h"
#include "obscure.h"
#include "portable.h"
#include "errabort.h"
#include "ra.h"
#include "hdb.h"



static char *clRoot = "~/kent/src/hg/makeDb/trackDb";	/* Root dir of trackDb system. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tdbRewriteRemoveUnused - Remove stanzas that have no table that exists in any existing database.\n"
  "usage:\n"
  "   tdbRewriteRemoveUnused outDir\n"
  "options:\n"
  "   -root=/path/to/trackDb/root/dir\n"
  "Sets the root directory of the trackDb.ra directory hierarchy to be given path. By default\n"
  "this is ~/kent/src/hg/makeDb/trackDb.\n"
  );
}

static struct optionSpec options[] = {
   {"root", OPTION_STRING},
   {NULL, 0},
};

#define glKeyField "track"	 /* The field that has the record ID */
#define glParentField "subTrack" /* The field that points to the parent. */

struct raTag
/* A tag in a .ra file. */
    {
    struct raTag *next;
    char *name;		/* Name of tag. */
    char *val;		/* Value of tag. */
    char *text;		/* Text - including white space and comments before tag. */
    };

struct raRecord
/* A record in a .ra file. */
    {
    struct raRecord *next;	/* Next in list. */
    char *key;			/* First word in track line if any. */
    struct raTag *tagList;	/* List of tags that make us up. */
    int startLineIx, endLineIx; /* Start and end in file for error reporting. */
    struct raFile *file;	/* Pointer to file we are in. */
    char *endComments;		/* Some comments that may follow record. */
    boolean seenInDb;		/* True if associated table exists. */
    };

struct raFile
/* A file full of ra's. */
    {
    struct raFile *next;	  /* Next (in include list) */
    char *name;		  	  /* Name of file */
    struct raRecord *recordList;  /* List of all records in file */
    char *endSpace;		  /* Text after last record. */
    struct raLevel *level;	  /* Level this is in. */
    };

struct raLevel
/* A level of ra files.  List of files with includes starting from trackDb.ra at given
 * directory level. */
    {
    struct raLevel *parent;	/* Parent level */
    char *name;			/* Level name - directory */
    struct raFile *fileList;	/* List of files at this level. */
    struct hash *trackHash;	/* Hash of all track records in level. */
    };

void recordLocationReport(struct raRecord *rec, FILE *out)
/* Write out where record ends. */
{
fprintf(out, "in stanza from lines %d-%d of %s\n", 
	rec->startLineIx, rec->endLineIx, rec->file->name);
}

void recordWarn(struct raRecord *rec, char *format, ...)
/* Issue a warning message. */
{
va_list args;
va_start(args, format);
vaWarn(format, args);
va_end(args);
recordLocationReport(rec, stderr);
}

void recordAbort(struct raRecord *rec, char *format, ...)
/* Issue a warning message. */
{
va_list args;
va_start(args, format);
vaWarn(format, args);
va_end(args);
recordLocationReport(rec, stderr);
noWarnAbort();
}

struct raTag *raRecordFindTag(struct raRecord *r, char *name)
/* Find tag of given name.  Return NULL if not found. */
{
struct raTag *tag;
for (tag = r->tagList; tag != NULL; tag = tag->next)
    if (sameString(tag->name, name))
        break;
return tag;
}

char *firstTagInText(char *text)
/* Return the location of tag in text - skipping blank and comment lines and white-space */
{
char *s = text;
for (;;)
    {
    s = skipLeadingSpaces(s);
    if (s[0] == '#')
        {
	s = strchr(s, '\n');
	}
    else
        break;
    }
return s;
}

static struct raRecord *readRecordsFromFile(struct raFile *file, struct dyString *dy, struct lm *lm)
/* Read all the records in a file and return as a list.  The dy parameter returns the
 * last bits of the file (after the last record). */
{
char *fileName = file->name;
struct raRecord *r, *rList = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
while (raSkipLeadingEmptyLines(lf, dy))
    {
    /* Create a tag structure in local memory. */
    lmAllocVar(lm, r);
    r->startLineIx = lf->lineIx;
    char *name, *val;
    while (raNextTagVal(lf, &name, &val, dy))
        {
	struct raTag *tag;
	lmAllocVar(lm, tag);
	tag->name = lmCloneString(lm, name);
	tag->val = lmCloneString(lm, val);
	tag->text = lmCloneString(lm, dy->string);
	if (sameString(name, glKeyField))
	    r->key = lmCloneFirstWord(lm, tag->val);
	slAddHead(&r->tagList, tag);
	dyStringClear(dy);
	}
    if (dy->stringSize > 0)
        {
	r->endComments = lmCloneString(lm, dy->string);
	}
    slReverse(&r->tagList);
    r->endLineIx = lf->lineIx;
    r->file = file;
    slAddHead(&rList, r);
    }
lineFileClose(&lf);
slReverse(&rList);
return rList;
}

struct raFile *raFileRead(char *fileName, struct raLevel *level, struct lm *lm)
/* Read in file */
{
struct dyString *dy = dyStringNew(0);
struct raFile *raFile;
lmAllocVar(lm, raFile);
raFile->name = lmCloneString(lm, fileName);
raFile->recordList = readRecordsFromFile(raFile, dy, lm);
raFile->endSpace = lmCloneString(lm, dy->string);
raFile->level = level;
dyStringFree(&dy);
return raFile;
}

static void recurseThroughIncludes(char *fileName, struct lm *lm, 
	struct hash *circularHash,  struct raLevel *level, struct raFile **pFileList)
/* Recurse through include files. */
{
struct raFile *raFile = raFileRead(fileName, level, lm);
slAddHead(pFileList, raFile);
struct raRecord *r;
for (r = raFile->recordList; r != NULL; r = r->next)
    {
    struct raTag *tag = r->tagList;
    if (sameString(tag->name, "include"))
        {
	for (; tag != NULL; tag = tag->next)
	    {
	    if (!sameString(tag->name, "include"))
	       recordAbort(r, "Non-include tag %s in an include stanza", tag->name);
	    char *relPath = tag->val;
	    char dir[PATH_LEN];
	    splitPath(fileName, dir, NULL, NULL);
	    char includeName[PATH_LEN];
	    safef(includeName, sizeof(includeName), "%s%s", dir, relPath);
	    if (hashLookup(circularHash, includeName))
		recordAbort(r, "Including file %s in an infinite loop", includeName);
	    recurseThroughIncludes(includeName, lm, circularHash, level, pFileList);
	    }
	}
    }
}

struct hash *hashLevelTracks(struct raLevel *level)
/* Return hash of all track in level.  May include multiple copies of same track
 * with different releases. */
{
struct hash *hash = hashNew(0);
struct raFile *file;
for (file = level->fileList; file != NULL; file = file->next)
    {
    struct raRecord *r;
    for (r = file->recordList; r != NULL; r = r->next)
        {
	if (r->key != NULL)
	    hashAdd(hash, r->key, r);
	}
    }
return hash;
}

struct raLevel *raLevelRead(char *initialFile, struct lm *lm)
/* Read initialFile and all files that are included by it. */
{
/* Create structure for level. */
struct raLevel *level;
lmAllocVar(lm, level);
char dir[PATH_LEN];
splitPath(initialFile, dir, NULL, NULL);
level->name = lmCloneString(lm, dir);

/* Build up list of files by recursion. */
if (fileExists(initialFile))
    {
    struct hash *circularHash = hashNew(0);
    hashAdd(circularHash, initialFile, NULL);
    recurseThroughIncludes(initialFile, lm, circularHash, level, &level->fileList);
    hashFree(&circularHash);
    slReverse(&level->fileList);
    }

level->trackHash = hashLevelTracks(level);
return level;
}


struct raRecord *findClosestParent(struct raLevel *level, struct raRecord *record, char *parentKey)
/* Look up key in level hash, and return the one that is closest to self, but before self
 * in the same file. This disregards release.   It is in fact used to do the
 * inheritance of releases. */
{
struct raRecord *closestParent = NULL;
int closestDistance = BIGNUM;
struct hashEl *hel;
for (hel = hashLookup(level->trackHash, parentKey); hel != NULL; hel = hashLookupNext(hel))
    {
    struct raRecord *parent = hel->val;
    int distance = record->startLineIx - parent->startLineIx;
    if (distance < 0)
        distance = BIGNUM/4 - distance;
    if (record->file != parent->file)
        distance = BIGNUM/2; 
    if (distance < closestDistance)
        {
	closestDistance = distance;
	closestParent = parent;
	}
    }
return closestParent;
}

char *findRelease(struct raRecord *record, struct raLevel *level)
/* Find release tag in self or parent track at this level. */
{
while (record != NULL)
    {
    struct raTag *releaseTag = raRecordFindTag(record, "release");
    if (releaseTag != NULL)
        return releaseTag->val;
    struct raTag *parentTag = raRecordFindTag(record, glParentField);
    if (parentTag != NULL)
        record = findClosestParent(level, record, parentTag->val);
    else
        record = NULL;
    }
return NULL;
}

struct raRecord *findRecordAtLevel(struct raLevel *level, char *key, char *release)
/* Find record of given key and release in level. */
{
/* Look up key in hash */
struct hashEl *firstEl = hashLookup(level->trackHash, key);

/* Loop through and return any ones that match on both key (implicit in hash find) and
 * in release. */
struct hashEl *hel;
for (hel = firstEl; hel != NULL; hel = hashLookupNext(hel))
    {
    struct raRecord *r = hel->val;
    struct raTag *releaseTag = raRecordFindTag(r, "release");
    char *rRelease = (releaseTag == NULL ? NULL : releaseTag->val);
    if (sameOk(release, rRelease))
        return r;
    }

/* If given record has no defined release, return first match regardless of release. */
if (release == NULL && firstEl != NULL)
    return firstEl->val;

/* Match to records that have no release defined. */
for (hel = firstEl; hel != NULL; hel = hashLookupNext(hel))
    {
    struct raRecord *r = hel->val;
    struct raTag *releaseTag = raRecordFindTag(r, "release");
    if (releaseTag == NULL)
        return r;
    }

return NULL;
}

struct raRecord *findRecordInParentFileLevel(struct raLevel *level, struct raRecord *record)
/* Find record matching release in parent file level.  */
{
char *release = findRelease(record, level);
struct raRecord *parentRecord = NULL;
struct raLevel *parent;
for (parent = level->parent; parent != NULL; parent = parent->parent)
    {
    if ((parentRecord = findRecordAtLevel(parent, record->key, release)) != NULL)
        break;
    }
return parentRecord;
}

struct raRecord *findRecordInLevelOrLevelsUp(struct raLevel *level, char *key, char *release)
/* Find record matching key and compatible with release in level or ancestral levels.  */
{
struct raLevel *generation;
for (generation = level; generation != NULL; generation = generation->parent)
    {
    struct raRecord *record = findRecordAtLevel(generation, key, release);
    if (record != NULL)
        return record;
    }
return NULL;
}

void raRecordWriteTags(struct raRecord *r, FILE *f)
/* Write out tags in record to file, including preceding spaces. */
{
/* Write all tags. */
struct raTag *t;
for (t = r->tagList; t != NULL; t = t->next)
    fputs(t->text, f);
}

boolean tableExistsInAnyDb(struct fileInfo *dbList, struct raRecord *r)
/* Return TRUE if record exists in any database on list. */
{
struct fileInfo *db;
for (db = dbList; db != NULL; db = db->next)
    {
    if (hTableOrSplitExists(db->name, r->key))
        return TRUE;
    }
return FALSE;
}

void rewriteTrack(struct raLevel *level, struct raFile *file, 
	struct raRecord *r, FILE *f, struct lm *lm)
/* Write one track record.  */
{
if (r->seenInDb)
    {
    raRecordWriteTags(r, f);
    }
}

void rewriteFile(struct raLevel *level, struct raFile *file, char *outName, 
	struct lm *lm)
/* Rewrite file to outName, consulting symbols in parent. */
{
FILE *f = mustOpen(outName, "w");
struct raRecord *r;
for (r = file->recordList; r != NULL; r = r->next)
    {
    /* Rewrite leading track tag in stanzas.  Other tags pass through. */
    struct raTag *t = r->tagList;
    if (sameString(t->name, "track"))
        {
	rewriteTrack(level, file, r, f, lm);
	}
    else
	{
	for (; t != NULL; t = t->next)
	    fputs(t->text, f);
	}
    if (r->endComments != NULL)
        fputs(r->endComments, f);
    }
fputs(file->endSpace, f);
carefulClose(&f);
}

void markSelfAndParentsAtAllLevels(struct raRecord *r, struct raLevel *startLevel)
/* Set seenInDb field. See if track has a parent (supertrack or subtrack system).  
 * in which case will call self to do same on parent. */
{
r->seenInDb = TRUE;

/* Get sub tab. */
struct raTag *subTag = raRecordFindTag(r, "subTrack");

/* Get super tag if it points to a parent. */
struct raTag *superTag = raRecordFindTag(r, "superTrack");
if (superTag && sameWord(superTag->val, "on"))
    superTag = NULL;

/* If have one or the other then parse out name and call self on parent (in all releases) */
if (superTag != NULL || subTag != NULL)
     {
     /* Do some error checking and figure out parent's name. */
     if (superTag != NULL && subTag != NULL)
         recordAbort(r, "%s has parents in both subTrack and superTrack systems, can't cope.",
	 	r->key);
     struct raTag *parentTag = (superTag != NULL ? superTag : subTag);
     char *parentName = cloneFirstWord(parentTag->val);

     /* We may have parents at any level above us as well as our own. */
     struct raLevel *level;
     for (level = startLevel; level != NULL; level = level->parent)
         {
	 struct hashEl *hel;
	 /* Might have multiple parents in one level because of releases.  A better program
	  * might distinguish between them, but this one just marks them both. */
	 for (hel = hashLookup(level->trackHash, parentName); hel != NULL; hel=hashLookupNext(hel))
	     {
	     struct raRecord *parent = hel->val;
	     if (!parent->seenInDb)
		 markSelfAndParentsAtAllLevels(parent, startLevel);
	     }
	 }
     }
}

void rewriteLevel(struct raLevel *level, char *outDir, struct fileInfo *dbList, struct lm *lm)
/* Rewrite files in level. */
{
struct raFile *file;
if (level->fileList != NULL)
    makeDirsOnPath(outDir);   

/* Make pass that marks used records. */
for (file = level->fileList; file != NULL; file = file->next)
    {
    struct raRecord *r;
    for (r = file->recordList; r != NULL; r = r->next)
        {
	if (r->key)
	    {
	    if (tableExistsInAnyDb(dbList, r))
		{
		markSelfAndParentsAtAllLevels(r, level);
		}
	    }
	}
    }

/* Make second pass that writes out used records. */
for (file = level->fileList; file != NULL; file = file->next)
    {
    char outName[FILENAME_LEN], outExtension[FILEEXT_LEN];
    splitPath(file->name, NULL, outName, outExtension);
    char outPath[PATH_LEN];
    safef(outPath, sizeof(outPath), "%s/%s%s", outDir, outName, outExtension);
    rewriteFile(level, file, outPath, lm);
    }
}

void doRewrite(char *outDir, char *inDir, char *trackFile)
/* Do some sort of rewrite on entire system. */
{
/* Make list and hash of root dir */
struct lm *rootLm = lmInit(0);
struct fileInfo *allDbList = NULL;
char rootName[PATH_LEN];
safef(rootName, sizeof(rootName), "%s/%s", inDir, trackFile);
struct raLevel *rootLevel = raLevelRead(rootName, rootLm);

/* Make subdirectory list. */
struct fileInfo *org, *orgList = listDirX(inDir, "*", FALSE);
uglyf("orgList has %d elements\n", slCount(orgList));
for (org = orgList; org != NULL; org = org->next)
    {
    if (org->isDir)
	{
	struct lm *orgLm = lmInit(0);
	char inOrgDir[PATH_LEN], outOrgDir[PATH_LEN];
	safef(inOrgDir, sizeof(inOrgDir), "%s/%s", inDir, org->name);
	safef(outOrgDir, sizeof(outOrgDir), "%s/%s", outDir, org->name);
	char inOrgFile[PATH_LEN];
	safef(inOrgFile, sizeof(inOrgFile), "%s/%s", inOrgDir, trackFile);
	struct raLevel *orgLevel = raLevelRead(inOrgFile, orgLm);
	orgLevel->parent = rootLevel;
	struct fileInfo *db, *dbList = listDirX(inOrgDir, "*", FALSE);
	for (db = dbList; db != NULL; db = db->next)
	    {
	    if (db->isDir)
	        {
		struct lm *dbLm = lmInit(0);
		char inDbDir[PATH_LEN], outDbDir[PATH_LEN];
		safef(inDbDir, sizeof(inDbDir), "%s/%s", inOrgDir, db->name);
		safef(outDbDir, sizeof(outDbDir), "%s/%s", outOrgDir, db->name);
		char inDbFile[PATH_LEN];
		safef(inDbFile, sizeof(inDbFile), "%s/%s", inDbDir, trackFile);
		struct raLevel *dbLevel = raLevelRead(inDbFile, dbLm);
		/* Reduce db to a list of one temporarily and call rewrite level. */
		struct fileInfo *next = db->next;
		db->next = NULL;
		dbLevel->parent = orgLevel;
		rewriteLevel(dbLevel, outDbDir, db, dbLm);
		db->next = next;
		hashFree(&dbLevel->trackHash);
		lmCleanup(&dbLm);
		}
	    }
	rewriteLevel(orgLevel, outOrgDir, dbList, orgLm);
	allDbList = slCat(dbList, allDbList);
	hashFree(&orgLevel->trackHash);
	lmCleanup(&orgLm);
	}
    }
rewriteLevel(rootLevel, outDir, allDbList, rootLm);
hashFree(&rootLevel->trackHash);
lmCleanup(&rootLm);
}


void tdbRewriteRemoveUnused(char *outDir)
/* tdbRewriteRemoveUnused - Remove stanzas that have no table that exists in any existing database.. */
{
doRewrite(outDir, clRoot, "trackDb.ra");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
clRoot = simplifyPathToDir(optionVal("root", clRoot));
tdbRewriteRemoveUnused(argv[1]);
return 0;
}
