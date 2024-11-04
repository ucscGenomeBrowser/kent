#include "common.h"
#include "cart.h"

static char *edit6decipherTracks[] =
{
"decipher",
"decipherSnvs"
};

void cartEdit6(struct cart *cart)
{
cartTurnOnSuper(cart, edit6decipherTracks,  ArraySize(edit6decipherTracks),  "decipherContainer");
}
