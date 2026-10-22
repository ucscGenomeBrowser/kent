/* colorSpec - parse a color written the way a person writes one.  See colorSpec.h. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "htmlColor.h"
#include "colorSpec.h"

static boolean parseRgbSpec(char *spec, unsigned *retColor)
/* Parse three or four comma-separated values 0 to 255.  Spaces around a value are allowed,
 * since a track setting is often written 0, 0, 255. */
{
char dupe[64], *words[5];
unsigned part[4];
int count, i;
if (strlen(spec) >= sizeof dupe)
    return FALSE;
safecpy(dupe, sizeof dupe, spec);
count = chopString(dupe, ",", words, ArraySize(words));
if ((count != 3) && (count != 4))
    return FALSE;
for (i = 0; i < count; i++)
    {
    char *word = trimSpaces(words[i]);
    if (!isAllDigits(word) || (atoi(word) > 255))
        return FALSE;
    part[i] = atoi(word);
    }
*retColor = (part[0] << 16) | (part[1] << 8) | part[2];
if (count == 4)
    *retColor |= (part[3] << 24);
return TRUE;
}

boolean colorSpecMayParse(char *spec, unsigned *retColor)
/* Parse a color spec.  Return FALSE, leaving retColor alone, when it is not one, so that a
 * caller holding the context can say which setting or column was wrong. */
{
unsigned color = 0;
if (isEmpty(spec))
    return FALSE;
if (strchr(spec, ',') != NULL)
    {
    if (!parseRgbSpec(spec, &color))
        return FALSE;
    }
/* htmlColorForCode writes its answer out before it decides the code is bad, so it gets a
 * local to scribble on rather than the caller's variable. */
else if (!htmlColorForCode(spec, &color) && !htmlColorForName(spec, &color))
    return FALSE;
*retColor = color;
return TRUE;
}

unsigned colorSpecParse(char *spec)
/* Parse a color spec, errAbort naming the spec and the spellings it could have used. */
{
unsigned color;
if (!colorSpecMayParse(spec, &color))
    errAbort("'%s' is not a color.  Write it as r,g,b, for example 0,0,255, or as a hex "
             "code such as #0000ff, or as an HTML color name such as blue.", spec);
return color;
}
