/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "trackDb.h"
#include "hdb.h"
#include "hui.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

void freen(char *track)
/* Test some hair-brained thing. */
{
// struct trackDb *tdbList = hTrackDb("hg19");
struct trackDb *tdbList = NULL;
struct hash *hash = trackHashMakeWithComposites("hg19","chr1",&tdbList,FALSE);
struct trackDb *tdb, *wgEncodeReg = NULL, *wgEncodeRegTxn = NULL;
uglyf("Got %d items in tdbList\n", slCount(tdbList));
uglyf("Got %d items in hash\n", hash->elCount);
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    if (startsWith("wgEncodeReg", tdb->track))
	{
        printf("%s parent %p\n", tdb->track, tdb->parent);
	if (sameString("wgEncodeRegTxn", tdb->track))
	    {
	    uglyf("Got you %s!  parent is %p with %d kids\n", tdb->track, tdb->parent,
	    	slCount(tdb->parent->subtracks));
	    wgEncodeRegTxn = tdb;
	    wgEncodeReg = tdb->parent;
	    }
	}
    }

uglyf("wgEncodeReg has %d kids\n", slCount(wgEncodeReg->subtracks));
struct trackDb *parentTdb = tdbForTrack("hg19", "wgEncodeReg", &tdbList);
uglyf("parentTdb has %d kids\n", slCount(parentTdb->subtracks));
uglyf("wgEncodeReg = %p, parentTdb=%p\n", wgEncodeReg, parentTdb);
char *super = trackDbGetSupertrackName(wgEncodeRegTxn);
uglyf("super = %s\n", super);
tdbMarkAsSuperTrack(wgEncodeReg);
trackDbSuperMemberSettings(wgEncodeRegTxn);
uglyf("after more stuff wgEncodeReg has %d kids\n", slCount(wgEncodeReg->subtracks));
hTrackDbLoadSuper("hg19", wgEncodeReg);
uglyf("after hTrackDbLoadSuper, wgEncodeReg has %d kids\n", slCount(wgEncodeReg->subtracks));
#ifdef SOON
if (super != NULL)
    {
    uglyf("And for super %s\n", super->track);
    for (tdb = super->subtracks; tdb != NULL; tdb = tdb->next)
	 uglyf("child %s\n", tdb->track);
    }
else
    uglyf("super = NULL\n");
#endif /* SOON */
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
