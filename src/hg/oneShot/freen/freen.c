/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "customPp.h"

static char const rcsid[] = "$Id: freen.c,v 1.69 2006/07/26 06:10:21 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file \n");
}

int level = 0;


void freen(char *fileName)
/* Test some hair-brained thing. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct customPp *cpp = customPpNew(lf);
char *line;
struct slName *list, *el;
while ((line = customPpNext(cpp)) != NULL)
    puts(line);
printf("---- browser lines ----\n");
list = customPpTakeBrowserLines(cpp);
customPpFree(&cpp);
for (el = list; el != NULL; el = el->next)
    puts(el->name);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
