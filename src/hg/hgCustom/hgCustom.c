/* hgCustom - CGI-script to launch browser with a custom user track. 
 *
 * This loads files in either a bed or GFF format.  The BED format
 * files may include special 'track' lines. */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "linefile.h"
#include "genoFind.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "bed.h"
#include "browserTable.h"
#include "sqlList.h"
#include "customTrack.h"
#include "ctgPos.h"
#include "hdb.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgCustom - CGI-script to launch browser with a custom user track\n");
}

void doMiddle()
{
char *customText;
struct customTrack *trackList, *track;

#ifdef SOON
/* Grab custom track from text box or from file. */
customText = cgiString("customText");
if (customText[0] == 0)
    customText = cgiString("customFile");
trackList = customTracksFromText(customText);
#endif /* SOON */

trackList = customTracksFromFile("test.foo");
if (customTrackNeedsLift(trackList))
    {
    struct hash *ctgHash;
    uglyf("Needs lift\n");
    ctgHash = hCtgPosHash();
    customTrackLift(trackList, ctgHash);
    customTrackSave(trackList, "test.lift");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
customTrackTest();
dnaUtilOpen();
htmlSetBackground("../images/floret.jpg");
htmShell("BLAT Search", doMiddle, NULL);
return 0;
}
