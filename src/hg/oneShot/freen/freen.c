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

static char const rcsid[] = "$Id: freen.c,v 1.72 2006/08/10 01:04:05 kent Exp $";

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
struct slName *browserLines = NULL;
struct customTrack *trackList = customFactoryParse(inFile, TRUE, &browserLines);
struct customTrack *track;
customTrackSave(trackList, outFile);
for (track = trackList; track != NULL; track = track->next)
    {
    printf("track %s %s %s\n", track->tdb->shortLabel, track->tdb->tableName,
    	track->tdb->type);
    }
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
