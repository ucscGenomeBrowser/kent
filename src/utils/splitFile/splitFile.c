/* splitFile - Split up a file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "splitFile - Split up a file\n"
  "usage:\n"
  "   splitFile source linesPerFile outBaseName\n"
  "options:\n"
  "   -head=file - put head in front of each output\n"
  "   -tail=file - put tail at end of each output");
}

void appendFile(char *fileName, FILE *f)
/* Append file to f. */
{
struct lineFile *lf;
int lineSize;
char *line;
if (fileName != NULL)
    {
    lf = lineFileOpen(fileName, FALSE);
    while (lineFileNext(lf, &line, &lineSize))
	mustWrite(f, line, lineSize);
    lineFileClose(&lf);
    }
}

void splitFile(char *source, int linesPerFile, char *outBaseName,
	char *head, char *tail)
/* splitFile - Split up a file. */
{
char outName[512];
int fileIx, i;
struct lineFile *lf = lineFileOpen(source, FALSE);
FILE *f = NULL;
int lineSize;
char *line;
boolean done = FALSE;

for (fileIx=1; ; ++fileIx)
    {
    sprintf(outName, "%s%02d", outBaseName, fileIx);
    f = mustOpen(outName, "w");
    appendFile(head, f);
    for (i=0; i<linesPerFile; ++i)
        {
	if (!lineFileNext(lf, &line, &lineSize))
	    {
	    done = TRUE;
	    break;
	    }
	mustWrite(f, line, lineSize);
	}
    appendFile(tail, f);
    printf("wrote %d lines to %s\n", i, outName);
    carefulClose(&f);
    if (done)
        break;
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
if (!isdigit(argv[2][0]))
    usage();
splitFile(argv[1], atoi(argv[2]), argv[3], cgiOptionalString("head"), cgiOptionalString("tail"));
return 0;
}
