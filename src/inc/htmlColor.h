/* HTML colors */

/* Copyright (C) 2017 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef HTMLCOLOR_H
#define HTMLCOLOR_H

int htmlColorCount();
/* Return count of defined HTML colors */

boolean htmlColorExists(char *name);
/* Determine if color name is one of the defined HTML basic set */

struct slName *htmlColorNames();
/* Return list of defined HTML colors */

boolean htmlColorForName(char *name, unsigned *value);
/* Lookup color for name.  Return false if not a valid color name */

boolean htmlColorForCode(char *code, unsigned *value);
/* Convert value to decimal and return true if code is valid #NNNNNN hex code */

void htmlColorToRGB(unsigned value, int *r, int *g, int *b);
/* Convert an unsigned RGB value into separate R, G, and B components */

#endif

