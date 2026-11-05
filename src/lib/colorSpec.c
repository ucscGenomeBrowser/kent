/* colorSpec - parse a color written the way a person writes one.  See colorSpec.h. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "htmlColor.h"
#include "colorSpec.h"

static boolean parseRgbSpec(char *spec, boolean strict, unsigned *retColor)
/* Parse three or four comma-separated values 0 to 255.  Spaces around a value are allowed,
 * since a track setting is often written 0, 0, 255.
 *
 * Strict turns away an empty value and a number too long to be one.  Loose takes both, for
 * compatibility with existing hubs and data files: chopString runs the separators together,
 * so 0,,0,255 reads as 0,0,255, and atoi of 4294967296 comes back 0 rather than something
 * over 255. */
{
char dupe[64], *words[5];
unsigned part[4];
int count, i;
if (strlen(spec) >= sizeof dupe)
    return FALSE;
safecpy(dupe, sizeof dupe, spec);
if (strict)
    count = chopByChar(dupe, ',', words, ArraySize(words));
else
    count = chopString(dupe, ",", words, ArraySize(words));
if ((count != 3) && (count != 4))
    return FALSE;
for (i = 0; i < count; i++)
    {
    char *word = trimSpaces(words[i]);
    if (isEmpty(word) || !isAllDigits(word))
        return FALSE;
    if (strict && (strlen(word) > 3))
        return FALSE;   /* 4294967296 passes the test below, atoi having wrapped it to 0 */
    if (atoi(word) > 255)
        return FALSE;
    part[i] = atoi(word);
    }
*retColor = (part[0] << 16) | (part[1] << 8) | part[2];
if (count == 4)
    *retColor |= (part[3] << 24);
return TRUE;
}

static boolean mayParse(char *spec, boolean strict, unsigned *retColor)
/* Parse a color spec in one of the three spellings.  Return FALSE without touching
 * retColor when it is not one. */
{
unsigned color = 0;
if (isEmpty(spec))
    return FALSE;
if (strchr(spec, ',') != NULL)
    {
    if (!parseRgbSpec(spec, strict, &color))
        return FALSE;
    }
/* htmlColorForCode writes its answer out before it decides the code is bad, so it gets a
 * local to scribble on rather than the caller's variable. */
else if (!htmlColorForCode(spec, &color) && !htmlColorForName(spec, &color))
    return FALSE;
*retColor = color;
return TRUE;
}

boolean colorSpecMayParse(char *spec, unsigned *retColor)
/* Parse a color spec.  Return FALSE, leaving retColor alone, when it is not one, so that a
 * caller holding the context can say which setting or column was wrong. */
{
return mayParse(spec, TRUE, retColor);
}

unsigned colorSpecParse(char *spec)
/* Parse a color spec, errAbort naming the spec and the spellings it could have used. */
{
unsigned color = 0;
if (!colorSpecMayParse(spec, &color))
    errAbort("'%s' is not a color.  Write it as r,g,b, for example 0,0,255, or as a hex "
             "code such as #0000ff, or as an HTML color name such as blue.", spec);
return color;
}

unsigned colorSpecParseOrDefault(char *spec, char *defaultSpec)
/* Parse a color spec loosely, returning the color of defaultSpec for anything that cannot
 * be read at all.  defaultSpec is written in the code rather than by a user, so it is read
 * strictly and errAborts if it is not a color.
 *
 * For a field existing hubs and data files already set.  Those values were written against
 * looser parsing, and reading one strictly now drops a track over a value that has worked
 * for years; a wrong color is the smaller harm.  Use the strict pair for a new field, where
 * the error reaches the author writing the first value. */
{
unsigned color = 0;
if (!mayParse(spec, FALSE, &color))
    return colorSpecParse(defaultSpec);
return color;
}

unsigned colorSpecParseOrDefaultColor(char *spec, unsigned defaultColor)
/* colorSpecParseOrDefault with the default already packed, for a caller that reads a color
 * off every row and would otherwise parse the same default text each time. */
{
unsigned color = 0;
if (!mayParse(spec, FALSE, &color))
    return defaultColor;
return color;
}
