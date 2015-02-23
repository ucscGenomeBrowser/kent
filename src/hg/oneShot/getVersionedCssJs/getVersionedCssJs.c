/* getVersionedCssJs - Create directory full of versioned .css and .js files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "getVersionedCssJs - Create directory full of versioned .css and .js files\n"
  "usage:\n"
  "   getVersionedCssJs version inDir outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void copyVersioned(char *version, char *oldDir, char *newDir, char *suffix)
/* Copy files from old to new dir, adding version */
{
char wildcard[32];
safef(wildcard, sizeof(wildcard), "*%s", suffix);
struct fileInfo *fi, *fiList = listDirX(oldDir, wildcard, TRUE);
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    char oldFile[PATH_LEN];
    splitPath(fi->name, NULL, oldFile, NULL);
    if (strchr(oldFile, '-') == NULL)
        {
	char newPath[PATH_LEN];
	safef(newPath, sizeof(newPath), "%s/%s-%s%s", newDir, oldFile, version, suffix);
	char command[3*PATH_LEN];
	safef(command, sizeof(command), "cp -av %s %s", fi->name, newPath);
	verbose(1, "%s\n", command);
	mustSystem(command);
	}
    }
}

void getVersionedCssJs(char *version, char *inDir, char *outDir)
/* getVersionedCssJs - Create directory full of versioned .css and .js files. */
{
/* Figure out old and new subdir names, make new ones. */
char oldStyleDir[PATH_LEN], oldJsDir[PATH_LEN];
char newStyleDir[PATH_LEN], newJsDir[PATH_LEN];
safef(oldStyleDir, sizeof(oldStyleDir), "%s/style", inDir);
safef(oldJsDir, sizeof(oldJsDir), "%s/js", inDir);
safef(newStyleDir, sizeof(newStyleDir), "%s/style", outDir);
safef(newJsDir, sizeof(newJsDir), "%s/js", outDir);

makeDirsOnPath(newStyleDir);
copyVersioned(version, oldStyleDir, newStyleDir, ".css");

makeDirsOnPath(newJsDir);
copyVersioned(version, oldJsDir, newJsDir, ".js");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
getVersionedCssJs(argv[1], argv[2], argv[3]);
return 0;
}
