/* cdwUnlockSubmittedFile - unlock a symlink to a read only submitted file, returns it to a normal file. 
 *  This utility can be run from either the wrangle/ or submit/ directories. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "filePath.h"
#include "dystring.h"
#include "portable.h"
#include "obscure.h"
#include "cdwLib.h"


char *prefix = "";
boolean brokenOnly = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwUnlockSubmittedFile - turn a symlink to a read-only submitted file back into a regular file.\n"
  "usage:\n"
  " cdwUnlockSubmittedFile path\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void cdwUnlockSubmittedFile(char *submitFileName)
/* Return a symlink to a read-only submitted file back into a regular file. */
{
char *submitDir = cloneString(getCurrentDir());

char *lastPath = NULL;
char *path = NULL;
int symlinkLevels = 0;

int result = findSubmitSymlinkExt(submitFileName, submitDir, &path, &lastPath, &symlinkLevels);
if (result == -1)  // path does not exist
    {
    errAbort("path=[%s] does not exist following submitDir/submitFileName through symlinks.", path);
    }
if (symlinkLevels < 1)
    {
    errAbort("Too few symlinks followed: %d symlinks. Where is the symlink created by cdwSubmit?", symlinkLevels);
    }
verbose(3, "lastPath=%s path=%s\n", lastPath, path);
if (!startsWith(cdwRootDir, path))
    {
    errAbort("expected path=[%s] to start with %s", path, cdwRootDir);
    }

if (unlink(lastPath) == -1)  // drop about to be invalid symlink
    errnoAbort("unlink failure %s", lastPath);
copyFile(path, lastPath);
touchFileFromFile(path, lastPath);
chmod(lastPath, 0664);

freeMem(lastPath);
freeMem(path);

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cdwUnlockSubmittedFile(argv[1]);
return 0;
}
