/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

void rPrintTdb(struct trackDb *tdbList)
{
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    printf("%s\t%s\n", tdb->track, tdb->shortLabel);
    rPrintTdb(tdb->subtracks);
    }
}

void freen(char *input)
/* Test some hair-brained thing. */
{
struct trackDb *tdbList = hTrackDb(input, NULL);
rPrintTdb(tdbList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
// optionInit(&argc, argv, options);

if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
