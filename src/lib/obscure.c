/* Obscure stuff that is handy every now and again. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "portable.h"
#include "hash.h"
#include "obscure.h"
#include "linefile.h"

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
size_t size = (size_t)fileSize(fileName);
char *buf;
FILE *f = mustOpen(fileName, "rb");
*retBuf = buf = needLargeMem(size+1);
mustRead(f, buf, size);
buf[size] = 0;      /* Just in case it needs zero termination. */
fclose(f);
if (retSize != NULL)
    *retSize = size;
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

struct slName *readAllLines(char *fileName)
/* Read all lines of file into a list.  (Removes trailing carriage return.) */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct slName *list = NULL, *el;
char *line;

while (lineFileNext(lf, &line, NULL))
     {
     el = newSlName(line);
     slAddHead(&list, el);
     }
slReverse(&list);
return list;
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

void cpFile(int s, int d)
/* Copy from source file to dest until reach end of file. */
{
int bufSize = 64*1024, readSize;
char *buf = needMem(bufSize);

for (;;)
    {
    readSize = read(s, buf, bufSize);
    if (readSize > 0)
        write(d, buf, readSize);
    if (readSize <= 0)
        break;
    }
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

struct hash *hashVarLine(char *line, int lineIx)
/* Return a symbol table from a line of form:
 *   var1=val1 var2='quoted val2' var3="another val" */
{
char *s = line, c;
char *var, *val;
struct hash *hash = newHash(8);

for (;;)
    {
    if ((var = skipLeadingSpaces(s)) == NULL)
        break;
    if ((c = *var) == 0)
        break;
    if (!isalpha(c))
	errAbort("line %d of custom input: variable needs to start with letter", lineIx);
    val = strchr(var, '=');
    if (val == NULL)
        errAbort("line %d of custom input: missing = in var/val pair", lineIx);
    *val++ = 0;
    c = *val;
    if (c == '\'' || c == '"')
        {
	if (!parseQuotedString(val, val, &s))
	    errAbort("line %d of custom input: missing closing %c", lineIx, c);
	}
    else
	{
	s = skipToSpaces(val);
	if (s != NULL) *s++ = 0;
	}
    hashAdd(hash, var, cloneString(val));
    }
return hash;
}

