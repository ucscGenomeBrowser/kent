/* opendirTest - test opendir() function timing. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include <dirent.h>

/*	$Id: opendirTest.c,v 1.1 2006/06/23 18:55:55 hiram Exp $	*/

void usage()
/* Explain usage and exit. */
{
errAbort(
  "opendirTest - test opendir() function timing\n"
  "usage:\n"
  "   opendirTest [options] <dirList.txt>\n"
  "options:\n"
  "   <dirList.txt> - file name for listing of directories to use for opendir() test\n"
  "   -stat - use the stat() function instead of the opendir\n"
  "   -fail - make them all fail by changing a character in their name\n"
  "   -verbose=2 - display arguments\n"
  "   -verbose=3 - show failed tests\n"
  );
}

static boolean optStat = FALSE;
static boolean optFail = FALSE;

static struct optionSpec options[] = {
   {"stat", OPTION_BOOLEAN},
   {"fail", OPTION_BOOLEAN},
   {NULL, 0},
};

void opendirTest(char *dirList)
/* opendirTest - test opendir() function timing. */
{
struct lineFile *lf = lineFileOpen(dirList,TRUE);
char *line;
unsigned success = 0;
unsigned tested = 0;
long clockStart = clock1000();
long clockEnd = 0;

if (optStat)
    {
    struct stat buf;
    while (lineFileNextReal(lf,&line))
	{
	int statResult;
	int len = strlen(line);
	int halfLen = len >> 1;
	if (optFail) { line[halfLen-1] = 0233; }
	statResult = stat(line, &buf);
	++tested;
	if (0 == statResult)
	    {
	    ++success;
	    verbose(4,"#\tOK stat(\"%s\")\n", line);
	    }
	else
	    {
	    verbose(3,"#\tfailed stat(\"%s\")\n", line);
	    }
	}
    }
else
    {
    while (lineFileNextReal(lf,&line))
	{
	DIR *dirOpen;
	int len = strlen(line);
	int halfLen = len >> 1;
	if (optFail) { line[halfLen-1] = 0233; }
	dirOpen = opendir(line);
	++tested;
	if ((DIR *)NULL != dirOpen)
	    {
	    ++success;
	    closedir(dirOpen);
	    verbose(4,"#\tOK opendir(\"%s\")\n", line);
	    }
	else
	    {
	    verbose(3,"#\tfailed opendir(\"%s\")\n", line);
	    }
	}
    }
clockEnd = clock1000();
verbose(1,"#\t%s %u directories, %ld millis == %.2f millis/diropen\n",
    optStat ? "stat()" : "opendir()", tested, clockEnd-clockStart,
	(double)(clockEnd-clockStart)/(double)tested);
verbose(1,"#\tsuccess: %u, failed: %u\n", success, tested-success);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
optStat = optionExists("stat");
optFail = optionExists("fail");
if (argc != 2)
    usage();
verbose(2,"#\tfunction tested: %s\n", optStat ? "stat()" : "opendir()");
verbose(2,"#\t  make all fail: %s\n", optFail ? "TRUE" : "FALSE");
opendirTest(argv[1]);
return 0;
}
