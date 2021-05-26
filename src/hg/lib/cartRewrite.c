
#include "common.h"
#include "cart.h"

static char *edit0tracks[] =
{
"snp151Flagged",
"snp151Mult",
"snp150Mult",
"snp150",
"snp150Common",
"snp150Flagged",
"snp149Mult",
"snp149",
"snp149Common",
"snp149Flagged",
"snp141Mult",
"snp141Flagged",
"snp141Common",
"snp141",
"snp142Mult",
"snp142Flagged",
"snp142Common",
"snp142",
"snp144Mult",
"snp144Flagged",
"snp144Common",
"snp144",
"snp147Mult",
"snp147Flagged",
"snp147Common",
"snp147",
"snp146Mult",
"snp146Flagged",
"snp146Common",
"snp146",
};

static void cartEdit0(struct cart *cart)
/* Moving a bunch of SNP tracks to an archive super track.   We need
 * to turn on the super track if any of what are now subTracks are visible.
 */
{
boolean turnOnSuper = FALSE;
int length = ArraySize(edit0tracks);
int ii;

printf("cartEdit0 ");

// go through all the tracks moved into the supertrack
for(ii = 0; ii < length; ii++)
    {
    char *vis = cartOptionalString(cart, edit0tracks[ii]);

    if (vis)
        {
        if (sameString(vis, "dense") || sameString(vis, "pack") || sameString(vis, "full") )
            {
            // Turn on the super track since one of its subtracks was visible before
            printf("turning on super ");
            turnOnSuper = TRUE;
            break;
            }
        }
    }

if (turnOnSuper)
    cartSetString(cart, "dbSnpArchive", "show");
}

// Here's the list of cart rewrite functions
struct cartRewrite
{
void (*func)(struct cart *cart);
};

static struct cartRewrite cartRewrites[] =
{
{ cartEdit0},
};

void cartRewrite(struct cart *cart, unsigned trackDbCartVersion, unsigned cartVersion)
/* Rewrite the cart to update it to expectations of trackDb. */
{
if (trackDbCartVersion > ArraySize(cartRewrites))
    errAbort("Do not have cart rewrite rules to bring it up to version %d\n", trackDbCartVersion);

// call the rewrite functions to bring us up to the trackDb cart version
int ii;
for(ii = cartVersion; ii < trackDbCartVersion; ii++)
    (cartRewrites[ii].func)(cart);

cartSetVersion(cart, trackDbCartVersion);
}

