/* splitFile - Split a file into pieces. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "obscure.h"

char *suffix = "";
int startNum=1;
int digits = 2;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "splitFile - Split a file into pieces\n"
  "usage:\n"
  "   splitFile inFile outRoot count\n"
  "Splits files into pieces of count lines each\n"
  "options:\n"
  "   -head=fileName   Prepend header taken from file to each output file\n"
  "   -tail=fileName   Add tail to each output file\n"
  "   -suffix=.xxx     Suffix to add to each output file name\n"
  "   -startNum=NNN    Starting number for output file names\n"
  "   -digits=N        Number of digits in number part of output file name\n"
  );
}

void writeLines(FILE *f, struct slName *list)
/* Write list to file. */
{
struct slName *el;
for (el = list; el != NULL; el = el->next)
    fprintf(f, "%s\n", el->name);
}

void splitFile(char *inFile, char *outRoot, int count)
/* splitFile - Split a file into pieces. */
{
struct lineFile *lf = lineFileOpen(inFile, FALSE);
char *line;
int lineSize, fileIx = startNum;
char outName[512];
int i;
FILE *f = NULL;
struct slName *headList = NULL, *tailList = NULL;
char *name;
boolean going = TRUE;

if (count <= 0)
    usage();
if ((name = cgiOptionalString("head")) != NULL)
    headList = readAllLines(name);
if ((name = cgiOptionalString("tail")) != NULL)
    tailList = readAllLines(name);
while (going)
    {
    for (i=0; i<count; ++i)
        {
	if (!lineFileNext(lf, &line, &lineSize))
	    {
	    going = FALSE;
	    break;
	    }
	if (f == NULL)
	    {
	    sprintf(outName, "%s%0*d%s", outRoot, digits, fileIx++, suffix);
	    printf("Writing %s\n", outName);
	    f = mustOpen(outName, "w");
	    writeLines(f, headList);
	    }
	mustWrite(f, line, lineSize);
	}
    if (f != NULL)
	writeLines(f, tailList);        
    carefulClose(&f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
suffix = cgiUsualString("suffix", suffix);
startNum = cgiUsualInt("startNum", startNum);
digits = cgiUsualInt("digits", digits);
if (argc != 4)
    usage();
splitFile(argv[1], argv[2], atoi(argv[3]));
return 0;
}
