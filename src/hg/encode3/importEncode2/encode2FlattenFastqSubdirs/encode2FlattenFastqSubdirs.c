/* encode2FlattenFastqSubdirs - Look at directory.  Move all files in subdirs to root dir and 
 * destroy subdirs.  Complain and die if any of non-dir files are anything but fastq.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encode2FlattenFastqSubdirs - Look at directory.  Move all files in subdirs to root dir and\n"
  "destroy subdirs.  Complain and die if any of non-dir files are anything but fastq.\n"
  "usage:\n"
  "   encode2FlattenFastqSubdirs rootDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void rListDir(char *dir, struct fileInfo **pList)
/* Return list of all files in dir and it's subdirs.  List will be deepest last. */
{
struct fileInfo *oneList = listDirX(dir, "*", TRUE);
struct fileInfo *fi, *next;
for (fi = oneList; fi != NULL; fi = fi->next)
    {
    if (fi->isDir)
        rListDir(fi->name, pList);
    }
for (fi = oneList; fi != NULL; fi = next)
    {
    next = fi->next;
    slAddHead(pList, fi);
    }
}

void removeEmptyDir(char *path)
/* Remove a dir that must be empty.  If it's not then squawk and die. */
{
int err = rmdir(path);
if (err != 0)
    errnoAbort("Couldn't remove directory %s", path);
}

void renameFile(char *oldPath, char *newPath)
/* Move file. */
{
int err = rename(oldPath, newPath);
if (err != 0)
    errnoAbort("Couldn't rename %s to %s", oldPath, newPath);
}

void encode2FlattenFastqSubdirs(char *rootDir)
/* encode2FlattenFastqSubdirs - Look at directory.  Move all files in subdirs to root dir and 
 * destroy subdirs.  Complain and die if any of non-dir files are anything but fastq. */
{
/* Get list of all files. */
struct fileInfo *fi, *fiList = NULL;
rListDir(rootDir, &fiList);
slReverse(&fiList);

/* Make sure anything that isn't a subdir is a fastq. */
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    if (!fi->isDir) 
        {
	if (!endsWith(fi->name, ".fastq"))
	    errAbort("Non-fastq %s in encode2FlattenFastqSubdir, aborting.", fi->name);
	}
    }

/* Move fastqs to root dir. */
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    if (!fi->isDir) 
        {
	char newPath[PATH_LEN];
	char fileName[FILENAME_LEN], fileExt[FILEEXT_LEN];
	splitPath(fi->name, NULL, fileName, fileExt);
	safef(newPath, sizeof(newPath), "%s/%s%s", rootDir, fileName, fileExt);
	renameFile(fi->name, newPath);
	}
    }

/* Remove subdirectories. This relies on depth first nature of list. */
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    if (fi->isDir)
        {
	removeEmptyDir(fi->name);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
encode2FlattenFastqSubdirs(argv[1]);
return 0;
}
