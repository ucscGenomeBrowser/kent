/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
#include "common.h"
#include "portable.h"
#include "obscure.h"

long incCounterFile(char *fileName)
/* Increment a 32 bit value on disk. */
{
long val = 0;
FILE *f = fopen(fileName, "r+b");
if (f != NULL)
    {
    fread(&val, sizeof(val), 1, f);
    rewind(f);
    }
else
    {
    f = fopen(fileName, "wb");
    }
++val;
if (f != NULL)
    {
    fwrite(&val, sizeof(val), 1, f);
    fclose(f);
    }
return val;
}

int digitsBaseTwo(unsigned long x)
/* Return base two # of digits. */
{
int digits = 0;
while (x)
    {
    digits += 1;
    x >>= 1;
    }
return digits;
}

int digitsBaseTen(int x)
/* Return number of digits base 10. */
{
int digCount = 1;
if (x < 0)
    {
    digCount = 2;
    x = -x;
    }
while (x >= 10)
    {
    digCount += 1;
    x /= 10;
    }
return digCount;
}

void readInGulp(char *fileName, char **retBuf, size_t *retSize)
/* Read whole file in one big gulp. */
{
size_t size = *retSize = (size_t)fileSize(fileName);
char *buf;
FILE *f = mustOpen(fileName, "rb");
*retBuf = buf = needLargeMem(size+1);
mustRead(f, buf, size);
buf[size] = 0;      /* Just in case it needs zero termination. */
fclose(f);
}

void readAllWords(char *fileName, char ***retWords, int *retWordCount, char **retBuf)
/* Read in whole file and break it into words. You need to freeMem both
 * *retWordCount and *retBuf when done. */
{
int wordCount;
char *buf = NULL;
char **words = NULL;
size_t bufSize;

readInGulp(fileName, &buf, &bufSize);
wordCount = chopByWhite(buf, NULL, 0);
if (wordCount != 0)
    {
    words = needMem(wordCount * sizeof(words[0]));
    chopByWhite(buf, words, wordCount);
    }
*retWords = words;
*retWordCount = wordCount;
*retBuf = buf;
}

void copyFile(char *source, char *dest)
/* Copy file from source to dest. */
{
int bufSize = 64*1024;
char *buf = needMem(bufSize);
int bytesRead;
int s, d;

s = open(source, O_RDONLY);
if (s < 0)
    errAbort("Couldn't open %s. %s\n", source, strerror(errno));
d = creat(dest, 0777);
if (d < 0)
    {
    close(s);
    errAbort("Couldn't open %s. %s\n", dest, strerror(errno));
    }
while ((bytesRead = read(s, buf, bufSize)) > 0)
    {
    if (write(d, buf, bytesRead) < 0)
        errAbort("Write error on %s. %s\n", dest, strerror(errno));
    }
close(s);
close(d);
freeMem(buf);
}


void *intToPt(int i)
/* Convert integer to pointer. Use when really want to store an
 * int in a pointer field. */
{
char *pt = NULL;
return pt+i;
}

int ptToInt(void *pt)
/* Convert pointer to integer.  Use when really want to store an
 * int in a pointer field. */
{
char *a = NULL, *b = pt;
return b - a;
}

boolean parseQuotedString( char *in, char *out, char **retNext)
/* Read quoted string from in (which should begin with first quote).
 * Write unquoted string to out, which may be the same as in.
 * Return pointer to character past end of string in *retNext. 
 * Return FALSE if can't find end. */
{
char c, *s = in;
int quoteChar = *s++;
boolean escaped = FALSE;

for (;;)
   {
   c = *s++;
   if (c == 0)
       {
       warn("Unmatched %c", quoteChar);
       return FALSE;
       }
   if (escaped)
       {
       if (c == '\\' || c == quoteChar)
          *out++ = c;
       else
          {
	  *out++ = '\\';
	  *out++ = c;
	  }
       escaped = FALSE;
       }
   else
       {
       if (c == '\\')
           escaped = TRUE;
       else if (c == quoteChar)
           break;
       else
           *out++ = c;
       }
   }
*out = 0;
*retNext = s;
return TRUE;
}

