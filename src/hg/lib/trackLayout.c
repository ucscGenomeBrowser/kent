/* trackLayout - this controls the dimensions of the graphic
 * for the genome browser.  Also used for the genome view. */

#include "common.h"
#include "memgfx.h"
#include "cart.h"
#include "hgConfig.h"
#include "hCommon.h"
#include "trackLayout.h"

static char const rcsid[] = "$Id: trackLayout.c,v 1.3 2008/09/17 18:10:14 kent Exp $";


void trackLayoutSetPicWidth(struct trackLayout *tl, char *s)
/* Set pixel width from ascii string. */
{
if (s != NULL && isdigit(s[0]))
    {
    tl->picWidth = atoi(s);
#ifdef LOWELAB    
    if (tl->picWidth > 60000)
      tl->picWidth = 60000;   
#else
    if (tl->picWidth > 5000)
      tl->picWidth = 5000;   
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
char *fontType = cartUsualString(cart, "fontType", "medium");
boolean fontExtras = trackLayoutInclFontExtras();

MgFont *font = (MgFont *)NULL;
tl->textSize = cartUsualString(cart, textSizeVar, "small");
if (fontExtras && sameString(fontType,"bold"))
    {
    if (sameString(tl->textSize, "small"))
	 font = mgSmallBoldFont();
    else if (sameString(tl->textSize, "tiny"))
	 font = mgTinyBoldFont();
    else if (sameString(tl->textSize, "medium"))
	 font = mgMediumBoldFont();
    else if (sameString(tl->textSize, "large"))
	 font = mgLargeBoldFont();
    else if (sameString(tl->textSize, "huge"))
	 font = mgHugeBoldFont();
    else
	 font = mgSmallBoldFont();	/*	default to small	*/
    }
else if (fontExtras && sameString(fontType,"fixed"))
    {
    if (sameString(tl->textSize, "small"))
	 font = mgSmallFixedFont();
    else if (sameString(tl->textSize, "tiny"))
	 font = mgTinyFixedFont();
    else if (sameString(tl->textSize, "medium"))
	 font = mgMediumFixedFont();
    else if (sameString(tl->textSize, "large"))
	 font = mgLargeFixedFont();
    else if (sameString(tl->textSize, "huge"))
	 font = mgHugeFixedFont();
    else
	 font = mgSmallFixedFont();	/*	default to small	*/
    }
else
    {
    if (sameString(tl->textSize, "small"))
	 font = mgSmallFont();
    else if (sameString(tl->textSize, "tiny"))
	 font = mgTinyFont();
    else if (sameString(tl->textSize, "medium"))
	 font = mgMediumFont();
    else if (sameString(tl->textSize, "large"))
	 font = mgLargeFont();
    else if (sameString(tl->textSize, "huge"))
	 font = mgHugeFont();
    else
	font = mgSmallFont(); /* default to small */
    }
tl->font = font;
tl->mWidth = mgFontStringWidth(font, "M");
tl->nWidth = mgFontStringWidth(font, "n");
tl->fontHeight = mgFontLineHeight(font);
tl->barbHeight = tl->fontHeight/4;
tl->barbSpacing = (tl->fontHeight+1)/2;
tl->picWidth = hgDefaultPixWidth;
trackLayoutSetPicWidth(tl, cartOptionalString(cart, "pix"));
}
