/* The tenth edition of cartEdits.  Re-applies the encodeCcreCombined ->
 * cCREs supertrack migration from cartEdit9.  This is needed because carts
 * saved between the cartVersion 9 rollout and the trackDb switch to the new
 * cCREs supertrack still reference encodeCcreCombined directly and were
 * already at version 9, so cartEdit9 would not run for them. */
#include "common.h"
#include "cart.h"

static char *edit10TracksCcre[] = {
    "encodeCcreCombined",
};

void cartEdit10(struct cart *cart)
{
int length = ArraySize(edit10TracksCcre);
cartTurnOnSuper(cart, edit10TracksCcre, length, "cCREs");
}
