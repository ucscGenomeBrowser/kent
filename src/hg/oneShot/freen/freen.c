/* freen - My Pet Freen. */
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen file/dir");
}

time_t now;

int rFreen(int level, char *dirName, int dirNameSize, char *fileName)
/* Recursively traverse directory tree. */
{
int err;
static struct stat st;
int newSize = strlen(fileName) + 1 + dirNameSize;

if (level > 0 && (sameString(fileName, ".") || sameString(fileName, "..")))
    return 0;
if (newSize > PATH_MAX)
    return -1;
if (dirName[0] == 0)
    {
    --newSize;
    strcpy(dirName, fileName);
    }
else
    {
    dirName[dirNameSize] = '/';
    strcpy(dirName + dirNameSize + 1, fileName);
    }
if ((err = stat(dirName, &st)) >= 0)
    {
    spaceOut(stdout, level);
    printf("%s", fileName);
    if (S_ISDIR(st.st_mode))
	printf("/");
    printf("\t%lld", st.st_size);
    printf("\t%ld\n", now - st.st_mtime);
    if (S_ISDIR(st.st_mode))
	{
	if (newSize <= PATH_MAX)
	    {
	    DIR *d;
	    if ((d = opendir(dirName)) != NULL)
		{
		struct dirent *de;
		while ((de = readdir(d)) != NULL)
		    {
		    rFreen(level+1, dirName, newSize, de->d_name);
		    }
		closedir(d);
		}
	    }
	}
    }
dirName[dirNameSize] = 0;
return err;
}

void freen(char *fileName)
/* Print borf as fasta. */
{
char buf[PATH_MAX+1];
buf[0] = 0;
now = time(NULL);
printf("%ld %s", now, ctime(&now));
rFreen(0, buf, 0, fileName);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
