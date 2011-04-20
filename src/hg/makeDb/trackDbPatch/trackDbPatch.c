/* trackDbPatch - Patch files in trackDb with a specification .ra file that has db, track, and file fields that say where to apply the patch, and other fields that are patched in.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "ra.h"
#include "portable.h"

static char const rcsid[] = "$Id: trackDbPatch.c,v 1.6 2010/05/29 22:19:50 kent Exp $";

char *clPatchDir = NULL;
char *clKey = "track";
boolean clFirstFile = FALSE;
boolean clMultiFile = FALSE;
boolean clDelete = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trackDbPatch - Patch files in trackDb with a specification .ra file that has db, track, \n"
  "and filePos fields that say where to apply the patch, and other fields that are patched in.\n"
  "The filePos field contains a file name and a line number. Currently the line number is ignored\n"
  "usage:\n"
  "   trackDbPatch patches.ra backupDir\n"
  "options:\n"
  "   -test=patchDir - rather than doing patches in place, write patched output to this dir\n"
  "   -key=tagName - use tagName as key.  Default '%s'\n"
  "   -multiFile - allow multiple files in filePos tag\n"
  "   -firstFile - when a patch can go to multiple files apply it to first rather than last file\n"
  "   -delete - delete tracks in patches.ra, which should contain only track and filePos tags\n"
  , clKey
  );
}

static struct optionSpec options[] = {
   {"test", OPTION_STRING},
   {"key", OPTION_STRING},
   {"multiFile", OPTION_BOOLEAN},
   {"firstFile", OPTION_BOOLEAN},
   {"delete", OPTION_BOOLEAN},
   {NULL, 0},
};


/* Program first reads in patchs to list of raPatches.  Then it creates a list of filePatches
 * so that it can do all patches in one pile at once. */

struct raPatch
/* A patch record. */
    {
    struct raPatch *next;
    char *db;		/* Database. */
    char *track;	/* Track. */
    struct slName *fileList;	/* List of files. */
    struct slPair *tagList;	/* List of tags to merge in. */
    };

struct fileToPatch
/* A file needing patching */
    {
    struct fileToPatch *next;
    char *fileName;		/* Name of file. */
    struct slRef *patchList;	/* References to raPatches associated with file.  */
    };

int glPatchFieldModifyCount = 0;
int glPatchFieldAddCount = 0;
int glPatchRecordCount = 0;

struct fileToPatch *groupPatchesByFiles(struct raPatch *patchList, boolean firstFile)
/* Make fileToPatch list that covers all files in patchList. If lastFile is set will apply patch to first (as
 * opposed to the usual last) file in list of files  associated with a patch. */
{
struct fileToPatch *fileList = NULL, *file;
struct hash *fileHash = hashNew(0);
struct raPatch *patch;

for (patch = patchList; patch != NULL; patch = patch->next)
    {
    struct slName *fileName = patch->fileList;
    if (!firstFile)
        fileName = slLastEl(patch->fileList);
    assert(fileName);
    file = hashFindVal(fileHash, fileName->name);
    if (file == NULL)
        {
	AllocVar(file);
	file->fileName = cloneString(fileName->name);
	slAddHead(&fileList, file);
	hashAdd(fileHash, file->fileName, file);
	}
    refAdd(&file->patchList, patch);
    }

/* Straighten out all lists. */
slReverse(&fileList);
for (file = fileList; file != NULL; file = file->next)
    slReverse(&file->patchList);
hashFree(&fileHash);
return fileList;
}


static struct slName *makeFileList(struct lineFile *lf, char *filesAndPos)
/* Convert something that looks like "file # file #" to a list of files.  This
 * writes zeroes into the filesAndPos input.  The lf parameter is just for
 * error reporting. */
{
struct slName *list = NULL;
char *word;
while ((word = nextWord(&filesAndPos)) != NULL)
    {
    slNameAddTail(&list, word);
    word = nextWord(&filesAndPos);
    if (word == NULL || !isdigit(word[0]))
        errAbort("Expecting number in filePos tag, got %s, line %d of %s", word,
		lf->lineIx, lf->fileName);
    }
return list;
}

struct raPatch *raPatchReadOne(struct lineFile *lf, boolean deleteFieldsOnly)
/* Read next patch from lineFile. */
{
struct slPair *tagList = raNextRecordAsSlPairList(lf);
if (tagList == NULL)
    return NULL;

/* Go through tag list, diverting some tags to be actual fields in patch. */
struct slPair *newTagList = NULL, *tag, *next;
struct raPatch *patch;
AllocVar(patch);
for (tag = tagList; tag != NULL; tag = next)
    {
    next = tag->next;
    if (sameString(tag->name, "db"))
	{
        patch->db = tag->val;
	freez(&tag);
	}
    else if (sameString(tag->name, clKey))
        {
	patch->track = tag->val;
	freez(&tag);
	}
    else if (sameString(tag->name, "filePos"))
        {
	patch->fileList = makeFileList(lf, tag->val);
	int fileCount = slCount(patch->fileList);
	if (fileCount != 1)
	    {
	    if (fileCount == 0)
	        errAbort("Empty filePos tag near line %d of %s", lf->lineIx, lf->fileName);
	    else if (!clMultiFile)
	        errAbort("Multiple files in filePos near line %d of %s. "
			"Use -multiFile option if this is not a mistake.",
			lf->lineIx, lf->fileName);
	    }
	freeMem(tag->val);
	freez(&tag);
	}
    else
        {
	if (deleteFieldsOnly)
	    errAbort("Tag %s not allowed with -delete mode in stanza ending line %d of %s.",
	    	tag->name, lf->lineIx, lf->fileName);
	slAddHead(&newTagList, tag);
	}
    }
slReverse(&newTagList);

if (patch->track == NULL)
    errAbort("Missing %s tag before line %d of %s", clKey, lf->lineIx, lf->fileName);
if (patch->fileList == NULL)
    errAbort("Missing %s tag before line %d of %s", "file", lf->lineIx, lf->fileName);
patch->tagList= newTagList;
return patch;
}

char *findLastMatchInString(char *string, char *match)
/* Return last position in string that starts with match.  Return NULL
 * if no match */
{
int matchLen = strlen(match);
int stringLen = strlen(string);
int startIx = stringLen - matchLen + 1;
while (--startIx >= 0)
    {
    if (memcmp(string + startIx, match, matchLen) == 0)
        return string + startIx;
    }
return NULL;
}

char *skipTrackDbPathPrefix(char *full)
/* Get suffix of full that skips the trackDb source code position. */
{
char *pat = "makeDb/trackDb/";
char *start = findLastMatchInString(full, pat);
if (start != NULL)
    start += strlen(pat);
return start;
}


void makeDirForFile(char *file)
/* Create directory that fill will sit in if it doesn't already exist. */
{
char dir[PATH_LEN], name[FILENAME_LEN], extension[FILEEXT_LEN];
splitPath(file, dir, name, extension);
if (dir[0] != 0)
    {
    char *simplePath = simplifyPathToDir(dir);
    if (simplePath[0] != 0)
	{
	makeDirsOnPath(simplePath);
	}
    freeMem(simplePath);
    }
}

void mustRename(char *oldPath, char *newPath)
/* Rename.  If fail print error message and abort. */
{
int err = rename(oldPath, newPath);
if (err != 0)
    errnoAbort("Couldn't rename %s to %s", oldPath, newPath);
}

static void applyPatches(char *inName, struct slRef *patchRefList, char *keyField, char *outName,
	boolean doDelete)
/* Apply patches in list. */
{
int keyFieldLen = strlen(keyField);

/* Convert list of patch references to hash of patches. */
struct slRef *ref;
struct hash *patchHash = hashNew(0);
for (ref = patchRefList; ref != NULL; ref = ref->next)
    {
    struct raPatch *patch = ref->val;
    char *key = cloneFirstWord(patch->track);
    hashAdd(patchHash, key, patch);
    freeMem(key);
    }
verbose(2, "%d patches in hash, %d in list\n", patchHash->elCount, slCount(patchRefList));

/* Open input and output files. */
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");

/* Scan through one stanza at a time. */
for (;;)
    {
    /* First just fetch stanza - including any blank and comment lines before, into a list of strings. */
    struct slPair *stanza = raNextStanzaLinesAndUntouched(lf);
    if (stanza == NULL)
        break;

    /* Go through stanza once just to see if have any patches to apply. */
    struct raPatch *patch = NULL;
    struct slPair *line;
    for (line = stanza; line != NULL; line = line->next)
        {
	char *tagStart = skipLeadingSpaces(line->name);
	if (startsWithWord(keyField, tagStart))
	     {
	     char *valStart = skipLeadingSpaces(tagStart + keyFieldLen);
	     char *key = cloneFirstWord(valStart);
	     patch = hashFindVal(patchHash, key);
	     freeMem(key);
	     break;
	     }
	}

    /* If have patch apply it, otherwise just copy. */
    if (patch)
        {
	++glPatchRecordCount;
	if (doDelete)
	    verbose(2, "Deleting %s in %s\n", patch->track, inName);
	else
	    {
	    verbose(3, "Got patch %s with %d tags starting %s %s\n", patch->track, slCount(patch->tagList), patch->tagList->name, (char *)patch->tagList->val);
	    struct hash *appliedHash = hashNew(0);	// keep track of tags patched in

	    /* Go through stanza looking for tags to update. */
	    char *lineStart = NULL;	// At end of loop points to last line
	    int indent = 0;	// # of whitespace chars
            for (line = stanza; line != NULL; line = line->next)
		{
		lineStart = line->name;
		char *tagStart = skipLeadingSpaces(lineStart);
		boolean copyLine = TRUE;
		if (tagStart[0] != 0 && tagStart[0] != '#')
		    {
		    indent = tagStart - lineStart;
		    struct slPair *tagPatch;
		    for (tagPatch = patch->tagList; tagPatch != NULL; tagPatch = tagPatch->next)
			{
			if (startsWithWord(tagPatch->name, tagStart))
			    {
			    copyLine = FALSE;
			    mustWrite(f, lineStart, indent);
			    fprintf(f, "%s %s\n", tagPatch->name, (char*)tagPatch->val);
			    verbose(2, "Applying patch '%s' to modify %s'\n", (char*)tagPatch->val, tagStart);
			    ++glPatchFieldModifyCount;
			    hashAdd(appliedHash, tagPatch->name, NULL);
			    break;
			    }
			}
		    }
		if (copyLine)
		    {
		    fprintf(f, "%s\n", line->val != NULL ? (char *)(line->val) : line->name);
		    }
		}

	    /* Go through and add any tags not already patched in. */
	    struct slPair *tagPatch;
	    for (tagPatch = patch->tagList; tagPatch != NULL; tagPatch = tagPatch->next)
		{
		if (!hashLookup(appliedHash, tagPatch->name))
		    {
		    ++glPatchFieldAddCount;
		    verbose(2, "Applying patch to %s adding %s %s\n", patch->track, tagPatch->name, (char*)tagPatch->val);
		    mustWrite(f, lineStart, indent);
		    fprintf(f, "%s %s\n", tagPatch->name, (char*)tagPatch->val);
		    hashAdd(appliedHash, tagPatch->name, NULL);
		    }
		}
	    hashFree(&appliedHash);
	    }
	}
    else
        {
	for (line = stanza; line != NULL; line = line->next)
	    {
	    if (startsWithWord("track", skipLeadingSpaces(line->name)))
		verbose(3, "copying %s unchanged\n", line->name);
            fprintf(f, "%s\n", line->val != NULL ? (char *)(line->val) : line->name);
	    }
	}

    for (line = stanza; line != NULL; line = line->next)
        {
        if (line->val != NULL)
            freeMem(line->val);
        }
    slPairFreeList(&stanza);
    }
lineFileClose(&lf);
carefulClose(&f);
}

void trackDbPatch(char *patchesFile, char *backupDir)
/* trackDbPatch - Patch files in trackDb with a specification .ra file that has db, track, and file fields that say
 * where to apply the patch, and other fields that are patched in.. */
{
/* Read in patch file. */
struct lineFile *lf = lineFileOpen(patchesFile, TRUE);
struct raPatch *patch, *patchList = NULL;
while ((patch = raPatchReadOne(lf, clDelete)) != NULL)
    {
    slAddHead(&patchList, patch);
    }
slReverse(&patchList);

/* Group it by file to patch */
struct fileToPatch *file, *fileList = groupPatchesByFiles(patchList, clFirstFile);
verbose(1, "Got %d patches covering %d files in %s\n", slCount(patchList), slCount(fileList), patchesFile);

/* Do some setting up for backups and patches */
makeDirsOnPath(backupDir);
if (clPatchDir != NULL)
    makeDirsOnPath(clPatchDir);

for (file = fileList; file != NULL; file = file->next)
    {
    /* Figure out name that skips most of the long path to the trackDb files */
    char *relName = skipTrackDbPathPrefix(file->fileName);
    if (relName == NULL)
         relName = file->fileName;

    /* Create file names for backup file and temp file. */
    char backupPath[PATH_LEN], patchPath[PATH_LEN];
    char tempPath[PATH_LEN];
    safef(backupPath, sizeof(backupPath), "%s/%s", backupDir, relName);
    safef(tempPath, sizeof(tempPath), "%s/%s.tmp", backupDir, relName);
    if (clPatchDir)
        safef(patchPath, sizeof(patchPath), "%s/%s", clPatchDir, relName);
    else
        safef(patchPath, sizeof(patchPath), "%s", file->fileName);


    /* Do patch reading original source and creating temp file. */
    makeDirForFile(backupPath);
    applyPatches(file->fileName, file->patchList, clKey, tempPath, clDelete);

    /* If testing, move temp to patch */
    if (clPatchDir)
	{
        makeDirForFile(patchPath);
	mustRename(tempPath, patchPath);
	copyFile(file->fileName, backupPath);
	}
    else
    /* If not testing then move original to backup and temp to original location. */
        {
	mustRename(file->fileName, backupPath);
	mustRename(tempPath, file->fileName);
	}
    }
verbose(1, "Modified %d records.  Modified %d fields, added %d fields\n",
	glPatchRecordCount, glPatchFieldModifyCount, glPatchFieldAddCount);

lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clKey = optionVal("key", clKey);
clPatchDir = optionVal("test", clPatchDir);
clFirstFile = optionExists("firstFile");
clMultiFile = optionExists("multiFile");
clDelete = optionExists("delete");
trackDbPatch(argv[1], argv[2]);
return 0;
}
