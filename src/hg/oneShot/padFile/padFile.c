/* padFile - Add a bunch of zeroes to end of file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

static char const rcsid[] = "$Id: padFile.c,v 1.3 2003/05/06 07:22:30 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "padFile - Add a bunch of zeroes to end of file\n"
  "usage:\n"
  "   padFile file count\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void padFile(char *file, int count)
/* padFile - Add a bunch of zeroes to end of file. */
{
int i;
FILE *f = mustOpen(file, "a");
for (i=0; i<count; ++i)
    fputc(0, f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
padFile(argv[1], atoi(argv[2]));
return 0;
}
