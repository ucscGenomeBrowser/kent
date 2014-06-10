/* multizzy - Run a lot of multiz jobs. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "obscure.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "multizzy - Run a lot of multiz jobs\n"
  "usage:\n"
  "   multizzy dirList scaffoldList destDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void oneMultiz(char *a, char *b, char *out) 
/* Call multiz if files exist, otherwise just copy. */
{
char command[3*PATH_LEN];
boolean aExists = fileExists(a);
boolean bExists = fileExists(b);
int ret;
if (aExists && bExists)
    {
    safef(command, sizeof(command), "%s %s %s - > %s", 
    	"multiz", a, b, out);
    ret = system(command);
    if (ret != 0)
        errAbort("%s failed status %d", command, ret);
    }
else if (aExists)
    {
    copyFile(a, out);
    }
else if (bExists)
    {
    copyFile(b, out);
    }
}

void multizzy(char *dirList, char *scaffoldList, char *destDir)
/* multizzy - Run a lot of multiz jobs. */
{
char *dirBuf, *scaffoldBuf;
int dirCount, scaffoldCount;
char **dirs, **scaffolds, *dir, *scaffold;
char command[512];
int dirIx, scaffoldIx;
char *tempNames[2];

makeDir(destDir);
tempNames[0] = "multizzy0.tmp";
tempNames[1] = "multizzy1.tmp";

readAllWords(dirList, &dirs, &dirCount, &dirBuf);
readAllWords(scaffoldList, &scaffolds, &scaffoldCount, &scaffoldBuf);
if (dirCount < 2)
   errAbort("Need at least 2 directories for multizzy to work."); 
for (scaffoldIx=0; scaffoldIx < scaffoldCount; ++scaffoldIx)
    {
    char *scaffold = scaffolds[scaffoldIx];
    char firstPath[PATH_LEN];
    char destPath[PATH_LEN];
    int flipper = 0;	/* FLip between 1 and 0. */
    verbose(1, "%s\n", scaffold);
    safef(firstPath, sizeof(firstPath), "%s/%s.maf", dirs[0], scaffold);
    safef(destPath, sizeof(destPath), "%s/%s.maf", destDir, scaffold);
    remove(tempNames[0]);
    remove(tempNames[1]);
    for (dirIx=1; dirIx < dirCount; ++dirIx)
        {
	char path[PATH_LEN];
	char *mid, *out;
	if (dirIx == 1)
	    mid = firstPath;
	else
	    mid = tempNames[flipper];
	if (dirIx == dirCount-1)
	    out = destPath;
	else
	    out = tempNames[1-flipper];
	safef(path, sizeof(path), "%s/%s.maf", dirs[dirIx], scaffold);
	oneMultiz(path, mid, out);
	flipper = 1 - flipper;
	}
    }
remove(tempNames[0]);
remove(tempNames[1]);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
multizzy(argv[1], argv[2], argv[3]);
return 0;
}
