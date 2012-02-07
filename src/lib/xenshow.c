/* xenshow - show a cross-species alignment. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "nt4.h"
#include "xenalign.h"


static void printMatchers(char *a, char *b, int lineSize, FILE *f)
/* Print '|' where a[i] and b[i] match, ' ' where they don't */
{
int i;
for (i=0; i<lineSize; ++i)
   {
   char c = ((a[i] == b[i]) ? '|' : ' ');
   fputc(c, f);
   }
}

static int nonDashCount(char *s, int size)
/* Return number of characters in s[0] to s[size-1] that
 * aren't dashes. */
{
int count = 0;
int i;
for (i=0; i<size; ++i)
    if (s[i] != '-')
        ++count;
return count;
}

void xenShowAli(char *qSym, char *tSym, char *hSym, int symCount, FILE *f,
   int qOffset, int tOffset, char qStrand, char tStrand, int maxLineSize)
/* Print alignment and HMM symbols maxLineSize bases at a time to file. */
{
int i;
int lineSize;
int count;

for (i=0; i<symCount; i += lineSize)
    {
    lineSize = symCount - i;
    if (lineSize > maxLineSize) lineSize = maxLineSize;
    mustWrite(f, qSym+i, lineSize);
    count = nonDashCount(qSym+i, lineSize);
    if (qStrand == '-')
        count = -count;
    qOffset += count;
    fprintf(f, " %9d\n", qOffset);
    printMatchers(qSym+i, tSym+i, lineSize, f);
    fputc('\n', f);
    mustWrite(f, tSym+i, lineSize);
    count = nonDashCount(tSym+i, lineSize);
    if (tStrand == '-')
        count = -count;
    tOffset += count;
    fprintf(f, " %9d\n", tOffset);
    mustWrite(f, hSym+i, lineSize);
    fputc('\n', f);
    fputc('\n', f);
    }
}

