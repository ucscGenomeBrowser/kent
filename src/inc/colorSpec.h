/* colorSpec - parse a color written the way a person writes one, in a track setting or a
 * data column: three or four comma-separated values 0 to 255, a #rrggbb hex code, or an
 * HTML color name.
 *
 * The value handed back is packed the way the bed code packs a color, alpha in the
 * highest byte then red, green and blue, so bedColorToRgb and bedColorToGfxColor in
 * basicBed.h convert it for drawing.  A spec with no alpha leaves that byte zero.
 *
 * See bedParseColor in basicBed.h, which takes the same spellings and some others, and
 * which this does not replace yet. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef COLORSPEC_H
#define COLORSPEC_H

boolean colorSpecMayParse(char *spec, unsigned *retColor);
/* Parse a color spec.  Return FALSE, leaving retColor alone, when it is not one, so that a
 * caller holding the context can say which setting or column was wrong. */

unsigned colorSpecParse(char *spec);
/* Parse a color spec, errAbort naming the spec and the spellings it could have used. */

unsigned colorSpecParseOrDefault(char *spec, char *defaultSpec);
/* Parse a color spec loosely, returning the color of defaultSpec for anything that cannot
 * be read at all.  This takes spellings the two above turn away, among them an empty value
 * between two commas and a number wide enough to wrap.  defaultSpec is written in the code
 * rather than by a user, so it is read strictly and errAborts if it is not a color.
 *
 * Use this for a field existing hubs and data files already set, so that a value already
 * written keeps drawing the color it draws now.  Use the strict pair for a new field,
 * where the error reaches the author writing the first value. */

unsigned colorSpecParseOrDefaultColor(char *spec, unsigned defaultColor);
/* colorSpecParseOrDefault with the default already packed, for a caller that reads a color
 * off every row and would otherwise parse the same default text each time. */

#endif /* COLORSPEC_H */
