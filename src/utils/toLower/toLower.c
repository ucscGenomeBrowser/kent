/* toLower - Convert upper case to lower case in file. Leave other chars alone. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "toLower - Convert upper case to lower case in file. Leave other chars alone\n"
  "usage:\n"
  "   toLower inFile outFile\n"
  "equivalent to the unix commands: cat inFile | tr '[A-Z]' '[a-z]' > outFile");
}

void toLower(char *inName, char *outName)
/* toLower - Convert upper case to lower case in file. Leave other chars alone. */
{
struct lineFile *lf = lineFileOpen(inName, FALSE);
FILE *f = mustOpen(outName, "w");
int lineSize;
char *line;

while (lineFileNext(lf, &line, &lineSize))
    {
    toLowerN(line, lineSize);
    mustWrite(f, line, lineSize);
    }
fclose(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
toLower(argv[1], argv[2]);
return 0;
}
