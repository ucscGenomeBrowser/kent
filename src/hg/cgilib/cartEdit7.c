#include "common.h"
#include "cart.h"


void cartEdit7(struct cart *cart)
{
// since spliceAI got turned from a super track into a composite track and the browser saves the default state of subtracks of supertracks into 
// sesssions, which unfortunately will turn on the new composite spliceAI track if set, we need to remove these default settings from loaded sessions
// if spliceAI is not set to show.  If spliceAI is on, then we want to show the supertrack.
char *spliceAiVis = cartOptionalString(cart, "spliceAI");
boolean turnOnSuper = FALSE;

if ((spliceAiVis == NULL) || sameString(spliceAiVis, "hide"))
    {
    cartRemove(cart, "spliceAIsnvs");
    cartRemove(cart, "spliceAIindels");
    cartRemove(cart, "spliceAIsnvsMasked");
    cartRemove(cart, "spliceAIindelsMasked");
    }
else
    turnOnSuper = TRUE;

// check to see if abSplice is visibile, if so turn on the supertrack
char *abSpliceVis = cartOptionalString(cart, "abSplice");

if ((abSpliceVis != NULL) && differentString(abSpliceVis, "hide"))
    turnOnSuper = TRUE;

if (turnOnSuper)
    {
    // if we're going to turn on the superTrack we want to turn off spliceVarDb if the supertrack wasn't on before since it's on by default
    char *superVis = cartOptionalString(cart, "spliceImpactSuper");

    if ((superVis == NULL) || sameString(superVis, "hide"))
        cartSetString(cart, "spliceVarDb", "hide");
    cartSetString(cart, "spliceImpactSuper", "show");
    }
}
