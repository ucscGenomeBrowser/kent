/* cartRewrite -- routines to enable cart rewrites. Carts and trackDbs
 * have a version number and this code knows how to make the cart compatble
 * with trackDb. */

/* Copyright (C) 2021 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "cart.h"
#include "hgConfig.h"

void cartEdit0(struct cart *cart);
void cartEdit1(struct cart *cart);
void cartEdit2(struct cart *cart);
void cartEdit3(struct cart *cart);
void cartEdit4(struct cart *cart);
void cartEdit5(struct cart *cart);
void cartEdit6(struct cart *cart);
void cartEdit7(struct cart *cart);
void cartEdit9(struct cart *cart);

struct cartRewrite
{
void (*func)(struct cart *cart);
};

// Here's the list of cart rewrite functions
static struct cartRewrite cartRewrites[] =
{
{ cartEdit0},
{ cartEdit1},
{ cartEdit2},
{ cartEdit3},
{ cartEdit4},
{ cartEdit5},
{ cartEdit6},
{ cartEdit7},
{ cartEdit9},
};

void cartRewrite(struct cart *cart, unsigned trackDbCartVersion, unsigned cartVersion)
/* Rewrite the cart to update it to expectations of trackDb. */
{
if (sameString(cfgOptionDefault("cartVersion", "on"), "off"))
    return;

// call the rewrite functions to bring us up to the trackDb cart version
for(; cartVersion < trackDbCartVersion; cartVersion++)
    {
    // if we don't have a rewrite for this increment, bail out
    // with a warning in the error_log
    if (cartVersion >= ArraySize(cartRewrites))
        {
        fprintf(stderr,"CartRewriteError: do not have cart rewrite rules to bring it up to version %d requested by trackDb. Reached level %d\n", trackDbCartVersion, cartVersion);
        break;
        }
    (cartRewrites[cartVersion].func)(cart);
    }

cartSetVersion(cart, cartVersion);
}

void cartTurnOnSuper(struct cart *cart, char **trackNames, unsigned numTracks, char *superTrackName)
/* Turn on a supertrack if any of the subtracks are not hidden.  ASSUMES ALL TRACKS ARE HIDDEN
 * by default.
 */
{
boolean cartTurnOnSuper = FALSE;
int ii;

// go through all the tracks moved into the supertrack
for(ii = 0; ii < numTracks; ii++)
    {
    char *vis = cartOptionalString(cart, trackNames[ii]);

    if (vis)
        {
        if (differentString(vis, "hide"))
            {
            // Turn on the super track since one of its subtracks was visible before
            cartTurnOnSuper = TRUE;
            break;
            }
        }
    }

if (cartTurnOnSuper)
    cartSetString(cart, superTrackName, "show");
}

void cartMatchValue(struct cart *cart, char *oldTrackName,  char *newTrackName)
/* Make new track have the same visibility as an old track */
{
char *vis = cartOptionalString(cart, oldTrackName);

if (vis)
    cartSetString(cart, newTrackName, vis);
}
