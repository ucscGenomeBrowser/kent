/* trackLayout - this controls the dimensions of the graphic
 * for the genome browser.  Also used for the genome view. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "memgfx.h"
#include "cart.h"
#include "hgConfig.h"
#include "hCommon.h"
#include "trackLayout.h"



void trackLayoutSetPicWidth(struct trackLayout *tl, char *s)
/* Set pixel width from ascii string. */
{
int maxWidth = MAX_DISPLAY_PIXEL_WIDTH;
char *maxDisplayPixelWidth = cfgOptionDefault("maxDisplayPixelWidth", NULL);

if (isNotEmpty(maxDisplayPixelWidth))
   maxWidth = sqlUnsigned(maxDisplayPixelWidth);

if (s != NULL && isdigit(s[0]))
    {
    tl->picWidth = atoi(s);
#ifdef LOWELAB
    if (tl->picWidth > 60000)
      tl->picWidth = 60000;
#else
    if (tl->picWidth > maxWidth)
      tl->picWidth = maxWidth;
#endif
    if (tl->picWidth < 320)
        tl->picWidth = 320;
    }
tl->trackWidth = tl->picWidth - tl->leftLabelWidth;
}

boolean trackLayoutInclFontExtras()
/* Check if fonts.extra is set to use "yes" in the config.  This enables
 * extra fonts and related options that are not part of the public browser */
{
static boolean first = TRUE;
static boolean enabled = FALSE;
if (first)
    {
    char *val = cfgOptionDefault("fonts.extra", NULL);
    if (val != NULL)
        enabled = sameString(val, "yes");
    first = FALSE;
    }
return enabled;
}

void trackLayoutInit(struct trackLayout *tl, struct cart *cart)
/* Initialize layout around small font and a picture about 600 pixels
 * wide, but this can be overridden by cart. */
{
char *fontType = "medium";
boolean fontExtras = trackLayoutInclFontExtras();
if (fontExtras)
    fontType = cartUsualString(cart, "fontType", fontType);

tl->textSize = mgFontSizeBackwardsCompatible(cartUsualString(cart, textSizeVar, "small"));
MgFont *font = mgFontForSizeAndStyle(tl->textSize, fontType);
tl->font = font;
tl->mWidth = mgFontStringWidth(font, "M");
tl->nWidth = mgFontStringWidth(font, "n");
tl->fontHeight = mgFontLineHeight(font);
tl->barbHeight = tl->fontHeight/4;
tl->barbSpacing = (tl->fontHeight+1)/2;
tl->picWidth = hgDefaultPixWidth;
trackLayoutSetPicWidth(tl, cartOptionalString(cart, "pix"));
}
