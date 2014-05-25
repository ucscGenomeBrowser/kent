/* wigAsciiCrunch - Convert wigAscii to denser more flexible format. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "wigAsciiCrunch - Convert wigAscii to denser more flexible format\n"
  "usage:\n"
  "   wigAsciiCrunch input output\n"
  "require flag:\n"
  "   -single=seqName input is all for a single sequence of the given name\n"
  "   -dir input is a directory. Sequence name is fileName minus suffix\n"
  "   -dirDir input is directory of directories.  Sequence name is subdir\n"
  "           All files in subdir sorted numerically by name and concatenated\n"
  "           into single sequence.  This removes overlaps (first file to cover a\n"
  "           position wins). This is usually used with phastCons output.\n"
  "   -oneToThree - Use one to three column format\n"
  "Note the flags single, dir, and dirDir are mutually exclusive, and one\n"
  "must be set\n"
  "Options:\n"
  "   -fixOverlap - Removes overlapping regions (first wins)\n"
  );
}

static struct optionSpec options[] = {
   {"single", OPTION_STRING},
   {"dir", OPTION_BOOLEAN},
   {"dirDir", OPTION_BOOLEAN},
   {"fixOverlap", OPTION_BOOLEAN},
   {"oneToThree", OPTION_BOOLEAN},
   {NULL, 0},
};

char *clSingle = NULL;
boolean clDir = FALSE;
boolean clDirDir = FALSE;
boolean clFixOverlap = FALSE;
boolean clOneToThree = FALSE;

int crunchOne(char *input, FILE *f, char *initialSeq, int minPos)
/* Transform output to crunched format and append to file. */
{
long offset = 0, lastOffset = 0;
struct lineFile *lf = lineFileOpen(input, TRUE);
char *words[4];
int wordCount;
char *data = NULL;
char *seq = initialSeq, *lastSeq = NULL;
boolean newSeq;

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
    if (wordCount != 3)
        seq = lastSeq;	/* Memory here will last until next line. */

    /* If we've gotten this far, seq should be defined or it's a syntax error. */
    if (seq == NULL)
	{
	errAbort("No sequence name defined line %d of %s", 
	    lf->lineIx, lf->fileName);
	}

    /* See if it's a new sequence, and if so reset last offset. */
    newSeq = (lastSeq == NULL || !sameString(seq, lastSeq) );
    if (newSeq)
	minPos = lastOffset = 0;

    /* Check for stepping backwards.  Either error out, or if
     * command line is set, skip over things until we go forward
     * again. */
    if (offset < lastOffset && offset >= minPos)
        {
	if (clFixOverlap)
	    {
	    minPos = lastOffset;
	    verbose(1, "Removing overlap %d-%d line %d of %s\n", 
		    lastOffset, offset, lf->lineIx, lf->fileName);
	    }
	else
	    {
	    errAbort("Offsets going backwards line %d of %s", lf->lineIx, lf->fileName);
	    }
	}

    /* Check to see we are not screening out this as part of an overlap.
     * If not, print it. */
    if (offset >= minPos)
	{
	if (clOneToThree)
	    {
	    if (newSeq)
		{
		fprintf(f, "%s\t", seq);
		fprintf(f, "%ld\t", offset);
		}
	    else if (lastOffset + 1 != offset)
		fprintf(f, "%ld\t", offset);
	    }
	else
	    {
	    if (newSeq || lastOffset + 1 != offset)
	        {
		fprintf(f, "fixedStep chrom=%s start=%ld step=1\n", seq, offset);
		}
	    }
	fprintf(f, "%s\n", data);
	}

    /* Update variables that keep track of previous line. */
    if (newSeq)
	{
	freeMem(lastSeq);
	lastSeq = cloneString(seq);
	}
    lastOffset = offset;
    }
freeMem(lastSeq);
lineFileClose(&lf);
return offset;
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
    crunchOne(path, f, file->name, 0);
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
    int minPos = -1;
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
	minPos = crunchOne(path, f, dir->name, minPos+1);
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
    crunchOne(input, f, clSingle, 0);
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
clFixOverlap = optionExists("fixOverlap");
clOneToThree = optionExists("oneToThree");
wigAsciiCrunch(argv[1], argv[2]);
return 0;
}
