/* Stuff to help handle a track that is a duplicate of a another - sharing data
 * and type, but having it's own cart settings. */

#include "common.h"
#include "hash.h"
#include "cart.h"
#include "dupTrack.h"
#include "ra.h"
#include "portable.h"
#include "trashDir.h"
#include "hgConfig.h"

boolean isDupTrack(char *track)
/* determine if track name refers to a custom track */
{
return (startsWith(DUP_TRACK_PREFIX, track));
}

char *dupTrackSkipToSourceName(char *dupeTrackName)
/* If it looks like it's a dupe track then skip over duppy part 
 * in particular skip over dup_N_ form prefix for numerical N. */
{
char *name = dupeTrackName;

if ((name != NULL ) && startsWith(DUP_TRACK_PREFIX, name))
    {
    char *s = name + strlen(DUP_TRACK_PREFIX);
    if (isdigit(s[0]))
        {
	s = skipNumeric(s);
	if (s[0] == '_')
	    return s+1;
	}
    }
return name;
}

static void makeDupName(char *sourceName, int n, char *buf, int bufSize)
/* Create name for dupe */
{
char *source = dupTrackSkipToSourceName(sourceName);
safef(buf, bufSize, "%s%d_%s", DUP_TRACK_PREFIX, n, source);
}

static void findUniqDupName(char *sourceName, struct hash *dupHash, char nameBuf[], int nameBufSize)
/* Make up a name of format dup_N_sourceTrack where N is a small unique number */
{
int i;
for (i=1; ;++i)
    {
    makeDupName(sourceName, i, nameBuf, nameBufSize);
    if (!hashLookup(dupHash, nameBuf))
	return;
    }
}

char *dupTrackInCartAndTrash(char *sourceTrack, struct cart *cart, struct trackDb *sourceTdb)
/* Update cart vars to reflect existance of duplicate of sourceTrack.
 * Also write out or append to dupe track trash file */
{
char *dupFileName = cartUsualString(cart, DUP_TRACKS_VAR, NULL);
FILE *f = NULL;	    // This will be our output

/* We keep duplicate's name here. */
int bufSize = strlen(sourceTrack) + 32;
char dupeTrackName[bufSize];
makeDupName(sourceTrack, 1, dupeTrackName, sizeof(dupeTrackName));

if (dupFileName != NULL && fileExists(dupFileName))   // Try and read in from old file
    {
    /* If there are already duplicates and trash cleaner hasn't nuked file read it in
     * and make sre we come up with a unique name before we append to existing file */
    struct hash *dupHash = raReadAll(dupFileName, "track");
    findUniqDupName(sourceTrack, dupHash, dupeTrackName, sizeof(dupeTrackName));
    f = mustOpen(dupFileName, "a");	// Append to old file but use new name
    fputc('\n', f);
    }
else
    {
    /* Otherwise we need to make up a new temp file and recored it in the cart */
    struct tempName dupTn;
    trashDirDateFile(&dupTn, "dup", "dup", ".ra");
    f = mustOpen(dupTn.forCgi, "w");
    cartSetString(cart, DUP_TRACKS_VAR, dupTn.forCgi);
    }
/* Write out new stanza */
fprintf(f, "track %s\n", dupeTrackName);
fprintf(f, "source %s\n", dupTrackSkipToSourceName(sourceTrack));
int dupNo = atoi(dupeTrackName + strlen(DUP_TRACK_PREFIX)) + 1;
if (sourceTdb != NULL)
    {
    fprintf(f, "shortLabel %s #%d\n", sourceTdb->shortLabel, dupNo);
    fprintf(f, "longLabel %s copy #%d\n", sourceTdb->longLabel, dupNo);
    }
else
    {
    fprintf(f, "%s #%d\n", sourceTrack, dupNo);
    fprintf(f, "longLabel %s copy #%d\n", sourceTrack, dupNo);
    }
carefulClose(&f);

cartCloneVarsWithPrefix(cart, sourceTrack, dupeTrackName);
return cloneString(dupeTrackName);
}

static void cartRemoveConfigVarsForTrack(struct cart *cart, char *track)
/* Remove track, track.* and track_*.  Perhaps this is more cautious than
 * track*? */
{
cartRemove(cart, track);
int nameSize = strlen(track);
char namePlusBuf[nameSize+2];
strcpy(namePlusBuf, track);
namePlusBuf[nameSize] = '.';
namePlusBuf[nameSize+1] = 0; 
cartRemovePrefix(cart, namePlusBuf);
namePlusBuf[nameSize] = '_';
cartRemovePrefix(cart, namePlusBuf);
}

static void dupTracksRemoveAllFromCart(struct cart *cart)
/* Remove all trace of dupe tracks from cart */
{
cartRemovePrefix(cart, DUP_TRACK_PREFIX);
}

void undupTrackInCartAndTrash(char *dupName, struct cart *cart)
/* Update cart vars to reflect removal of dupTrack.  Also reduce or 
 * remove trash file */
{
char *dupFileName = cartUsualString(cart, DUP_TRACKS_VAR, NULL);
if (dupFileName == NULL)
    return;  // Nothing to do here

/* We'll try and create our new list from info in old file */
struct dupTrack *newList = NULL;
if (fileExists(dupFileName))
    {
    struct dupTrack *dupList = dupTrackReadAll(dupFileName);

    /* Turn newList into a copy of dupList with our own stanza removed */
    struct dupTrack *dup, *next;
    for (dup = dupList; dup != NULL; dup = next)
        {
	next = dup->next;
	if (!sameString(dupName, dup->name))
	    slAddHead(&newList, dup);
	}
    slReverse(&newList);
    }

/* If we have any dupes left, write them to file, otherwise remove dup variable from cart */
if (newList == NULL)
    {
    dupTracksRemoveAllFromCart(cart);
    }
else
    {
    FILE *f = mustOpen(dupFileName, "w");
    struct dupTrack *dup;
    for (dup = newList; dup != NULL; dup = dup->next)
        {
	if (dup != newList)
	    fputc('\n', f);
	struct slPair *tag;
	for (tag = dup->tagList; tag != NULL; tag = tag->next)
	    fprintf(f, "%s\t%s\n", tag->name, (char *)(tag->val));
	}
    carefulClose(&f);
    cartRemoveConfigVarsForTrack(cart, dupName);
    }
}

struct dupTrack *dupTrackReadAll(char *fileName)
/* Read in ra file and return it as a list of dupTracks */
{
struct dupTrack *list = NULL;
struct slPair *tagList;
struct lineFile *lf = lineFileMayOpen(fileName, TRUE);
if (lf == NULL)
    return NULL;

while ((tagList = raNextStanzAsPairs(lf)) != NULL)
    {
    char *name = slPairFindVal(tagList, "track");
    if (name == NULL)
	errAbort("Trackless stanza ending line %d of %s", lf->lineIx, lf->fileName);
    char *source = slPairFindVal(tagList, "source");
    if (source == NULL)
       errAbort("Sourceless stanza ending line %d of %s", lf->lineIx, lf->fileName);
    struct dupTrack *dup;
    AllocVar(dup);
    dup->name = cloneString(name);
    dup->source = cloneString(source);
    dup->tagList = tagList;
    slAddHead(&list, dup);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

static void dupTrackAddInfoToTdb(struct dupTrack *dup, struct trackDb *tdb)
/* Add tags from dup to tdb */
{
struct slPair *tag;
for (tag = dup->tagList; tag != NULL; tag = tag->next)
    {
    if (!sameString(tag->name, "track"))
	{
	if (sameString(tag->name, "longLabel"))
	    tdb->longLabel = tag->val;
	else if (sameString(tag->name, "shortLabel"))
	    tdb->shortLabel = tag->val;
	hashAdd(tdb->settingsHash, tag->name, tag->val);
	}
    }
}

struct dupTrack *dupTrackListFromCart(struct cart *cart)
/* Consult cart for dup track variable and if it's there return
 * list of dupes */
{
char *dupFileName = cartUsualString(cart, DUP_TRACKS_VAR, NULL);
if (dupFileName == NULL)
    return NULL;
struct dupTrack *list = dupTrackReadAll(dupFileName);
if (list == NULL)	// Trash cleaner got it, so clean up cart too
    dupTracksRemoveAllFromCart(cart);
return list;
}

struct dupTrack *dupTrackFindInList(struct dupTrack *list, char *name)
/* Return matching element in list */
{
struct dupTrack *dup;
for (dup = list; dup != NULL; dup = dup->next)
   if (sameString(dup->name, name))
       break;
return dup;
}

struct trackDb *dupTdbFrom(struct trackDb *sourceTdb, struct dupTrack *dup)
/* Generate a duplicate trackDb */
{
/* Start with a basic shallow clone */
struct trackDb *tdb = CloneVar(sourceTdb);
tdb->next = NULL;
tdb->track = cloneString(dup->name);
dupTrackAddInfoToTdb(dup, tdb);
return tdb;
}

boolean dupTrackEnabled()
/* Return true if we allow tracks to be duped. */
{
return  cfgOptionBooleanDefault("canDupTracks", TRUE);
}

