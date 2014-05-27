/* combineFiles - combine small files into files of a certain size. */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "sys/types.h"
#include "sys/stat.h"
#include "dirent.h"
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "combineFiles - combine small files into files of a certain size\n"
  "usage:\n"
  "   combineFiles dir maxSize\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct file
{
    struct file *next;
    char *name;
    int  size;
};

int sizeCmp(const void *va, const void *vb)
{
const struct file *a = *((struct file **)va);
const struct file *b = *((struct file **)vb);

return b->size - a->size;
}

void append(char *dest, char *src)
{
FILE *f1 = mustOpen(dest, "a");
FILE *f2 = mustOpen(src, "r");
char buffer[BUFSIZ];
int num;

while (num = fread(buffer, 1, sizeof(buffer), f2))
    fwrite(buffer, 1, num, f1);

fclose(f1);
fclose(f2);
}

void combineFiles(char *directory, int size)
/* combineFiles - combine small files into files of a certain size. */
{
DIR *dir;
struct file *file = NULL;
struct file *fileList = NULL;
struct dirent *dirent;

chdir(directory);
dir = opendir(".");
while ((dirent = readdir(dir)) != NULL)
    {
    struct stat statBuf;
    stat(dirent->d_name, &statBuf);

    if (S_ISREG(statBuf.st_mode))
	{
	AllocVar(file);
	file->name = cloneString(dirent->d_name);
	file->size = statBuf.st_size;

	slAddHead(&fileList, file);
	}
    }

slSort(&fileList, sizeCmp);
for(file=fileList; file; file = file->next)
    {
    if (file->size <= size)
	{
	struct file *file2;
	struct file *prev = file;

	for(file2=file->next; file2; file2 = file2->next)
	    {
	    if (file->size + file2->size <= size)
		{
		append(file->name, file2->name);
		unlink(file2->name);
		file->size += file2->size;
		file->next = file2->next;
		}
	    else
		break;
	    }
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
int size;

optionInit(&argc, argv, options);
if (argc != 3)
    usage();
size = atoi(argv[2]);
if ((size < 0) || (size > 1024*1024*1024))
    errAbort("size must be between 0 and %d\n",1024*1024*1024);
combineFiles(argv[1], size);
return 0;
}
