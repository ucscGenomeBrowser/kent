/* bigBedLabel.c - Label things in big beds . */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "bPlusTree.h"
#include "bbiFile.h"
#include "bigBed.h"
#include "hgFind.h"
#include "trix.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "hdb.h"
#include "errCatch.h"
#include "hui.h"
#include "obscure.h"

void bigBedLabelCalculateFields(struct cart *cart, struct trackDb *tdb, struct bbiFile *bbi,  struct slInt **labelColumns )
/* Figure out which fields are available to label a bigBed track. */
{
//struct bbiFile *bbi = fetchBbiForTrack(track);
struct asObject *as = bigBedAsOrDefault(bbi);
struct slPair *labelList = buildFieldList(tdb, "labelFields",  as);

if (labelList == NULL)
    {
    // There is no labelFields entry in the trackDb.
    // If there is a name, use it by default, otherwise no label by default 
    if (bbi->fieldCount > 3)
        slAddHead(labelColumns, slIntNew(3));
    }
else if (sameString(labelList->name, "none"))
    return;  // no label
else
    {
    // what has the user said to use as a label
    // we need to check parents as well as this tdb
    char cartVar[1024];
    struct hashEl *labelEl = NULL;
    struct trackDb *cartTdb = tdb;
    while ( labelEl == NULL)
        {
        safef(cartVar, sizeof cartVar, "%s.label", cartTdb->track);
        labelEl = cartFindPrefix(cart, cartVar);
        if ((labelEl != NULL) || (cartTdb->parent == NULL))
            break;

        cartTdb = cartTdb->parent;
        }

    // fill hash with fields that should be used for labels
    // first turn on all the fields that are in defaultLabelFields
    struct hash *onHash = newHash(4);
    struct slPair *defaultLabelList = buildFieldList(tdb, "defaultLabelFields",  as);
    if (defaultLabelList != NULL)
        {
        for(; defaultLabelList; defaultLabelList = defaultLabelList->next)
            hashStore(onHash, defaultLabelList->name);
        }
    else
        // no default list, use first entry in labelFields as default
        hashStore(onHash, labelList->name);

    // use cart variables to tweak the default-on hash
    for(; labelEl; labelEl = labelEl->next)
        {
        /* the field name is after the <trackName>.label string */
        char *fieldName = &labelEl->name[strlen(cartVar) + 1];

        if (sameString((char *)labelEl->val, "1"))
            hashStore(onHash, fieldName);
        else if (sameString((char *)labelEl->val, "0"))
            hashRemove(onHash, fieldName);
        }

    struct slPair *thisLabel = labelList;
    for(; thisLabel; thisLabel = thisLabel->next)
        {
        if (hashLookup(onHash, thisLabel->name))
            {
            // put this column number in the list of columns to use to make label
            slAddHead(labelColumns, slIntNew(ptToInt(thisLabel->val)));
            }
        }
    slReverse(labelColumns);
    }
}

char *bigBedMakeLabel(struct trackDb *tdb,  struct slInt *labelColumns, struct bigBedInterval *bb, char *chromName)
// Build a label for a bigBedTrack from the requested label fields.
{
char *labelSeparator = stripEnclosingDoubleQuotes(trackDbSettingClosestToHome(tdb, "labelSeparator"));
if (labelSeparator == NULL)
    labelSeparator = "/";
char *restFields[256];
if (bb->rest != NULL)
    chopTabs(cloneString(bb->rest), restFields);
struct dyString *dy = newDyString(128);
boolean firstTime = TRUE;
struct slInt *labelInt = labelColumns;
for(; labelInt; labelInt = labelInt->next)
    {
    if (!firstTime)
        dyStringAppend(dy, labelSeparator);

    switch(labelInt->val)
        {
        case 0:
            dyStringAppend(dy, chromName);
            break;
        case 1:
            dyStringPrintf(dy, "%d", bb->start);
            break;
        case 2:
            dyStringPrintf(dy, "%d", bb->end);
            break;
        default:
            assert(bb->rest != NULL);
            dyStringPrintf(dy, "%s", restFields[labelInt->val - 3]);
            break;
        }
    firstTime = FALSE;
    }
return dyStringCannibalize(&dy);
}
