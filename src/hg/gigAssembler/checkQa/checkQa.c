/* checkQa - make sure that .fa and .qa files are in correspondence. */

#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "qaSeq.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
 "checkQa - make sure that .fa and .qa files are in correspondence.\n"
 "usage:\n"
 "    checkQa dirs qaDir faDir\n"
 "This checks that all the .fa files in faDir have corresponding\n"
 ".qa files in qaDir.\n"
 "    checkQa files qaFile faFile\n"
 "This checks that qaFile and faFile go together\n"
 "    checkQa top dir1 dir2 ... dirN\n"
 "This checks that all the files in dir1/fa and dir1/qa go together.\n"
 "Likewise for dir2, dir3, etc.");
}

void qaFiles(char *qaName, char *faName)
/* Check that qaName and faName agree. */
{
struct qaSeq *qaList = qaReadBoth(qaName, faName);
qaSeqFreeList(&qaList);
}

void qaDirs(char *qaDir, char *faDir)
/* check that all the files in faDir have quality
 * files in faDir. */
{
char qaName[512];
char faName[512];
char cloneName[512];
int ix = 0;
struct slName *el, *list;
struct qaSeq *qaList;

printf("Comparing %s and %s\n", qaDir, faDir);
list = listDir(faDir, "*.fa");
for (el = list; el != NULL; el = el->next)
    {
    strcpy(cloneName, el->name);
    chopSuffix(cloneName);
    sprintf(qaName, "%s/%s.qa", qaDir, cloneName);
    sprintf(faName, "%s/%s.fa", faDir, cloneName);
    if (!fileExists(qaName))
        warn("%s doesn't exist", qaName);
    qaList = qaReadBoth(qaName, faName);
    qaSeqFreeList(&qaList);
    if ((++ix & 0x3f) == 0)
          {
	  printf(".");
	  fflush(stdout);
	  }
    }
printf("\n");
}

void qaFromTop(int dirCount, char *dirs[])
/* Check fa and qa subdirectories against each other. */
{
int i;
char faDir[512], qaDir[512];
for (i=0; i<dirCount; ++i)
     {
     sprintf(qaDir, "%s/qa", dirs[i]);
     sprintf(faDir, "%s/fa", dirs[i]);
     qaDirs(qaDir, faDir);
     }
}

int main(int argc, char *argv[])
/* Process command line . */
{
char *command;

if (argc < 3)
    usage();
command = argv[1];
if (sameWord(command, "dirs"))
    qaDirs(argv[2], argv[3]);
else if (sameWord(command, "files"))
    qaFiles(argv[2], argv[3]);
else if (sameWord(command, "top"))
    qaFromTop(argc-2, argv+2);
else
    usage();
return 0;
}
