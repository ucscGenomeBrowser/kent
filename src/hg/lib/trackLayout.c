/* trackLayout - this controls the dimensions of the graphic
 * for the genome browser.  Also used for the genome view. */

#include "common.h"
#include "memgfx.h"
#include "cart.h"
#include "hgConfig.h"
#include "hCommon.h"
#include "trackLayout.h"

static char const rcsid[] = "$Id: trackLayout.c,v 1.4 2010/03/27 04:26:47 kent Exp $";


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

static char *fontSizeBackwardsCompatible(char *size)
/* Given "size" argument that may be in old tiny/small/medium/big/huge format,
 * return it in new numerical string format. */
{
if (isdigit(size[0]))
    return size;
else if (sameWord(size, "tiny"))
    return "6";
else if (sameWord(size, "small"))
    return "8";
else if (sameWord(size, "medium"))
    return "14";
else if (sameWord(size, "large"))
    return "18";
else if (sameWord(size, "huge"))
    return "34";
else
    {
    errAbort("unknown font size %s", size);
    return NULL;
    }
}

void trackLayoutInit(struct trackLayout *tl, struct cart *cart)
/* Initialize layout around small font and a picture about 600 pixels
 * wide, but this can be overridden by cart. */
{
char *fontType = cartUsualString(cart, "fontType", "medium");
boolean fontExtras = trackLayoutInclFontExtras();

tl->textSize = fontSizeBackwardsCompatible(cartUsualString(cart, textSizeVar, "small"));
MgFont *font = NULL;
if (fontExtras && sameString(fontType,"bold"))
    {
    if (sameString(tl->textSize, "6"))
	 font = mgTinyBoldFont();
    else if (sameString(tl->textSize, "8"))
	 font = mgHelveticaBold8Font();
    else if (sameString(tl->textSize, "10"))
	 font = mgHelveticaBold10Font();
    else if (sameString(tl->textSize, "12"))
	 font = mgHelveticaBold12Font();
    else if (sameString(tl->textSize, "14"))
	 font = mgHelveticaBold14Font();
    else if (sameString(tl->textSize, "18"))
	 font = mgHelveticaBold18Font();
    else if (sameString(tl->textSize, "24"))
	 font = mgHelveticaBold24Font();
    else if (sameString(tl->textSize, "34"))
	 font = mgHelveticaBold34Font();
    else
	 errAbort("unknown textSize %s", tl->textSize);
    }
else if (fontExtras && sameString(fontType,"fixed"))
    {
    if (sameString(tl->textSize, "6"))
	 font = mgTinyFixedFont();
    else if (sameString(tl->textSize, "8"))
	 font = mgCourier8Font();
    else if (sameString(tl->textSize, "10"))
	 font = mgCourier10Font();
    else if (sameString(tl->textSize, "12"))
	 font = mgCourier12Font();
    else if (sameString(tl->textSize, "14"))
	 font = mgCourier14Font();
    else if (sameString(tl->textSize, "18"))
	 font = mgCourier18Font();
    else if (sameString(tl->textSize, "24"))
	 font = mgCourier24Font();
    else if (sameString(tl->textSize, "34"))
	 font = mgCourier34Font();
    else
	 errAbort("unknown textSize %s", tl->textSize);
    }
else
    {
    if (sameString(tl->textSize, "6"))
	 font = mgTinyFont();
    else if (sameString(tl->textSize, "8"))
	 font = mgSmallFont();
    else if (sameString(tl->textSize, "10"))
	 font = mgHelvetica10Font();
    else if (sameString(tl->textSize, "12"))
	 font = mgHelvetica12Font();
    else if (sameString(tl->textSize, "14"))
	 font = mgHelvetica14Font();
    else if (sameString(tl->textSize, "18"))
	 font = mgHelvetica18Font();
    else if (sameString(tl->textSize, "24"))
	 font = mgHelvetica24Font();
    else if (sameString(tl->textSize, "34"))
	 font = mgHelvetica34Font();
    else
	 errAbort("unknown textSize %s", tl->textSize);
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
