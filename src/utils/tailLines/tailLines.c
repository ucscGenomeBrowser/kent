/* tailLines - add tail to each line of file. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "tailLines - add tail to each line of file\n"
  "usage:\n"
  "   tailLines file tail\n"
  "This will add tail to each line of file and print to stdout.");
}

void tailLines(char *fileName, char *tail)
/* tailLines - add tail to each line of file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int lineSize;
char *line;
while (lineFileNext(lf, &line, &lineSize))
    printf("%s%s\n", line, tail);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
tailLines(argv[1], argv[2]);
return 0;
}
