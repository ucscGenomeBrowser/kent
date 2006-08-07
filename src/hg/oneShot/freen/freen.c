/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "customPp.h"
#include "customTrack.h"
#include "customFactory.h"

static char const rcsid[] = "$Id: freen.c,v 1.71 2006/08/07 15:47:45 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file \n");
}

int level = 0;


void freen(char *inFile)
/* Test some hair-brained thing. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    int len =strlen(line);
    if (len > 80)
	lineFileErrAbort(lf, "line too long (%d chars):\n%s", len, line);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(1000000*1024L);
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
