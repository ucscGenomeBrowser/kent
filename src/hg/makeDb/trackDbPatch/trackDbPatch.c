/* trackDbPatch - Patch files in trackDb with a specification .ra file that has db, track, and file fields that say where to apply the patch, and other fields that are patched in.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "ra.h"

static char const rcsid[] = "$Id: trackDbPatch.c,v 1.1 2009/11/22 23:56:30 kent Exp $";

char *clPatchDir = NULL;
char *clKey = "track";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trackDbPatch - Patch files in trackDb with a specification .ra file that has db, track, and file fields that say where to apply the patch, and other fields that are patched in.\n"
  "usage:\n"
  "   trackDbPatch patches.ra backupDir\n"
  "options:\n"
  "   -test=patchDir - rather than doing patches in place, write patched output to this dir\n"
  "   -key=tagName - use tagName as key.  Default '%s'\n"
  , clKey
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct raPatch
/* A patch record. */
    {
    struct raPatch *next;
    char *db;		/* Database. */
    char *track;	/* Track. */
    struct slName *fileList;	/* List of files. */
    struct slPair *tagList;	/* List of tags to merge in. */
    };

static struct slName *makeFileList(char *filesAndPos)
/* Convert something that looks like "file # file #" to a list of files.  This
 * writes zeroes into the filesAndPos input. */
{
struct slName *list = NULL;
char *word;
while ((word = nextWord(&filesAndPos)) != NULL)
    {
    slNameAddTail(&list, word);
    word = nextWord(&filesAndPos);
    if (word == NULL && !isdigit(word[0]))
        errAbort("Expecting number in makeFileList, got %s", word);
    }
return list;
}

struct raPatch *raPatchReadOne(struct lineFile *lf)
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
    else if (sameString(tag->name, "file"))
        {
	patch->fileList = makeFileList(tag->val);
	freeMem(tag->val);
	freez(&tag);
	}
    else
        {
	slAddHead(&newTagList, tag);
	}
    }
slReverse(&newTagList);

if (patch->track == NULL)
    errAbort("Missing %s tag before line %d of %s", clKey, lf->lineIx, lf->fileName);
if (patch->fileList == NULL)
    errAbort("Missing %s tag before line %d of %s", "file", lf->lineIx, lf->fileName);
return patch;
}

void trackDbPatch(char *patchesFile, char *backupDir)
/* trackDbPatch - Patch files in trackDb with a specification .ra file that has db, track, and file fields that say 
 * where to apply the patch, and other fields that are patched in.. */
{
/* Read in patch file. */
struct lineFile *lf = lineFileOpen(patchesFile, TRUE);
struct raPatch *patch, *patchList = NULL;
while ((patch = raPatchReadOne(lf)) != NULL)
    slAddHead(&patchList, patch);
slReverse(&patchList);
verbose(1, "Got %d patches in %s\n", slCount(patchList), patchesFile);

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
trackDbPatch(argv[1], argv[2]);
return 0;
}
