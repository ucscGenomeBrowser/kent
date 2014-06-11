/* catDir - concatenate files in directory - for those times when too 
 * many files for cat to handle.. */
#include "common.h"
#include "portable.h"
#include "cheapcgi.h"
#include "errAbort.h"
#include "obscure.h"


boolean recurse = FALSE;
char *wildCard = NULL;
char *suffix = NULL;
boolean nonz = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "catDir - concatenate files in directory to stdout.\n"
  "For those times when too many files for cat to handle.\n"
  "usage:\n"
  "   catDir dir(s)\n"
  "options:\n"
  "   -r            Recurse into subdirectories\n"
  "   -suffix=.suf  This will restrict things to files ending in .suf\n"
  "   '-wild=*.\?\?\?' This will match wildcards.\n"
  "   -nonz         Prints file name of non-zero length files\n");
}

void catFile(char *fileName)
/* Write contents of file to stdout. */
{
int fd;

fd = open(fileName, O_RDONLY);
if (fd == -1)
    errnoAbort("Couldn't open %s", fileName);
if (nonz)
    {
    char c;
    if (read(fd, &c, 1) > 0)
	{
        printf("%s\n%c", fileName, c);
	fflush(stdout);
	}
    }
cpFile(fd, fileno(stdout));
close(fd);
}

void catDir(int dirCount, char *dirs[])
/* catDir - concatenate files in directory - for those times when too 
 * many files for cat to handle.. */
{
int i;
struct fileInfo *list, *el;

for (i=0; i<dirCount; ++i)
    {
    list = listDirX(dirs[i], NULL, TRUE);
    for (el = list; el != NULL; el = el->next)
        {
	char *name = el->name;
	if (el->isDir && recurse)
	    {
	    catDir(1, &name);
	    }
	else if (wildCard == NULL || wildMatch(wildCard, name))
	    {
	    if (suffix == NULL || endsWith(name, suffix))
		catFile(name);
	    }
	}
    slFreeList(&list);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc < 2)
    usage();
recurse = cgiBoolean("r");
suffix = cgiOptionalString("suffix");
wildCard = cgiOptionalString("wild");
nonz = cgiBoolean("nonz");
catDir(argc-1, argv+1);
return 0;
}
