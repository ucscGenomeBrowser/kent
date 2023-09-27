

#ifndef __MOUSEOVER_H__
#define __MOUSEOVER_H__

struct mouseOverScheme
{
    struct mouseOverScheme *next; // Don't know why this would be useful yet, but let's leave room.
    int mouseOverIdx;
    char *mouseOverPattern;
    int fieldCount;
    char **fieldNames;
};


struct mouseOverScheme *mouseOverSetupForBbi(struct trackDb *tdb, struct bbiFile *bbiFile);


char *mouseOverGetBbiText (struct mouseOverScheme *scheme, struct bigBedInterval *bbi, char *chromName);

#endif // __MOUSEOVER_H__
