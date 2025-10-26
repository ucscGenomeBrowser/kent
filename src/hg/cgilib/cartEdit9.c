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

static char *edit9TracksG2p[] = {
    "ddg2p",
};

void cartEdit9(struct cart *cart)
{
int length = 0;

// turn the encodeCcreCombined track into a child of the new ccres supertrack
fprintf(stderr, "turning cCREs track on to show\n");
fflush(stderr);
length = ArraySize(edit9TracksCcre);
cartTurnOnSuper(cart, edit9TracksCcre, length, "cCREs");

// Move the ddg2p track from the decipher container to it's own supertrack
fprintf(stderr, "turning g2pContainer on to show\n");
fflush(stderr);
length = ArraySize(edit9TracksG2p);
cartTurnOnSuper(cart, edit9TracksG2p, length, "g2pContainer");
}
