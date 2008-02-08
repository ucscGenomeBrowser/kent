/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "bed.h"
#include "jksql.h"
#include "spDb.h"


static char const rcsid[] = "$Id: freen.c,v 1.82 2008/02/08 23:27:18 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file \n");
}

void freen(char *fileName)
/* Test some hair-brained thing. */
{
char *acc = fileName;
struct sqlConnection *conn = sqlConnect("sp080205");
struct slName *el, *list = spProteinEvidence(conn, acc);
for (el = list; el != NULL; el = el->next)
    printf("%s %s\n", acc, el->name);
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
