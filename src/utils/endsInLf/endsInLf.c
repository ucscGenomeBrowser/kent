/* endsInLf - Check that last letter in files is end of line. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"


boolean zeroOk = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "endsInLf - Check that last letter in files is end of line\n"
  "usage:\n"
  "   endsInLf file(s)\n"
  "options:\n"
  "   -zeroOk\n"
  );
}

int endsInLf(int fileCount, char *fileNames[])
/* endsInLf - Check that last letter in files is end of line. */
{
int retStatus = 0;
char *fileName;
FILE *f;
int i;
char c;
int size;

for (i=0; i<fileCount; ++i)
    {
    fileName = fileNames[i];
    f = mustOpen(fileName, "r");
    size = fseek(f, -1, SEEK_END);
    if (size < 0)
        {
	if (!zeroOk)
	    {
	    retStatus = -1;
	    warn("%s zero length", fileName);
	    }
	}
    else
	{
	c = fgetc(f);
	carefulClose(&f);
	if (c != '\n')
	    {
	    retStatus = -1;
	    warn("%s incomplete last line", fileName);
	    }
	}
    }
return retStatus;
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
zeroOk = cgiBoolean("zeroOk");
if (argc < 2)
    usage();
return endsInLf(argc-1, argv+1);
}
