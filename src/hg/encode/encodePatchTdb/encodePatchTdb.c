/* encodePatchTdb - Lay a trackDb.ra file from the pipeline gently on top of the trackDb system. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "options.h"
#include "obscure.h"
#include "errabort.h"
#include "dystring.h"
#include "portable.h"
#include "ra.h"

static char const rcsid[] = "$Id: encodePatchTdb.c,v 1.1.2.3 2010/01/04 16:28:33 kent Exp $";

char *clMode = "add";
char *clTest = NULL;
static char *clRoot = "~/kent/src/hg/makeDb/trackDb";	/* Root dir of trackDb system. */


void usage()
/* Explain usage and exit. */
{
errAbort(
  "encodePatchTdb - Lay a trackDb.ra file from the pipeline gently on top of the trackDb system\n"
  "usage:\n"
  "   encodePatchTdb patchFile.ra fileToChange.ra\n"
  "Example:\n"
  "   encodePatchTdb 849/out/trackDb.ra ~/kent/src/makeDb/trackDb/human/hg18/trackDb.wgEncode.ra\n"
  "options:\n"
  "   -mode=mode (default %s).  Operate in one of the following modes\n"
  "         update - if record is new add it at end. If record is old add any new fields at end,\n"
  "                  and replace any old field values with new ones\n"
  "         replace - replace existing records rather than doing field by field update.\n"
  "         add - add new records at end of parent's subtrack list. Complain if record isn't new\n"
  "               warn if it's a new track rather than just new subtracks\n"
  "         addTrack - add new track plus subtracks.  Complains if not new\n"
  "   -test=patchFile - rather than doing patches in place, write patched output to this file\n"
  "   -root=/path/to/trackDb/root/dir - Sets the root directory of the trackDb.ra directory\n"
  "         hierarchy to be given path. By default this is ~/kent/src/hg/makeDb/trackDb.\n"
  "   -org=organism - try to put this at the organism level of the hierarchy instead of bottom\n"
  , clMode
  );
}

static struct optionSpec options[] = {
   {"mode", OPTION_STRING},
   {"test", OPTION_STRING},
   {"root", OPTION_STRING},
   {NULL, 0},
};

struct loadInfo
/* Information from a stanza of a load.ra file. */
    {
    struct loadInfo *next;
    char *name;		/* from tablename. */
    char *db;		/* from assembly. */
    char *view;		/* form view. */
    char *downloadOnly;	/* downloadOnly 0 or 1 */
    };

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
    struct raRecord *parent;	/* Parent if any. */
    struct raRecord *children;	/* Children - youngest to oldest. */
    struct raRecord *olderSibling;	/* Parent to older sibling if any. */
    char *key;			/* First word in track line if any. */
    struct raTag *tagList;	/* List of tags that make us up. */
    int startLineIx, endLineIx; /* Start and end in file for error reporting. */
    struct raFile *file;	/* Pointer to file we are in. */
    char *endComments;		/* Some comments that may follow record. */
    struct raRecord *subtracks;	/* Subtracks of this track. */
    };

struct raFile
/* A file full of ra's. */
    {
    struct raFile *next;	  /* Next (in include list) */
    char *name;		  	  /* Name of file */
    struct raRecord *recordList;  /* List of all records in file */
    char *endSpace;		  /* Text after last record. */
    struct hash *trackHash;	  /* Hash of records that have keys */
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

char *raRecordFindTagVal(struct raRecord *r, char *name)
/* Return value of tag of given name, or NULL if not found. */
{
struct raTag *tag = raRecordFindTag(r, name);
return (tag == NULL ? NULL : tag->val);
}

struct raTag *raRecordMustFindTag(struct raRecord *r, char *name)
/* Find given tag, abort with message if not found. */
{
struct raTag *tag = raRecordFindTag(r, name);
if (tag == NULL)
    recordAbort(r, "missing required tag %s", name);
return tag;
}

char *raRecordMustFindTagVal(struct raRecord *r, char *name)
/* Find value of given tag.  Abort if it doesn't exist. */
{
struct raTag *tag = raRecordMustFindTag(r, name);
return tag->val;
}

struct loadInfo *loadInfoReadRa(char *fileName)
/* Read ra file and turn it into a list of loadInfo. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct loadInfo *list = NULL, *el;
while (raSkipLeadingEmptyLines(lf, NULL))
    {
    char *tag, *val;
    AllocVar(el);
    while (raNextTagVal(lf, &tag, &val, NULL))
        {
	if (sameString(tag, "tablename"))
	    el->name = cloneString(val);
	else if (sameString(tag, "assembly"))
	    el->db = cloneString(val);
	else if (sameString(tag, "view"))
	    el->view = cloneString(val);
	else if (sameString(tag, "downloadOnly"))
	    el->downloadOnly = cloneString(val);
	}
    if (el->name == NULL)
        errAbort("missing tablename line %d of %s", lf->lineIx, lf->fileName);
    if (el->db == NULL)
        errAbort("missing assembly line %d of %s", lf->lineIx, lf->fileName);
    if (el->downloadOnly == NULL)
        errAbort("missing downloadOnly line %d of %s", lf->lineIx,lf->fileName);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

static struct raRecord *readRecordsFromFile(struct raFile *file, struct dyString *dy)
/* Read all the records in a file and return as a list.  The dy parameter returns the
 * last bits of the file (after the last record). */
{
char *fileName = file->name;
struct raRecord *r, *rList = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
while (raSkipLeadingEmptyLines(lf, dy))
    {
    /* Create a tag structure in local memory. */
    AllocVar(r);
    r->startLineIx = lf->lineIx;
    char *name, *val;
    while (raNextTagVal(lf, &name, &val, dy))
        {
	struct raTag *tag;
	AllocVar(tag);
	tag->name = cloneString( name);
	tag->val = cloneString( val);
	tag->text = cloneString( dy->string);
	if (sameString(name, "track"))
	    r->key = cloneFirstWord(tag->val);
	slAddHead(&r->tagList, tag);
	dyStringClear(dy);
	}
    if (dy->stringSize > 0)
        {
	r->endComments = cloneString( dy->string);
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

struct raFile *raFileRead(char *fileName)
/* Read in file */
{
struct dyString *dy = dyStringNew(0);
struct raFile *raFile;
AllocVar(raFile);
raFile->name = cloneString( fileName);
raFile->recordList = readRecordsFromFile(raFile, dy);
raFile->endSpace = cloneString( dy->string);

/* Hash up tracks. */
struct hash *hash = raFile->trackHash = newHash(0);
struct raRecord *r;
for (r = raFile->recordList; r != NULL; r = r->next)
    {
    if (r->key != NULL)
	hashAdd(hash, r->key, r);
    }

/* Clean up and go home. */
dyStringFree(&dy);
return raFile;
}

char *findDb(struct loadInfo *loadList, char *loadRa)
/* Find the db common to the list.  Abort if mixing dbs. */
{
struct loadInfo *loadInfo;
char *db = loadList->db;
for (loadInfo = loadList->next; loadInfo != NULL; loadInfo = loadInfo->next)
    {
    if (!sameString(db, loadInfo->db))
        errAbort("Multiple databases in %s: %s and %s, can't handle this.", 
		loadRa, db, loadInfo->db);
    }
return db;
}

boolean compositeFirst(struct raRecord *raList)
/* Return first record if it's composite.  Insure there are no other composites.
 * return NULL if first record is not composite. */
{
struct raRecord *r;
struct raTag *compositeTag = raRecordFindTag(raList, "compositeTrack");
if (compositeTag != NULL)
    if (!sameString(compositeTag->val, "on"))
        recordAbort(raList, "Expecting 'on' for compositeTrack value, got '%s'", compositeTag->val);
for (r = raList->next; r != NULL; r = r->next)
    {
    struct raTag *tag = raRecordFindTag(r, "compositeTrack");
    if (tag != NULL)
        recordAbort(r, "The compositeTrack tag is only allowed on the first stanza\n");
    }
return compositeTag != NULL;
}

static void checkSubsAreForParent(char *parentName, struct raRecord *subList)
/* Check that all subtracks in list have parent as subtrack. */
{
struct raRecord *sub;
for (sub = subList; sub != NULL; sub = sub->next)
    {
    char *subParent = cloneFirstWord(raRecordMustFindTagVal(sub, "subTrack"));
    if (!sameString(parentName, subParent))
        recordAbort(sub, "%s has subTrack %s, expecting subtrack %s", 
		sub->key, subParent, parentName);
    }
}

boolean hasViewSubtracks(struct raRecord *parent)
/*  Return TRUE if parent has view subtracks. */
{
struct raRecord *sub;
for (sub = parent->children; sub != NULL; sub = sub->next)
    if (raRecordFindTag(sub, "view"))
        return TRUE;
return FALSE;
}

struct raRecord *rFindView(struct raRecord *r, char *view)
/* Find view of given name at r or under. */
{
char *thisView = raRecordFindTagVal(r, "view");
if (thisView != NULL && sameString(thisView, view))
    return r;
struct raRecord *sub;
for (sub = r->subtracks; sub != NULL; sub = sub->next)
    {
    struct raRecord *viewRecord = rFindView(sub, view);
    if (viewRecord != NULL)
        return viewRecord;
    }
return NULL;
}

char *getOneOutOfThisEqualsThat(char *thisEqThat, char *name)
/* Get value on right side of name= in a name=value list. */
{
/* Not an efficient implementation, should be ok though. */
struct hash *hash = hashThisEqThatLine(thisEqThat, 0, FALSE);
char *val = cloneString(hashFindVal(hash, name));
freeHashAndVals(&hash);
return val;
}

char *findViewOfSub(struct raRecord *sub)
/* Find view that subtrack belongs to - parsing it out of subTrack. */
{
char *subGroups = raRecordFindTagVal(sub, "subGroups");
if (subGroups == NULL)
    return NULL;
char *viewName = getOneOutOfThisEqualsThat(subGroups, "view");
return viewName;
}

struct raRecord *findRecordCompatibleWithRelease(struct raFile *file, char *release, char *key)
/* Look in file for record with given key that is compatible with release.  Compatible means
 * that either the releases are the same, or one or the other is NULL. */
{
struct hashEl *hel;
for (hel = hashLookup(file->trackHash, key); hel != NULL; hel = hashLookupNext(hel))
    {
    struct raRecord *r = hel->val;
    struct raTag *tag = raRecordFindTag(r, "release");
    if (tag == NULL || release == NULL || sameString(tag->val, release))
        return r;
    }
return NULL;
}

static void linkUpParents(struct raFile *file)
/* Link up records according to parent/child relationships. */
{
struct raRecord *list = file->recordList;

/* Zero out children, parent, and older sibling fields, since going to recalculate
 * them and need lists to start out empty. */
struct raRecord *rec;
for (rec = list; rec != NULL; rec = rec->next)
    rec->parent = rec->olderSibling = rec->children = NULL;

/* Scan through linking up parents. */
for (rec = list; rec != NULL; rec = rec->next)
    {
    char *subTrack = raRecordFindTagVal(rec, "subTrack");
    if (subTrack != NULL)
	{
	char *release = raRecordFindTagVal(rec, "release");
	char *parentName = cloneFirstWord(subTrack);
	struct raRecord *parent = findRecordCompatibleWithRelease(file, release, parentName);
	if (parent == NULL)
	    recordAbort(rec, "Can't find parent record %s release %s", parentName, release);
	rec->parent = parent;
	rec->olderSibling = parent->children;
	parent->children = rec;
	}
    }
}

char *nonNullRelease(char *release1, char *release2)
/* Return whichever of the two releases is non-null. */
{
if (release1 != NULL)
    return release1;
return release2;
}


struct hash *hashOfHashSubGroupNs(struct raRecord *r)
/* Return a hash keyed by subGroup names (view, cellType, etc).  The hash value is
 * itself a hash, one that contains the name=val pairs on the subGroupN line. */
{
struct hash *hashOfHashes = hashNew(0);
int groupIx;
for (groupIx=1; ; groupIx++)
    {
    char tagName[16];
    safef(tagName,  sizeof(tagName), "subGroup%d", groupIx);
    struct raTag *tag = raRecordFindTag(r, tagName);
    if (tag == NULL)
        break;
    char *dupe = cloneString(tag->val);
    char *line = dupe;
    char *groupName = nextWord(&line);
    char *groupLabel = nextWord(&line);
    if (groupLabel == NULL)
        recordAbort(r, "malformed %s %s", tag->name, tag->val);
    struct hash *hash = hashThisEqThatLine(line, r->startLineIx, FALSE);
    if (hash->elCount < 1)
        recordAbort(r, "%s with no this=that", tag->name);
    hashAdd(hashOfHashes, groupName, hash);
    freez(&dupe);
    }
return hashOfHashes;
}

void validateParentSub(struct raRecord *parent, struct raRecord *sub)
/* Make sure that it is kosher that sub belongs to parent.  Mostly check
 * that subGroup1, subGroup2, etc in parent work with subGroups of sub. */
{
char *subGroups = raRecordMustFindTagVal(sub, "subGroups");
struct hash *myGroups = hashThisEqThatLine(subGroups, sub->startLineIx, FALSE);
struct hash *parentGroups = hashOfHashSubGroupNs(parent);
struct hashEl *myGroupList = hashElListHash(myGroups);
struct hashEl *myGroupEl;
for (myGroupEl = myGroupList; myGroupEl != NULL; myGroupEl = myGroupEl->next)
    {
    char *myName = myGroupEl->name;
    char *myVal = myGroupEl->val;
    struct hash *parentGroup = hashFindVal(parentGroups, myName);
    if (parentGroup == NULL)
        recordAbort(sub, "Parent %s doesn't have a subGroup %s", parent->key, myName);
    char *groupName = hashFindVal(parentGroup, myVal);
    if (groupName == NULL)
        recordAbort(sub, "Parent %s doesn't have a %s %s", parent->key, myName, myVal);
    verbose(2, "%s %s %s found in %s\n", sub->key, myName, myVal, parent->key);
    }
}

void validateParentViewSub(struct raRecord *parent, struct raRecord *view, struct raRecord *sub)
/* Make sure that it is kosher that sub belongs to parent and view.  */
{
validateParentSub(parent, sub);
}


void patchIntoEndOfView(struct raRecord *sub, struct raRecord *view)
/* Assuming view is in a file,  chase down later records in file until come to
 * first one that does not have view as a parent.  Insert sub right before that. */
{
struct raRecord *viewChild = NULL;
struct raRecord *r;
for (r = view->next; r != NULL; r = r->next)
    {
    if (r->parent == view)
        viewChild = r;
    else
        break;
    }
struct raRecord *recordBefore = (viewChild != NULL ? viewChild : view);
sub->next = recordBefore->next;
recordBefore->next = sub;
}

void patchInSubtrack(struct raRecord *parent, struct raRecord *sub)
/* Patch sub into the correct view of parent */
{
if (hasViewSubtracks(parent))
    {
    char *viewOfSub = findViewOfSub(sub);
    if (viewOfSub == NULL)
        recordAbort(sub, "Can only handle subtracks with view in subGroups");
    char viewTrackName[PATH_LEN];
    safef(viewTrackName, sizeof(viewTrackName), "%sView%s", parent->key, viewOfSub);
    char *parentRelease = raRecordFindTagVal(parent, "release");
    char *subRelease = raRecordFindTagVal(sub, "release");
    char *release = nonNullRelease(parentRelease, subRelease);
    struct raRecord *view = findRecordCompatibleWithRelease(parent->file, release, viewTrackName);
    struct raRecord *oldSub = findRecordCompatibleWithRelease(parent->file, release, sub->key);
    if (oldSub)
	{
        if (sameString(clMode, "update")  || sameString(clMode, "replace"))
	    {
	    uglyAbort("Unfortunately really don't know how to update or replace");
	    }
	else
	    recordAbort(sub, "record %s already exists - use mode update or replace",
	    	sub->key);
	}
    validateParentViewSub(parent, view, sub);
    patchIntoEndOfView(sub, view);
    }
else
    {
    recordAbort(parent, "Can only handle parents with views for now.");
    }
}

void patchInSubtracks(struct raRecord *tdbParent, struct raRecord *patchList)
/* Move records in patchList to correct position in tdbParent. */
{
struct raRecord *patch, *nextPatch;
for (patch = patchList; patch != NULL; patch = nextPatch)
    {
    nextPatch = patch->next;	/* Since patchInSubtrack will alter this. */
    patchInSubtrack(tdbParent, patch);
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

struct raRecord *makeParentPlusViewSubsFromComplexRecord(struct raRecord *complexRecord)
/* Convert one complex parent record from the settings-by-view era to parent record plus views. */
{
struct lm *lm = lmInit(0);
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


/* Get pointers to settingsByView and visibilityViewDefaults tags, and take them off of list. */
struct raTag *settingsByViewTag = raRecordFindTag(complexRecord, "settingsByView");
struct raTag *visibilityViewDefaultsTag = raRecordFindTag(complexRecord, "visibilityViewDefaults");
struct raTag *tagList = NULL, *t, *next;
for (t = complexRecord->tagList; t != NULL; t = next)
    {
    next = t->next;
    if (t != settingsByViewTag && t != visibilityViewDefaultsTag)
        slAddHead(&tagList, t);
    }
slReverse(&tagList);
complexRecord->tagList = tagList;

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

#ifdef SOON
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
#endif /* SOON */
return NULL;  // uglyf
}


void patchInTrack(struct raFile *tdbFile, struct raRecord *patchList)
/* Move records in patch to appropriate place in file.   Have to make up view subtracks. */
{
struct raFile *patchFile = patchList->file;
linkUpParents(patchFile);
struct raRecord *r;
for (r=patchFile->recordList; r != NULL; r = r->next)
    {
    if (r->parent)
        validateParentSub(r->parent, r);
    char *release = raRecordFindTagVal(r, "release");
    struct raRecord *oldR = findRecordCompatibleWithRelease(tdbFile, release, r->key);
    if (oldR)
        {
	if (!sameString(clMode, "update") && !sameString(clMode, "replace"))
	    {
	    errAbort("track %s already exists in %s.  Use mode=update or mode=replace",
	    	r->key, tdbFile->name);
	    }
	uglyAbort("mode update and replace not yet implemented");
	}
    }
uglyf("At least the subGroups in %s check\n", patchFile->name);
uglyAbort("patchInTrack not implemented");
}

void encodePatchTdb(char *patchFileName, char *tdbFileName)
/* encodePatchTdb - Lay a trackDb.ra file from the pipeline gently on top of the trackDb system. */
{
struct raFile *patchFile = raFileRead(patchFileName);
struct raRecord *patchList = patchFile->recordList;
int trackCount = slCount(patchList);
if (trackCount < 1)
    errAbort("No tracks in %s", patchFileName);

boolean hasTrack = compositeFirst(patchList);
if (hasTrack && sameString(clMode, "add"))
    errAbort("%s has a compositeTrack as well as subtracks.  Use mode=addTrack to permit this.",
        patchFileName);

/* Find parent track name. */
char *parentName;
if (hasTrack)
    {
    parentName = patchList->key;
    checkSubsAreForParent(parentName, patchList->next);
    }
else
   {
   parentName = cloneFirstWord(raRecordMustFindTagVal(patchList, "subTrack"));
   checkSubsAreForParent(parentName, patchList);
   }

/* Load file to patch. */
struct raFile *tdbFile = raFileRead(tdbFileName);
int oldTdbCount = slCount(tdbFile->recordList);
if (oldTdbCount < 100)
    warn("%s only has %d records, I hope you meant to hit a new file\n", tdbFileName, 
    	oldTdbCount);
linkUpParents(tdbFile);

struct raRecord *tdbParent = findRecordCompatibleWithRelease(tdbFile, "alpha", parentName);
if (hasTrack)
    {
    if (tdbParent)
        {
	if (!sameString(clMode, "update") && !sameString(clMode, "replace"))
	    {
	    errAbort("track %s already exists in %s.  Use mode=update or mode=replace",
	    	parentName, tdbFileName);
	    }
	uglyAbort("mode update and replace not yet implemented");
	}
    else
        {
	patchInTrack(tdbFile, patchList);
	}
    }
else
    {
    if (!tdbParent)
	errAbort("parent track %s doesn't exist in %s", parentName, tdbFileName);
    patchInSubtracks(tdbParent, patchList);
    }

char *outName = tdbFileName;
if (clTest != NULL)
    outName = clTest;


FILE *f = mustOpen(outName, "w");
struct raRecord *r;
for (r = tdbFile->recordList; r != NULL; r = r->next)
    {
    struct raTag *tag;
    for (tag = r->tagList; tag != NULL; tag = tag->next)
        fputs(tag->text, f);
    if (r->endComments != NULL)
	fputs(r->endComments, f);
    }
fputs(tdbFile->endSpace, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clRoot = simplifyPathToDir(optionVal("root", clRoot));
clMode = optionVal("mode", clMode);
clTest = optionVal("test", clTest);
encodePatchTdb(argv[1], argv[2]);
return 0;
}
