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

static char const rcsid[] = "$Id: freen.c,v 1.70 2006/08/03 18:06:03 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file \n");
}

int level = 0;


void freen(char *inFile, char *outFile)
/* Test some hair-brained thing. */
{
hSetDb("hg18");
struct slName *bLines = NULL, *b;
struct customTrack *track, *trackList = customFactoryParse(inFile, TRUE, &bLines);
carefulCheckHeap();
customTrackSave(trackList, outFile);
carefulCheckHeap();
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(1000000*1024L);
if (argc != 3)
   usage();
freen(argv[1], argv[2]);
return 0;
}
