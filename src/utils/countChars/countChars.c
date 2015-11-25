/* countChars - Count the number of occurences of a particular char. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "countChars - Count the number of occurrences of a particular char\n"
  "usage:\n"
  "   countChars char file(s)\n"
  "Char can either be a two digit hexadecimal value or\n"
  "a single letter literal character");
}

int unhex(char *s)
/* Return hex val of s. */
{
int acc = 0;
char c;

tolowers(s);
while ((c = *s++) != 0)
    {
    acc <<= 4;
    if (c >= '0' && c <= '9')
       acc += c - '0';
    else if (c >= 'a' && c <= 'f')
       acc += c + 10 - 'a';
    else
       errAbort("Expecting hexadecimal character got %s", s);
    }
return acc;
}


int countCharsInFile(char c, char *fileName)
/* Count up characters in file matching c. */
{
FILE *f = mustOpen(fileName, "r");
int i;
int count = 0;

while ((i = fgetc(f)) >= 0)
    {
    if (i == c)
        ++count;
    }
fclose(f);
return count;
}

void doIt(char *ch, int fileCount, char *fileNames[])
/* doIt - Count the number of occurences of a particular char. */
{
char c;
int i;
char *fileName;

if (strlen(ch) > 1)
   c = unhex(ch);
else
   c = ch[0];
for (i=0; i<fileCount; ++i)
    {
    int count;
    fileName = fileNames[i];
    count = countCharsInFile(c, fileName);
    if (count > 0)
       printf("%s %d\n", fileName, count);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 3)
    usage();
doIt(argv[1], argc-2, argv+2);
return 0;
}
