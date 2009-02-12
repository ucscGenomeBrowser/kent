/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "bed.h"
#include "jksql.h"
#include "binRange.h"
#include "hdb.h"


static char const rcsid[] = "$Id: freen.c,v 1.86 2009/02/12 00:20:38 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file\n");
}

void writeChars(int f, char c, int count)
/* Write a char to a low level file repeatedly followed by a new line. */
{
int i;
for (i=0; i<count; ++i)
    write(f, &c, 1);
c = '\n';
write(f, &c, 1);
}

void freen(char *fileName)
/* Test some hair-brained thing. */
{
int f = open(fileName, O_RDWR);
if (f <= 0)
    errAbort("Coulen't open %s", fileName);
// lseek(f, 0, SEEK_SET);
writeChars(f, '1', 49);
getchar();
// lseek(f, 50, SEEK_SET);
writeChars(f, '2', 49);
getchar();
// lseek(f, 100, SEEK_SET);
writeChars(f, '3', 49);
getchar();
// lseek(f, 150, SEEK_SET);
writeChars(f, '4', 49);
getchar();
close(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
