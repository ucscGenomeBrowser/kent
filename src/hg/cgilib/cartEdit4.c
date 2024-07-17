
#include "common.h"
#include "cart.h"

static char *edit4tracksOmim[] =
{
"omimAvSnp",
"omimGene2",
"omimLocation"
};

void cartEdit4(struct cart *cart)
{
int length = ArraySize(edit4tracksOmim);
cartTurnOnSuper(cart, edit4tracksOmim, length, "omimContainer");
}
