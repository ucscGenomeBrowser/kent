/* encode2CopyManifest - Copy files in encode2 manifest and in case of tar'd files rezip them 
 * independently. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "md5.h"
#include "portable.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encode2CopyManifest - Copy files in encode2 manifest and in case of tar'd files rezip them independently.\n"
  "usage:\n"
  "   encode2CopyManifest sourceDir sourceManifest destDir destManifest\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

#define FILEINFO_NUM_COLS 6

struct manifestInfo
/* Information on one file */
    {
    struct manifestInfo *next;  /* Next in singly linked list. */
    char *fileName;	/* Name of file with directory relative to manifest */
    char *format;	/* bam fastq etc */
    char *experiment;	/* wgEncodeXXXX */
    char *replicate;	/* 1 2 both n/a */
    char *enrichedIn;	/* promoter exon etc. */
    char *md5sum;	/* Hash of file contents or n/a */
    };

struct manifestInfo *manifestInfoLoad(char **row)
/* Load a manifestInfo from row fetched with select * from manifestInfo
 * from database.  Dispose of this with manifestInfoFree(). */
{
struct manifestInfo *ret;

AllocVar(ret);
ret->fileName = cloneString(row[0]);
ret->format = cloneString(row[1]);
ret->experiment = cloneString(row[2]);
ret->replicate = cloneString(row[3]);
ret->enrichedIn = cloneString(row[4]);
ret->md5sum = cloneString(row[5]);
return ret;
}

void manifestInfoTabOut(struct manifestInfo *mi, FILE *f)
/* Write tab-separated version of manifestInfo to f */
{
fprintf(f, "%s\t", mi->fileName);
fprintf(f, "%s\t", mi->format);
fprintf(f, "%s\t", mi->experiment);
fprintf(f, "%s\t", mi->replicate);
fprintf(f, "%s\t", mi->enrichedIn);
fprintf(f, "%s\n", mi->md5sum);
}

struct manifestInfo *manifestInfoLoadAll(char *fileName)
/* Load all manifestInfos from file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[FILEINFO_NUM_COLS];
struct manifestInfo *list = NULL, *fi;
while (lineFileRow(lf, row))
   {
   fi = manifestInfoLoad(row);
   slAddHead(&list, fi);
   }
slReverse(&list);
return list;
}

void makeDirOnlyOnce(char *dir, struct hash *hash)
/* Check if dir is already in hash.  If so we're done.  If not make dir and add it to hash. */
{
if (!hashLookup(hash, dir))
    {
    verbose(2, "make dir %s\n", dir);
    hashAdd(hash, dir, NULL);
    makeDirs(dir);
    }
}

void removeDirectoryAndFilesInIt(char *dir)
/* A deliberately stunted version of "rm -r" at the command line.  This will
 * do a listing of files in a directory and remove each.  It will fail if any of
 * the files inside of the directory is actually itself a directory.  */
{
uglyf("removeDirectoryAndFilesInIt(%s)\n", dir);
#ifdef SOON
/* Get contents of directory and check to make sure no subdirs. */
struct fileInfo *fi, *fiList = listDirX(dir, "*", TRUE);
for (fi = fiList; fi != NULL; fi = fi->next)
    if (fi->isDir)
       errAbort("%s contains subdirs, aborting removeDirectoryAndFilesInIt", dir);

/* Delete contents. */
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    if (remove(fi->name) < 0)
        errnoAbort("Can't remove %s", fi->name);
    }
slFreeList(&fiList);


/* Delete dir itself. */
if (rmdir(dir) < 0)
    errnoAbort("Can't rmdir %s", dir);
#endif /* SOON */
}

void systemCommand(char *s)
/* Do system() command on s,  and check error status, aborting with message if
 * any problem. */
{
verbose(1, "systemCommand(%s)\n", s);
#ifdef SOON
int err = system(s);
if (err < 0)
    errAbort("err %d\nCouldn't %s", err, s);
#endif /* SOON */
}

void untgzIntoDir(char *tgzFile, char *dir)
/* Will do the equivalent of
 *    cd dir
 *    tar -zx tgzFile */
{
uglyf("Attempting untgzIntoDir(%s, %s)\n", tgzFile, dir);
char *origDir = cloneString(getCurrentDir());
struct dyString *command = dyStringNew(0);
setCurrentDir(dir);
dyStringPrintf(command, "tar -zxf %s", tgzFile);
uglyf("command: %s\n", command->string);
systemCommand(command->string);
dyStringFree(&command);
freez(&origDir);
}


void gzipFastqs(char *dir, struct manifestInfo *mi, FILE *f)
/* Check that all files in dir have fastq suffix, and gzip them. */
{
/* Get file list and check all are .fastq. */
struct fileInfo *fi, *fiList = listDirX(dir, "*", FALSE);
for (fi = fiList; fi != NULL; fi = fi->next)
    if (!endsWith(fi->name, ".fastq") && !endsWith(fi->name, ".fastq.gz"))
        errAbort("%s is not fastq inside %s", fi->name, dir);

/* Do system calls for gzip. */
struct dyString *cmd = dyStringNew(0);
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    if (!endsWith(fi->name, ".gz"))
	{
	dyStringPrintf(cmd, "gzip %s/%s", dir, fi->name);
	systemCommand(cmd->string);
	}
    }

/* Write out revised manifest. */
char *oldFileName = mi->fileName;
char *oldFormat = mi->format;
mi->format = "fastq";
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    char *suffix = (endsWith(fi->name, ".gz") ? "" : ".gz");
    char name[PATH_LEN];
    safef(name, sizeof(name), "%s.dir/%s%s", oldFileName, fi->name, suffix);
    mi->fileName = name;
    manifestInfoTabOut(mi, f);
    }
mi->format = oldFormat;
mi->fileName = oldFileName;
}


void processManifestItem(struct manifestInfo *mi, char *sourcePath, 
    char *destPath, struct hash *dirHash, char *destRootDir, FILE *f)
/* Process a line from the manifest.  Source path is the full path to the source file.
 * destPath is where to put the destination in the straightforward case where the destination
 * is just a single file.  In the more complex case dirHash and destDir are helpful.
 * The dirHash contains directories that are already known to exist, and helps keep the
 * program from making the same directory repeatedly.  The destRootDir parameter 
 * contains the top level destination dir.  The destPath is the same as destRootDir + mi->fileName
 * and typically mi->fileName will include a / or two.  Finally the f parameter is where
 * the program writes the revised manifest file,  after whatever transformation occurred in
 * the processing step.
 *    What occurs inside the function is:
 * o - Most files are just copied.  Optionally an md5-sum might be checked.
 * o - Files that are tgz's of multiple fastqs are split into individual fastq.gz's inside
 *     a directory named after the archive. */
{
uglyf("processManifestItem(%p, %s, %s, %p, %s, %p)\n", mi, sourcePath, destPath, dirHash, destRootDir, f);
if (endsWith(sourcePath, ".fastq.tgz"))
    {
    char outDir[PATH_LEN];
    safef(outDir, sizeof(outDir), "%s.dir", destPath);
    verbose(1, "Unpacking %s into %s\n", sourcePath, outDir);
    makeDir(outDir);
    untgzIntoDir(sourcePath, outDir);
    gzipFastqs(outDir, mi, f);
    }
else
    {
    verbose(1, "copyFile(%s,%s)\n", sourcePath, destPath);
    copyFile(sourcePath, destPath);
    manifestInfoTabOut(mi, f);
    }
}

void encode2CopyManifest(char *sourceDir, char *sourceManifest, char *destDir, char *destManifest)
/* encode2CopyManifest - Copy files in encode2 manifest and in case of tar'd files rezip them 
 * independently. */
{
struct manifestInfo *fileList = manifestInfoLoadAll(sourceManifest);
verbose(1, "Loaded information on %d files from %s\n", slCount(fileList), sourceManifest);
verboseTimeInit();
FILE *f = mustOpen(destManifest, "w");
struct manifestInfo *mi;
struct hash *destDirHash = hashNew(0);
makeDirOnlyOnce(destDir, destDirHash);
for (mi = fileList; mi != NULL; mi = mi->next)
    {
    /* Make path to source file. */
    char sourcePath[PATH_LEN];
    safef(sourcePath, sizeof(sourcePath), "%s/%s", sourceDir, mi->fileName);

    /* Make destination dir */
    char localDir[PATH_LEN];
    splitPath(mi->fileName, localDir, NULL, NULL);
    char destSubDir[PATH_LEN];
    safef(destSubDir, sizeof(destSubDir), "%s/%s", destDir, localDir);
    makeDirOnlyOnce(destSubDir, destDirHash);
    char destPath[PATH_LEN];
    safef(destPath, sizeof(destPath), "%s/%s", destDir, mi->fileName);

    processManifestItem(mi, sourcePath, destPath, destDirHash, destDir, f);

#ifdef OLD
    /* If md5sum available check it. */
    if (!sameString(mi->md5sum, "n/a"))
        {
	char *md5sum = md5HexForFile(sourcePath);
	verboseTime(1, "md5Summed %s (%lld bytes)", mi->fileName, (long long)fileSize(sourcePath));
	if (!sameString(md5sum, mi->md5sum))
	    {
	    warn("md5sum mismatch on %s, %s in metaDb vs %s in file", sourcePath, mi->md5sum, md5sum);
	    fprintf(f, "md5sum mismatch on %s, %s in metaDb vs %s in file\n", sourcePath, mi->md5sum, md5sum);
	    ++mismatch;
	    verbose(1, "%d md5sum matches, %d mismatches\n", match, mismatch);
	    }
	else 
	    ++match;
	}
#endif /* OLD */
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
encode2CopyManifest(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
