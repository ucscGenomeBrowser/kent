/* tdbRewriteSubtrackToParent - Convert subtrack tags to parent tags.. */
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

char *newTag = "parent";
char *oldTag = "subTrack";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tdbRewriteSubtrackToParent - Convert subtrack tags to parent tags.\n"
  "usage:\n"
  "   tdbRewriteSubtrackToParent outDir\n"
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
static void rewriteOneFile(char *inFile, char *outFile)
/* Rewrite file. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct dyString *dy = dyStringNew(0);
while (raSkipLeadingEmptyLines(lf, dy))
    {
    char *name, *val;
    while (raNextTagVal(lf, &name, &val, dy))
        {
	if (sameString(oldTag, name))
	    {
	    char *s = dy->string;
	    char *e = firstTagInText(dy->string);
	    mustWrite(f, s, e-s);
	    fputs(newTag, f);
	    s = skipToSpaces(e);
	    fputs(s, f);
	    }
	else
	    {
	    fputs(dy->string, f);
	    }
	dyStringClear(dy);
	}
    fputs(dy->string, f);  /* Catches trailing comments in stanza */
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

void tdbRewriteSubtrackToParent(char *outDir)
/* tdbRewriteSubtrackToParent - Convert subtrack tags to parent tags.. */
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
tdbRewriteSubtrackToParent(argv[1]);
return 0;
}
