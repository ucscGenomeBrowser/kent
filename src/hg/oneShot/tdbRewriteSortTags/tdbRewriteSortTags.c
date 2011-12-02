/* tdbRewriteSortTags - Stream through a tdb file and sort the tags in it.. */
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
#include "rql.h"


static char *clRoot = "~/kent/src/hg/makeDb/trackDb";	/* Root dir of trackDb system. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tdbRewriteSortTags - Stream through tdb system and sort the tags in it.\n"
  "usage:\n"
  "   tdbRewriteSortTags outDir\n"
  "options:\n"
  "   -root=/path/to/trackDb/root/dir\n"
  "Sets the root directory of the trackDb.ra directory hierarchy to be given path. By default\n"
  "this is ~/kent/src/hg/makeDb/trackDb.\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct raTag
/* A tag in a .ra file. */
    {
    struct raTag *next;
    char *name;		/* Name of tag. */
    char *val;		/* Value of tag. */
    char *text;		/* Text - including white space and comments before tag. */
    };

static void raTagWrite(struct raTag *tag, FILE *f)
/* Write tag to file */
{
fputs(tag->text, f);
}

int raTagCmp(const void *va, const void *vb)
/* Compare two raTags. */
{
const struct raTag *a = *((struct raTag **)va);
const struct raTag *b = *((struct raTag **)vb);
return strcmp(a->name, b->name);
}

struct raRecord
/* A record in a .ra file. */
    {
    struct raTag *tagList;	/* List of tags that make us up. */
    int startLineIx, endLineIx; /* Start and end in file for error reporting. */
    };

static void rewriteOneFile(char *inFile, char *outFile)
/* Rewrite file. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct dyString *dy = dyStringNew(0);
while (raSkipLeadingEmptyLines(lf, dy))
    {
    /* Create memory pool for loop. */
    struct lm *lm = lmInit(0);

    /* Create a tag structure in local memory. */
    struct raRecord *r;
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
	slAddHead(&r->tagList, tag);
	dyStringClear(dy);
	}
    fputs(dy->string, f);  /* Catches trailing comments in stanza */
    slReverse(&r->tagList);
    r->endLineIx = lf->lineIx;

    /** Write out revised version of record. **/

    /* First line is unchanged. */
    raTagWrite(r->tagList, f);

    /* Subsequent lines are sorted. */
    slSort(&r->tagList->next, raTagCmp);
    struct raTag *tag;
    for (tag = r->tagList->next; tag != NULL; tag = tag->next)
        raTagWrite(tag, f);

    /* Clean up memory pool for loop. */
    lmCleanup(&lm);
    }
fputs(dy->string, f);		/* Print part of file after last stanza */
dyStringFree(&dy);
carefulClose(&f);
lineFileClose(&lf);
}

static void rewriteInsideSubdir(char *outDir, char *inDir, char *trackFile)
/* Do some sort of rewrite on one subdirectory. */
{
char tIn[PATH_LEN];
safef(tIn, sizeof(tIn), "%s/%s", inDir, trackFile);
if (fileExists(tIn))
     {
     char tOut[PATH_LEN];
     safef(tOut, sizeof(tOut), "%s/%s", outDir, trackFile);
     makeDirsOnPath(outDir);
     rewriteOneFile(tIn, tOut);
     }
}

void doRewrite(char *outDir, char *inDir, char *trackFile)
/* Do some sort of rewrite on entire system. */
{
struct fileInfo *org, *orgList = listDirX(inDir, "*", FALSE);
rewriteInsideSubdir(outDir, inDir, trackFile);
for (org = orgList; org != NULL; org = org->next)
    {
    if (org->isDir)
	{
	char inOrgDir[PATH_LEN], outOrgDir[PATH_LEN];
	safef(inOrgDir, sizeof(inOrgDir), "%s/%s", inDir, org->name);
	safef(outOrgDir, sizeof(outOrgDir), "%s/%s", outDir, org->name);
	rewriteInsideSubdir(outOrgDir, inOrgDir, trackFile);
	struct fileInfo *db, *dbList = listDirX(inOrgDir, "*", FALSE);
	for (db = dbList; db != NULL; db = db->next)
	    {
	    if (db->isDir)
	        {
		char inDbDir[PATH_LEN], outDbDir[PATH_LEN];
		safef(inDbDir, sizeof(inDbDir), "%s/%s", inOrgDir, db->name);
		safef(outDbDir, sizeof(outDbDir), "%s/%s", outOrgDir, db->name);
		rewriteInsideSubdir(outDbDir, inDbDir, trackFile);
		}
	    }
	}
    }
}

void tdbRewriteSortTags(char *outDir)
/* tdbRewriteSortTags - Stream through a tdb file and sort the tags in it.. */
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
tdbRewriteSortTags(argv[1]);
return 0;
}
