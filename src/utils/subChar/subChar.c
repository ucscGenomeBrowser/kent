/* subChar - Substitute one character for another throughout a file.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "obscure.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "subChar - Substitute one character for another throughout a file.\n"
  "usage:\n"
  "   subChar oldChar newChar file(s)\n"
  "oldChar and newChar can either be single letter literal characters,\n"
  "or two digit hexadecimal ascii codes");
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

char charVal(char *s)
/* Return hexadecimal or literal decoding of string into char. */
{
int len = strlen(s);
if (len == 1)
   return s[0];
else if (len == 2)
   return unhex(s);
else
   {
   usage();
   return 0;
   }
}

void subCharsInFile(char old, char new, char *fileName)
/* Substitute new for old throughout file. */
{
int subCount = 0;
size_t fileSize;
int i;
char *buf;

readInGulp(fileName, &buf, &fileSize);
for (i=0; i<fileSize; ++i)
    {
    if (buf[i] == old)
       {
       buf[i] = new;
       ++subCount;
       }
    }
if (subCount > 0)
    {
    FILE *f = mustOpen(fileName, "w");
    printf("Substituting %x for %x %d times in %s\n",
    	new, old, subCount, fileName);
    mustWrite(f, buf, fileSize);
    fclose(f);
    }
freez(&buf);
}

void subChars(char *oldChar, char *newChar, int fileCount, char *fileNames[])
/* subChar - Substitute one character for another throughout a file.. */
{
char old = charVal(oldChar), new = charVal(newChar);
int i;
char *fileName;

for (i=0; i<fileCount; ++i)
    {
    fileName = fileNames[i];
    subCharsInFile(old, new, fileName);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 4)
    usage();
subChars(argv[1], argv[2], argc-3, argv+3);
return 0;
}
