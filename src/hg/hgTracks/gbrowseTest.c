/* Test harness: dump a gif of ce4.gc5Base, chrI:4001-5000. */

#include "common.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hvGfx.h"
#include "hgTracks.h"
#include "oneTrack.h"


int main(int argc, char **argv)
/* Test harness -- dump gif to
 * http://hgwdev.cse.ucsc.edu/~angie/gbrowseTest.gif */
{
cgiSpoof(&argc, argv);
cart = cartFromCgiOnly(0, 0, NULL, NULL);
oneTrackInit();
/* Now set up a single track, load it and draw it: */
char *trackName = cartUsualString(cart, "gbrowseTrack", "multiz5way");
struct hvGfx *hvg = oneTrackMakeTrackHvg(trackName,
			"/cluster/home/angie/public_html/gbrowseTest.gif");
FILE *vecF = fopen("vec.dat", "w");
int vecSize = 0;
unsigned char *vec = oneTrackHvgToUcscGlyph(hvg, &vecSize);
fwrite(vec, sizeof(vec[0]), vecSize, vecF);
fclose(vecF);
oneTrackCloseAndWriteImage(&hvg);
return 0;
}
