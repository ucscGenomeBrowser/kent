/* tdbRewriteViewsToSubtracks - Convert views to subtracks with sub-sub-tracks.. */
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



static char *clRoot = "~/kent/src/hg/makeDb/trackDb";	/* Root dir of trackDb system. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tdbRewriteViewsToSubtracks - Convert views to subtracks with sub-sub-tracks.\n"
  "usage:\n"
  "   tdbRewriteViewsToSubtracks outDir\n"
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

#ifdef SOON
static void raTagWrite(struct raTag *tag, FILE *f)
/* Write tag to file */
{
fputs(tag->text, f);
}
#endif

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


void raTagWriteIndented(struct raTag *t, FILE *f, int indents)
/* Write out tag with extra indents.  Include (and indent) any preceeding comments. */
{
char *s = t->text;
for (;;)
    {
    char *e = skipLeadingSpaces(s);
    mustWrite(f, s, e-s);
    spaceOut(f, indents);
    s = e;
    e = strchr(s, '\n');
    if (e != NULL)
	{
        mustWrite(f, s, e-s+1);
	s = e+1;
	if (s[0] == 0)
	    break;
	}
    else
        {
	fprintf(f, "%s\n", s);
	}
    }
}

void raRecordWriteIndented(struct raRecord *r, FILE *f, int indent)
/* Write out tags in record to file, including preceding spaces. */
{
/* Write all tags. */
struct raTag *t;
for (t = r->tagList; t != NULL; t = t->next)
    raTagWriteIndented(t, f, indent);
if (r->endComments)
    {
    spaceOut(f, indent);
    fputs(r->endComments, f);
    }
}

struct raTag *findViewSubGroup(struct raRecord *r)
/* Find tag that is one of the subGroup tags with first word view.  May return NULL. */
{
int i;
for (i=1; ; ++i)
    {
    char tagName[16];
    safef(tagName, sizeof(tagName), "subGroup%d", i);
    struct raTag *tag = raRecordFindTag(r, tagName);
    if (tag == NULL)
        return NULL;
    if (startsWithWord("view", tag->val))
        return tag;
    }
}

static void writeRecordAsSubOfView(struct raRecord *r, FILE *f, char *viewTrackName)
/* Write out a record, but indent it more and substitute viewTrackName for existing
 * subTrack. */
{
struct raTag *t;
for (t = r->tagList; t != NULL; t = t->next)
    {
    if (sameString(t->name, "subTrack"))
        {
	char *s = firstTagInText(t->text);
	mustWrite(f, t->text, s - t->text);
	spaceOut(f, 4);
	fprintf(f, "subTrack %s", viewTrackName);
	/* Skip over subTrack and name in original text. */
	int i;
	for (i=0; i<2; ++i)
	    {
	    s = skipLeadingSpaces(s);
	    s = skipToSpaces(s);
	    }
	if (s != NULL)
	     fputs(s, f);
	else
	    fputc('\n', f);
	}
    else
        {
	raTagWriteIndented(t, f, 4);
	}
    }
}


boolean shouldBeParentOfView(struct raRecord *r)
/* Return TRUE if its a record that we'll insert views underneath */
{
return findViewSubGroup(r) != NULL;
}

void rewriteSettingsByViewComplex(struct raFile *file, struct raRecord *complexRecord, FILE *f,	
	struct lm *lm)
/* Rewrite track that has settings by view */
{
/* Get list of views from subGroup1 or subGroup2 tag. */
struct raTag *viewSubGroupTag = findViewSubGroup(complexRecord);
if (viewSubGroupTag == NULL)
    recordAbort(complexRecord, "Can't find view subGroup#");
char *line = lmCloneString(lm, viewSubGroupTag->val);
/*  line looks something like: 
 *       view Views FiltTransfrags=Filtered_Transfrags Transfrags=Raw_Transfrags */
char *viewWord = nextWord(&line);
assert(sameString(viewWord, "view"));
nextWord(&line);	// Just skip over name to label views with
struct slPair *viewList = NULL;
char *thisEqThat;
while ((thisEqThat = nextWord(&line)) != NULL)
    {
    char *eq = strchr(thisEqThat, '=');
    if (eq == NULL)
        recordAbort(complexRecord, "expecting this=that got %s in %s tag", 
		eq, viewSubGroupTag->name);
    *eq = 0;
    slPairAdd(&viewList, thisEqThat, lmCloneString(lm, eq+1));
    }
slReverse(&viewList);

/* Write self, the parent record.  This is just most but not all tags of the original record,
 * omitting settingsByView and visibilityViewDefaults which will be moved into the view. */
struct raTag *t;
for (t = complexRecord->tagList; t != NULL; t = t->next)
    {
    if (!sameString(t->name, "settingsByView") && !sameString(t->name, "visibilityViewDefaults"))
	fputs(t->text, f);
    }

/* Parse out visibilityViewDefaults. */
struct hash *visHash = NULL;
struct raTag *visTag = raRecordFindTag(complexRecord, "visibilityViewDefaults");
if (visTag != NULL)
    {
    char *dupe = lmCloneString(lm, visTag->val);
    visHash = hashThisEqThatLine(dupe, complexRecord->startLineIx, FALSE);
    }

/* Parse out settingsByView. */
struct raTag *settingsTag = raRecordFindTag(complexRecord, "settingsByView");
struct hash  *settingsHash = hashNew(4);
if (settingsTag != NULL)
    {
    char *dupe = lmCloneString(lm, settingsTag->val);
    char *line = dupe;
    char *viewName;
    while ((viewName = nextWord(&line)) != NULL)
	{
	char *settings = strchr(viewName, ':');
	if (settings == NULL)
	    recordAbort(complexRecord, "missing colon in settingsByView '%s'", viewName);
	struct slPair *el, *list = NULL;
	*settings++ = 0;
	if (!slPairFind(viewList, viewName))
	    recordAbort(complexRecord, "View '%s' in settingsByView is not defined in subGroup",
	    	viewName);
	char *words[32];
	int cnt,ix;
	cnt = chopByChar(settings,',',words,ArraySize(words));
	for (ix=0; ix<cnt; ix++)
	    {
	    char *name = words[ix];
	    char *val = strchr(name, '=');
	    if (val == NULL)
		recordAbort(complexRecord, "Missing equals in settingsByView on %s", name);
	    *val++ = 0;

	    AllocVar(el);
	    el->name = cloneString(name);
	    el->val = cloneString(val);
	    slAddHead(&list,el);
	    }
	slReverse(&list);
	hashAdd(settingsHash, viewName, list);
	}
    }

/* Go through each view and write it, and then the children who are in that view. */
struct slPair *view;
for (view = viewList; view != NULL; view = view->next)
    {
    char viewTrackName[256];
    safef(viewTrackName, sizeof(viewTrackName), "%sView%s", complexRecord->key, view->name);
    fprintf(f, "\n");	/* Blank line to open view. */
    fprintf(f, "    track %s\n", viewTrackName);
    char *shortLabel = lmCloneString(lm, view->val);
    subChar(shortLabel, '_', ' ');
    fprintf(f, "    shortLabel %s\n", shortLabel);
    fprintf(f, "    view %s\n", view->name);
    char *vis = NULL;
    if (visHash != NULL)
         vis = hashFindVal(visHash, view->name);
    if (vis != NULL)
	{
	int len = strlen(vis);
	boolean gotPlus = (lastChar(vis) == '+');
	if (gotPlus)
	    len -= 1;
	char visOnly[len+1];
	memcpy(visOnly, vis, len);
	visOnly[len] = 0;
	fprintf(f, "    visibility %s\n", visOnly);
	if (gotPlus)
	    fprintf(f, "    viewUi on\n");
	}
    fprintf(f, "    subTrack %s\n", complexRecord->key);
    struct slPair *settingList = hashFindVal(settingsHash, view->name);
    struct slPair *setting;
    for (setting = settingList; setting != NULL; setting = setting->next)
	fprintf(f, "    %s %s\n", setting->name, (char*)setting->val);

    /* Scan for children that are in this view. */
    struct raRecord *r;
    for (r = file->recordList; r != NULL; r = r->next)
	{
	struct raTag *subTrackTag = raRecordFindTag(r, "subTrack");
	if (subTrackTag != NULL)
	    {
	    if (startsWithWord(complexRecord->key, subTrackTag->val))
		{
		struct raTag *subGroupsTag = raRecordFindTag(r, "subGroups");
		if (subGroupsTag != NULL)
		    {
		    struct hash *hash = hashThisEqThatLine(subGroupsTag->val, 
			    r->startLineIx, FALSE);
		    char *viewName = hashFindVal(hash, "view");
		    if (viewName != NULL && sameString(viewName, view->name))
			{
			writeRecordAsSubOfView(r, f, viewTrackName);
			}
		    hashFree(&hash);
		    }
		}
	    }
	}
    }
}

void rewriteTrack(struct raLevel *level, struct raFile *file, 
	struct raRecord *r, FILE *f, struct lm *lm)
/* Write one track record.  */
{
struct raRecord *recordInParentFile = findRecordInParentFileLevel(level, r);
struct raTag *subTrack = raRecordFindTag(r, "subTrack");

if (shouldBeParentOfView(r))
    {
    if (recordInParentFile)
        recordAbort(r, "Can't handle settingsByViews with records in parent file levels");
    /* We are the parent. */
    // fprintf(f, "# Rewriting parent with subGroup view %s\n", r->key);
    rewriteSettingsByViewComplex(file, r, f, lm);
    }
else if (subTrack)
    {
    struct raRecord *rParent = findRecordInLevelOrLevelsUp(level, 
    	lmCloneFirstWord(lm, subTrack->val),
    	findRelease(r, level));
    if (rParent == NULL)
        recordAbort(r, "Couldn't find rParent track %s, subTrack %s.", r->key, subTrack->val);
    if (shouldBeParentOfView(rParent))
        {
	if (rParent->file != r->file)
	    recordAbort(r, "complex parent %s not in same file as subTrack %s", 
	    	rParent->key, r->key);
	// fprintf(f, "# Omitting child (%s) of parent (%s) with views\n", r->key, rParent->key);
	}
    else
        {
	raRecordWriteTags(r, f);
	}
    }
else
    {
    raRecordWriteTags(r, f);
    }
}

void rewriteFile(struct raLevel *level, struct raFile *file, char *outName, struct lm *lm)
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

void rewriteLevel(struct raLevel *level, char *outDir, struct lm *lm)
/* Rewrite files in level. */
{
struct raFile *file;
if (level->fileList != NULL)
    makeDirsOnPath(outDir);   
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
char rootName[PATH_LEN];
safef(rootName, sizeof(rootName), "%s/%s", inDir, trackFile);
struct raLevel *rootLevel = raLevelRead(rootName, rootLm);
rewriteLevel(rootLevel, outDir, rootLm);

/* Make subdirectory list. */
struct fileInfo *org, *orgList = listDirX(inDir, "*", FALSE);
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
	rewriteLevel(orgLevel, outOrgDir, orgLm);
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
		dbLevel->parent = orgLevel;
		rewriteLevel(dbLevel, outDbDir, dbLm);
		hashFree(&dbLevel->trackHash);
		lmCleanup(&dbLm);
		}
	    }
	hashFree(&orgLevel->trackHash);
	lmCleanup(&orgLm);
	}
    }
hashFree(&rootLevel->trackHash);
lmCleanup(&rootLm);
}

void tdbRewriteViewsToSubtracks(char *outDir)
/* tdbRewriteViewsToSubtracks - Convert views to subtracks with sub-sub-tracks.. */
{
doRewrite(outDir, clRoot, "trackDb.ra");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clRoot = simplifyPathToDir(optionVal("root", clRoot));
if (argc != 2)
    usage();
tdbRewriteViewsToSubtracks(argv[1]);
return 0;
}
