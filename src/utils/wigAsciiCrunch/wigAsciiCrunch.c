/* wigAsciiCrunch - Convert wigAscii to denser more flexible format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"

static char const rcsid[] = "$Id: wigAsciiCrunch.c,v 1.2 2004/10/28 05:24:09 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "wigAsciiCrunch - Convert wigAscii to denser more flexible format\n"
  "usage:\n"
  "   wigAsciiCrunch input output\n"
  "options:\n"
  "   -single=seqName input is all for a single sequence of the given name\n"
  "   -dir input is a directory. Sequence name is fileName minus suffix\n"
  "   -dirDir input is directory of directories.  Sequence name is subdir\n"
  "           All files in subdir sorted numerically by name and concatenated\n"
  "           into single sequence.\n"
  "Note the options single, dir, and dirDir are mutually exclusive, and one\n"
  "must be set\n"
  );
}

static struct optionSpec options[] = {
   {"single", OPTION_STRING},
   {"dir", OPTION_BOOLEAN},
   {"dirDir", OPTION_BOOLEAN},
   {NULL, 0},
};

char *clSingle = NULL;
boolean clDir = FALSE;
boolean clDirDir = FALSE;

void crunchOne(char *input, FILE *f, char *initialSeq)
/* Transform output to crunched format and append to file. */
{
long offset = 0, lastOffset = 0;
struct lineFile *lf = lineFileOpen(input, TRUE);
char *words[4];
int wordCount;
char *data = NULL;
char *seq = initialSeq, *lastSeq = NULL;

verbose(1, "%s\n", input);
while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    /* Read line that may already be crunched. */
    if (wordCount == 1)
        {
	++offset;
	data = words[0];
	}
    else if (wordCount == 2)
        {
	offset = lineFileNeedNum(lf, words, 0);
	data = words[1];
	}
    else if (wordCount == 3)
        {
	seq = words[0];
	offset = lineFileNeedNum(lf, words, 1);
	data = words[2];
	}
    else
        {
	errAbort("Expecting no more than 3 words, got %d line %d of %s",
		wordCount, lf->lineIx, lf->fileName);
	}

    if (lastSeq == NULL || !sameString(lastSeq, seq))
        {
	if (seq == NULL)
	    {
	    errAbort("No sequence name defined line %d of %s", 
	    	lf->lineIx, lf->fileName);
	    }
	fprintf(f, "%s\t", seq);
	freeMem(lastSeq);
	lastSeq = cloneString(seq);
	fprintf(f, "%ld\t", offset);
	}
    else if (lastOffset + 1 != offset)
	fprintf(f, "%ld\t", offset);
    fprintf(f, "%s\n", data);
    lastOffset = offset;
    }
freeMem(lastSeq);
lineFileClose(&lf);
}

void crunchDir(char *dir, FILE *f)
/* Crunch list of files in dir. */
{
struct fileInfo *fileList = listDirX(dir, NULL, FALSE), *file;
for (file = fileList; file != NULL; file = file->next)
    {
    char path[PATH_LEN];
    if (file->isDir)
        continue;
    safef(path, sizeof(path), "%s/%s", dir, file->name);
    chopSuffix(file->name);
    crunchOne(path, f, file->name);
    }
slFreeList(&fileList);
}

struct namePos
/* Record containing a name and a position. */
    {
    struct namePos *next;
    char *name;		/* Name (not allocated here) */
    int pos;		/* position. */
    };

int namePosCmp(const void *va, const void *vb)
/* Compare to sort based on pos. */
{
const struct namePos *a = *((struct namePos **)va);
const struct namePos *b = *((struct namePos **)vb);
return a->pos - b->pos;
}


int firstLinePos(char *fileName)
/* Return position of first line. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[3];
int retVal = 0;
int wordCount = lineFileChop(lf, words);
if (wordCount > 0)
    {
    if (wordCount != 2)
	errAbort("%s is not a two column file", fileName);
    retVal = lineFileNeedNum(lf, words, 0);
    lineFileClose(&lf);
    }
return retVal;
}

void crunchDirDir(char *dirDir, FILE *f)
/* Crunch dir of dirs . */
{
struct fileInfo *dirList = listDirX(dirDir, NULL, FALSE), *dir;
for (dir = dirList; dir != NULL; dir = dir->next)
    {
    struct fileInfo *fileList, *file;
    char path[PATH_LEN];
    struct namePos *posList = NULL, *pos;
    if (!dir->isDir)
	 {
         warn("%s isn't a dir, skipping", dir->name);
	 continue;
	 }
    if (sameString(dir->name, "CVS"))
         continue;	/* Skip CVS directories in test suite. */
    safef(path, sizeof(path), "%s/%s", dirDir, dir->name);
    fileList = listDirX(path, NULL, FALSE);
    for (file = fileList; file != NULL; file = file->next)
        {
	if (file->isDir)
	    continue;
	AllocVar(pos);
	pos->name = file->name;
	safef(path, sizeof(path), "%s/%s/%s", dirDir, dir->name, file->name);
	pos->pos = firstLinePos(path);
	slAddHead(&posList, pos);
	}
    slSort(&posList, namePosCmp);
    for (pos = posList; pos != NULL; pos = pos->next)
        {
	safef(path, sizeof(path), "%s/%s/%s", dirDir, dir->name, pos->name);
	crunchOne(path, f, dir->name);
	}
    slFreeList(&posList);
    slFreeList(&fileList);
    }
slFreeList(&dirList);
}

void wigAsciiCrunch(char *input, char *output)
/* wigAsciiCrunch - Convert wigAscii to denser more flexible format. */
{
FILE *f = mustOpen(output, "w");
if (clSingle != NULL)
    crunchOne(input, f, clSingle);
else if (clDir)
    crunchDir(input, f);
else if (clDirDir)
    crunchDirDir(input, f);
else
    errAbort("Please select either the single, dir, or dirDir option");
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clSingle = optionVal("single", NULL);
clDir = optionExists("dir");
clDirDir = optionExists("dirDir");
wigAsciiCrunch(argv[1], argv[2]);
return 0;
}
