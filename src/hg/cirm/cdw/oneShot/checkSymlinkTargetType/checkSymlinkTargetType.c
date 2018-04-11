/* checkSymlinkTargetType - scan symlinks list. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "filePath.h"
#include "dystring.h"


char *prefix = "";
boolean brokenOnly = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkSymlinkTargetType - scan symlinks list to determine the target type.\n"
  "usage:\n"
  " find /data/cirm/wrangle -type l | checkSymlinkTargetType stdin\n"
  "options:\n"
  "   -prefix=/data/cirm/wrangle prepend all input with prefix.\n"
  "   -brokenOnly if specified, only output broken links.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"prefix", OPTION_STRING},
   {"brokenOnly", OPTION_BOOLEAN},
   {NULL, 0},
};

void showSymlinkType(char *submitFileName, char *submitDir)
/* Show symlink type */
{
struct stat sb;
char *lastPath = NULL;
char *path = cloneString(submitFileName);

// apply path to submitDir, giving an absolute path
char *newPath = expandRelativePath(submitDir, path);
verbose(3, "submitDir=%s\npath=%s\nnewPath=%s\n", submitDir, path, newPath);
if (!newPath)
    errAbort("Too many .. in path %s to make relative to submitDir %s\n", path, submitDir);
freeMem(path);
path = newPath;
struct dyString *dy = dyStringNew(256);
int symlinkLevels = 0;
boolean broken = FALSE;
while (TRUE)
    {
    dyStringPrintf(dy,"path=%s", path);
    if (lstat(path, &sb) == -1)
	{
	if (errno == ENOENT)
	    {
	    dyStringPrintf(dy," NOT FOUND");
	    broken = TRUE;
	    break;
	    }
	errnoAbort("stat failure on %s", path);
	}
    if ((sb.st_mode & S_IFMT) == S_IFLNK)
	dyStringPrintf(dy," SYMLINK ");
    else if ((sb.st_mode & S_IFMT) == S_IFREG)
	dyStringPrintf(dy," FILE");
    else if ((sb.st_mode & S_IFMT) == S_IFDIR)
	dyStringPrintf(dy," DIR");
    else
	dyStringPrintf(dy," SPECIAL");
    if ((sb.st_mode & S_IFMT) != S_IFLNK)
	break;

    // follow the symlink
    ++symlinkLevels;
    if (symlinkLevels > 10)
	errAbort("Too many symlinks followed: %d symlinks. Probably a symlink loop.", symlinkLevels);

    // read the symlink
    ssize_t nbytes, bufsiz;
    // determine whether the buffer returned was truncated.
    bufsiz = sb.st_size + 1;
    char *symPath = needMem(bufsiz);
    nbytes = readlink(path, symPath, bufsiz);
    if (nbytes == -1) 
	errnoAbort("readlink failure on symlink %s", path);
    if (nbytes == bufsiz)
        errAbort("readlink returned buffer truncated\n");


    // apply symPath to path
    newPath = pathRelativeToFile(path, symPath);
    verbose(3, "path=%s\nsymPath=%s\nnewPath=%s\n", path, symPath, newPath);
    if (!newPath)
        errAbort("Too many .. in symlink path %s to make relative to %s\n", symPath, path);
    if (lastPath)
	freeMem(lastPath);
    lastPath = path;
    freeMem(symPath);
    path = newPath;
    }
if (symlinkLevels < 1)
    {
    errAbort("Too few symlinks followed: %d symlinks.", symlinkLevels);
    }
freeMem(path);

if (!(brokenOnly && !broken))
    printf("%s\n", dy->string);

dyStringFree(&dy);
}

void checkSymlinkTargetType(char *inputList)
/* Check out the symlink to determine its type. */
{
struct lineFile *lf = NULL;
if (startsWith("stdin",inputList))
    lf = lineFileStdin(TRUE);
else
    lf = lineFileOpen(inputList, TRUE);
char *line = NULL;
while (lineFileNext(lf, &line, NULL))
    {
    // find creates paths starting with ./ in the current dir.
    if (startsWith("./", line))
	line += 2;   // skip over it
    verbose(3, "%s\n", line);
    showSymlinkType(line, prefix);
    }
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
prefix = optionVal("prefix", prefix);
brokenOnly = optionExists("brokenOnly");
checkSymlinkTargetType(argv[1]);
return 0;
}
