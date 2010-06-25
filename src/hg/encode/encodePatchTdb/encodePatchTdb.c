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

static char const rcsid[] = "$Id: encodePatchTdb.c,v 1.8 2010/01/05 20:25:37 kent Exp $";

char *clMode = "add";
char *clTest = NULL;
boolean clNoComment = FALSE;

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
  "         replace - replace existing records rather than doing field by field update.\n"
  "                   Leaves existing record commented out.\n"
  "         add - add new records at end of parent's subtrack list. Complain if record isn't new\n"
  "               warn if it's a new track rather than just new subtracks\n"
  "   -noComment - If set will not leave old record commented out\n"
  "   -test=patchFile - rather than doing patches in place, write patched output to this file\n"
  , clMode
  );
}

static struct optionSpec options[] = {
   {"mode", OPTION_STRING},
   {"test", OPTION_STRING},
   {"noComment", OPTION_BOOLEAN},
   {"root", OPTION_STRING},
   {NULL, 0},
};

boolean glReplace;	// If TRUE then do a replacement operation.

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
    boolean isRemoved;		/* If set, suppresses output. */
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
    char *parent = raRecordFindTagVal(sub, "parent");
    if (parent == NULL)
        parent = raRecordMustFindTagVal(sub, "subTrack");
    char *subParent = cloneFirstWord(parent);
    if (!startsWith(parentName, subParent))
        recordAbort(sub, "%s has parent %s, expecting %s",
		sub->key, subParent, parentName);
    }
}

static char *findViewTrackName(struct raRecord *parent, char *view)
/* Returns the name of the viewInTheMiddle track for a given view tag */
{
struct raRecord *mid;
for (mid = parent->children; mid != NULL; mid = mid->olderSibling)
    {
    char *viewName = raRecordFindTagVal(mid, "view");
    if (viewName != NULL && sameString(viewName,view))
        return mid->key;
    }
return NULL;
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
    char *subTrack = raRecordFindTagVal(rec, "parent");
    if (subTrack == NULL)
        subTrack = raRecordFindTagVal(rec, "subTrack");
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
sub->parent = view;
sub->next = recordBefore->next;
recordBefore->next = sub;
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

void substituteParentText(struct raRecord *parent, struct raRecord *view,
	struct raRecord *sub)
/* Convert subtrack parent with subtrack view. */
{
struct raTag *t = raRecordFindTag(sub, "parent");
if (t == NULL)
    t = raRecordMustFindTag(sub, "subTrack");
struct dyString *dy = dyStringNew(0);
char *s = firstTagInText(t->text);
dyStringAppendN(dy, t->text, s - t->text);
dyStringPrintf(dy, "parent %s", view->key);
/* Skip over subTrack and name in original text. */
int i;
for (i=0; i<2; ++i)
    {
    s = skipLeadingSpaces(s);
    s = skipToSpaces(s);
    }
if (s != NULL)
     dyStringAppend(dy, s);
else
    dyStringAppendC(dy, '\n');
t->text = dyStringCannibalize(&dy);
}

boolean hasBlankLine(char *text)
/* Return TRUE if there is an empty line in text. */
{
char *s, *e;
for (s = text; !isEmpty(s); s = e)
    {
    e = strchr(s, '\n');
    if (e == s)
        return TRUE;
    else
        e += 1;
    }
return FALSE;
}

void makeSureBlankLineBefore(struct raRecord *r)
/* Make sure there is a blank line before record. */
{
struct raTag *first = r->tagList;
char *firstText = first->text;
if (!hasBlankLine(firstText))
    {
    int len = strlen(firstText);
    char *newText = needMem(len+2);
    newText[0] = '\n';
    strcpy(newText+1, firstText);
    first->text = newText;
    }
}

void addToStartOfTextLines(struct raRecord *r, char c, int charCount)
/* Add char to start of all text in r. */
{
struct raTag *t;
struct dyString *dy = dyStringNew(0);
for (t = r->tagList; t != NULL; t = t->next)
    {
    char *s, *e;
    dyStringClear(dy);
    for (s = t->text; !isEmpty(s); s = e)
        {
	int i;
	e = strchr(s, '\n');
	if (e == s)  // empty line, keep empty
	    {
	    dyStringAppendC(dy, '\n');
	    e += 1;
	    }
	else
	    {
	    // Indent some extra.
	    for (i=0; i<charCount; ++i)
		dyStringAppendC(dy, c);
	    if (e == NULL)
		{
		dyStringAppend(dy, s);
		}
	    else
		{
		dyStringAppendN(dy, s, e-s+1);
		e += 1;
		}
	    }
	}
    t->text = cloneString(dy->string);
    }
if (r->endComments != NULL)
    {
    dyStringClear(dy);
    int i;
    for (i=0; i<charCount; ++i)
        dyStringAppendC(dy, c);
    dyStringAppend(dy, r->endComments);
    r->endComments = cloneString(dy->string);
    }
dyStringFree(&dy);
}

void indentTdbText(struct raRecord *r, int indentCount)
/* Add spaces to start of all text in r. */
{
addToStartOfTextLines(r, ' ', indentCount);
}

void commentOutStanza(struct raRecord *r)
/* Add # to start of all test in r. */
{
addToStartOfTextLines(r, '#', 1);
}

void substituteIntoView(struct raRecord *sub, struct raRecord *oldSub, struct raRecord *view)
/* Substitute sub for oldSub as a child of view.  Assumes oldSub is in same file and after view.
 * Leaves in oldSub, but "commented out" */
{
sub->parent = view;
sub->next = oldSub->next;
oldSub->next = sub;
if (clNoComment)
    oldSub->isRemoved = TRUE;
else
    commentOutStanza(oldSub);
}

void patchInSubtrack(struct raRecord *parent, struct raRecord *sub)
/* Patch sub into the correct view of parent */
{
char *viewOfSub = findViewOfSub(sub);
if (viewOfSub == NULL)
    recordAbort(sub, "Can only handle subtracks with view in subGroups");
char *viewTrackName = findViewTrackName(parent,viewOfSub);
if(viewTrackName == NULL)
    recordAbort(sub, "Can't find view track name for %s in subtrack %s and parent %s.",viewOfSub,sub->key,parent->key);
char *parentRelease = raRecordFindTagVal(parent, "release");
char *subRelease = raRecordFindTagVal(sub, "release");
char *release = nonNullRelease(parentRelease, subRelease);
struct raRecord *view = findRecordCompatibleWithRelease(parent->file, release, viewTrackName);
validateParentViewSub(parent, view, sub);
substituteParentText(parent, view, sub);
makeSureBlankLineBefore(sub);
if(raRecordFindTag(sub, "subTrack"))
    indentTdbText(sub, 4);
struct raRecord *oldSub = findRecordCompatibleWithRelease(parent->file, release, sub->key);
if (glReplace)
    {
    if (oldSub == NULL)
        recordAbort(sub, "%s doesn't exist but using mode=replace\n", sub->key);
    substituteIntoView(sub, oldSub, view);
    }
else
    {
    if (oldSub != NULL)
        recordAbort(sub, "record %s already exists - use mode=replace", sub->key);
    patchIntoEndOfView(sub, view);
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


void encodePatchTdb(char *patchFileName, char *tdbFileName)
/* encodePatchTdb - Lay a trackDb.ra file from the pipeline gently on top of the trackDb system. */
{
struct raFile *patchFile = raFileRead(patchFileName);
struct raRecord *patchList = patchFile->recordList;
int trackCount = slCount(patchList);
if (trackCount < 1)
    errAbort("No tracks in %s", patchFileName);

boolean hasTrack = compositeFirst(patchList);

/* Find parent track name. */
char *parentName;
struct raRecord *subList;
if (hasTrack)
    {
    parentName = patchList->key;
    subList = patchList->next;
    }
else
    {
    char *val = raRecordFindTagVal(patchList, "parent");
    if (val == NULL)
        val = raRecordMustFindTagVal(patchList, "subTrack");
    parentName = cloneFirstWord(val);
    subList = patchList;
    val = strstr(parentName,"View"); // NOTE: Requires viewInTheMiddle follows naming convention {parentTrack}View{ViewName}
    if ( val != NULL)
        *val = '\0';  // Chop off the view portion of the name.
    }
checkSubsAreForParent(parentName, subList);

/* Load file to patch. */
struct raFile *tdbFile = raFileRead(tdbFileName);
int oldTdbCount = slCount(tdbFile->recordList);
if (oldTdbCount < 50)
    warn("%s only has %d records, I hope you meant to hit a new file\n", tdbFileName,
    	oldTdbCount);
linkUpParents(tdbFile);

struct raRecord *tdbParent = findRecordCompatibleWithRelease(tdbFile, "alpha", parentName);
if (!tdbParent)
    errAbort("Can't find composite track %s compatible with alpha mode in %s",
    	parentName, tdbFileName);
patchInSubtracks(tdbParent, subList);

char *outName = tdbFileName;
if (clTest != NULL)
    outName = clTest;
FILE *f = mustOpen(outName, "w");
struct raRecord *r;
for (r = tdbFile->recordList; r != NULL; r = r->next)
    {
    if (!r->isRemoved)
	{
	struct raTag *tag;
	for (tag = r->tagList; tag != NULL; tag = tag->next)
	    {
	    fputs(tag->text, f);
	    }
	if (r->endComments != NULL)
	    fputs(r->endComments, f);
	}
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
clMode = optionVal("mode", clMode);
clTest = optionVal("test", clTest);
clNoComment = optionExists("noComment");
if (sameString(clMode, "add"))
    glReplace = FALSE;
else if (sameString(clMode, "replace"))
    glReplace = TRUE;
else
    errAbort("unrecognized mode %s", clMode);
encodePatchTdb(argv[1], argv[2]);
return 0;
}
