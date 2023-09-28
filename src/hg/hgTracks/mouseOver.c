/* mouseOver.c
 * Some routines for managing the mouseOver options for bigBed-like tracks, where mouseover can
 * optionally be drawn from any (or any combination) of fields.
 */

// arguably these routines should go in bigBedTrack.c

#include "common.h"
#include "cart.h"
#include "trackDb.h"
#include "hgTracks.h"
#include "bigBed.h"
#include "mouseOver.h"


struct mouseOverScheme *mouseOverSetupForBbi(struct trackDb *tdb, struct bbiFile *bbiFile)
{
struct mouseOverScheme *newScheme;
AllocVar(newScheme);
char *mouseOverField = cartOrTdbString(cart, tdb, "mouseOverField", NULL);
int mouseOverIdx = bbExtraFieldIndex(bbiFile, mouseOverField);
newScheme->fieldCount = bbiFile->fieldCount;
if (mouseOverIdx)
    {
    newScheme->mouseOverIdx = mouseOverIdx;
    }
else
    {
    newScheme->mouseOverPattern = cartOrTdbString(cart, tdb, "mouseOver", NULL);
    if (newScheme->mouseOverPattern)
        {
        AllocArray(newScheme->fieldNames, bbiFile->fieldCount);
        struct slName *field = NULL, *fields = bbFieldNames(bbiFile);
        int i =  0;
        for (field = fields; field != NULL; field = field->next)
            newScheme->fieldNames[i++] = field->name;
        }
    }
return newScheme;
}


char *mouseOverGetBbiText (struct mouseOverScheme *scheme, struct bigBedInterval *bbi, char *chromName)
{
char startBuf[16], endBuf[16];
char *bedRow[scheme->fieldCount];
bigBedIntervalToRow(bbi, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
char *mouseOverText = NULL;

if (scheme->fieldCount > 3)
    mouseOverText = cloneString(bedRow[3]); // Use the name field if it exists, by default

if (scheme->mouseOverIdx > 0)
//    mouseOverText = restField(bbi, scheme->mouseOverIdx);
    mouseOverText = cloneString(bedRow[scheme->mouseOverIdx+3]);  // +3 because these "rest" indexes start with the name (4th) field
else if (scheme->mouseOverPattern)
    mouseOverText = replaceFieldInPattern(scheme->mouseOverPattern, scheme->fieldCount, scheme->fieldNames, bedRow);

return mouseOverText;
}
