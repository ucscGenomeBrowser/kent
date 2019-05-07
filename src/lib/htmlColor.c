/* HTML colors */

/* Copyright (C) 2017 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

struct htmlColor {
    char *name;
    unsigned rgb;
};

static struct htmlColor htmlColors[] = {
/* The 16 HTML basic colors, as per //www.w3.org/TR/css3-color/#html4 */
    { "black", 0 },
    { "silver", 0xc0c0c0 },
    { "gray", 0x808080 },
    { "white", 0xffffff },
    { "maroon", 0x800000 },
    { "red", 0xff0000 },
    { "purple", 0x800080 },
    { "fuchsia", 0xff00ff },
    { "green", 0x008000 },
    { "lime", 0x00ff00 },
    { "olive", 0x808000 },
    { "yellow", 0xffff00 },
    { "navy", 0x000080 },
    { "blue", 0x0000ff },
    { "teal", 0x008080 },
    { "aqua", 0x00ffff }
};

int htmlColorCount()
/* Return count of defined HTML colors */
{
return (sizeof(htmlColors) / (sizeof(struct htmlColor)));
}

struct slName *htmlColorNames()
/* Return list of defined HTML colors */
{
int count = htmlColorCount();
int i;
struct slName *colors = NULL;
for (i=0; i<count; i++)
    {
    slAddHead(&colors, slNameNew(htmlColors[i].name));
    }
slReverse(&colors);
return colors;
}

boolean htmlColorForName(char *name, unsigned *value)
/* Lookup color for name.  Return false if not a valid color name */
{
int count = htmlColorCount();
int i;
for (i=0; i<count; i++)
    {
    if (sameString(name, htmlColors[i].name))
        {
        if (value != NULL)
            *value = htmlColors[i].rgb;
        return TRUE;
        }
    }
return FALSE;
}

boolean htmlColorForCode(char *code, unsigned *value)
/* Convert value to decimal and return true if code is valid #NNNNNN hex code */
{
if (*code == '\\')
    code++;
if (*code != '#' || strlen(code) != 7) 
    return FALSE;
char *end;
unsigned ret = (unsigned)strtol(&code[1], &end, 16);
if (value != NULL)
    *value = ret;
if (*end == '\0')
    return TRUE;
return FALSE;
}

boolean htmlColorExists(char *name)
/* Determine if color name is one of the defined HTML basic set */
{
return htmlColorForName(name, NULL);
}

void htmlColorToRGB(unsigned value, int *r, int *g, int *b)
/* Convert an unsigned RGB value into separate R, G, and B components */
{
    if (r != NULL)
        *r = (value >> 16) & 0xff;
    if (g != NULL)
        *g = (value >> 8) & 0xff;
    if (b != NULL)
        *b = value & 0xff;
}
