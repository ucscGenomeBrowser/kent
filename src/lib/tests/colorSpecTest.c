/* colorSpecTest - check what colorSpecMayParse takes and what it turns away. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "colorSpec.h"

static char *specs[] = {
    /* the three spellings */
    "0,0,255", "255,255,255", "0,0,0", "#0000ff", "#00FF00", "#abcdef", "blue", "black",
    "aqua",
    /* spaces around the values, which a track setting often has */
    " 12 , 34 , 56 ",
    /* four values, the fourth being alpha */
    "0,0,255,128", "1,2,3,4",
    /* a name the HTML table does not carry, so not a color here */
    "darkgreen", "rebeccapurple",
    /* out of range, wrong count, junk */
    "", "256,0,0", "0,0", "0,0,0,0,0", "0,,255", "0,0,blue", "-1,0,0", "0,0,25.5",
    "blue2", "#0000f", "#0000fff", "#xxyyzz", "0000ff", "16711680", "rgb(0,0,255)",
};

int main(int argc, char *argv[])
{
int i;
for (i = 0; i < ArraySize(specs); i++)
    {
    unsigned color = 0xdeadbeef;
    boolean ok = colorSpecMayParse(specs[i], &color);
    if (ok)
        printf("'%s' -> %08x\n", specs[i], color);
    else
        printf("'%s' -> not a color, retColor untouched %s\n", specs[i],
               (color == 0xdeadbeef ? "yes" : "NO"));
    }
return 0;
}
