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

/* Grab custom track from text box or from file. */
customText = cgiString("customText");
if (customText[0] == 0)
    customText = cgiString("customFile");
trackList = customTracksFromText(customText);
}

int main(int argc, char *argv[])
/* Process command line. */
{
customTrackTest();
#ifdef SOON
trackList = parseCustomTracks(customText);
cgiSpoof(&argc, argv);
dnaUtilOpen();
htmlSetBackground("../images/floret.jpg");
htmShell("BLAT Search", doMiddle, NULL);
#endif /* SOON */
return 0;
}
