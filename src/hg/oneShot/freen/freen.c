/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fa.h"
#include "jksql.h"
#include "mysqlTableStatus.h"

static char const rcsid[] = "$Id: freen.c,v 1.47 2004/07/20 20:50:20 kent Exp $";

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen something");
}

int countSameNonDigit(char *a, char *b)
/* Return count of characters in a,b that are the same
 * up until first digit in either one. */
{
char cA, cB;
int same = 0;
for (;;)
   {
   cA = *a++;
   cB = *b++;
   if (cA != cB)
       break;
   if (isdigit(cA) || isdigit(cB))
       break;
   ++same;
   }
return same;
}

boolean allDigits(char *s)
/* Return TRUE if s is all digits */
{
char c;
while ((c = *s++) != 0)
    if (!isdigit(c))
        return FALSE;
return TRUE;
}

int cmpChrom(char *a, char *b)
/* Compare two chromosomes. */
{
int cSame = countSameNonDigit(a, b);
int diff;

a += cSame;
b += cSame;
uglyf("cSame %d, a %s, b %s\n", cSame, a, b);
if (allDigits(a) && allDigits(b))
    return atoi(a) - atoi(b);
else
    return strcmp(a,b);
}

void freen(char *a, char *b)
/* Test some hair-brained thing. */
{
int diff = cmpChrom(a,b);
uglyf( "diff = % d \n", diff);
}


int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(1000000);
if (argc != 3)
   usage();
freen(argv[1], argv[2]);
return 0;
}
