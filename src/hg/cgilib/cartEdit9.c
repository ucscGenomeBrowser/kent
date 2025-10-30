/* The eigth edition of cartEdits. Please note that historically, cartEdit files
 * were named with 0-index names, like cartEdit0, cartEdit1, ... but the corresponding
 * trackDb entry referring to the cartEdit was 1-indexes, cartVersion 1, ...
 * Beginning with this cartEdit, we will now name cartEdits 1-indexed, so there
 * will be no cartEdit8, just like there was no October 5-14, 1582. */
#include "common.h"
#include "cart.h"

static char *edit9TracksCcre[] = {
    "encodeCcreCombined",
};

void cartEdit9(struct cart *cart)
{
int length = 0;

// turn the encodeCcreCombined track into a child of the new ccres supertrack
length = ArraySize(edit9TracksCcre);
cartTurnOnSuper(cart, edit9TracksCcre, length, "cCREs");

// Move the ddg2p track from the decipher container to it's own track called g2p,
// but only if the decipherContainer was on previously and ddg2p was also on
char *oldContainerVis = cartOptionalString(cart, "decipherContainer");
char *oldDdg2pVis = cartOptionalString(cart, "ddg2p");
if (oldContainerVis && sameString(oldContainerVis, "show"))
    if (oldDdg2pVis && !sameString(oldDdg2pVis, "hide"))
        cartSetString(cart, "g2p", oldDdg2pVis);
}
