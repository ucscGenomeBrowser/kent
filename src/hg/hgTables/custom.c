/* custom - stuff related to custom tracks. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "hdb.h"
#include "trackDb.h"
#include "grp.h"
#include "customTrack.h"
#include "hgTables.h"

struct customTrack *theCtList = NULL;	/* List of custom tracks. */
struct slName *browserLines = NULL;	/* Browser lines in custom tracks. */

boolean isCustomTrack(char *table)
/* Return TRUE if table is a custom track. */
{
return startsWith("ct_", table);
}

struct customTrack *getCustomTracks()
/* Get custom track list. */
{
if (theCtList == NULL)
    theCtList = customTracksParseCart(cart, &browserLines, NULL);
return(theCtList);
}

struct customTrack *lookupCt(char *name)
/* Find named custom track. */
{
struct customTrack *ctList = getCustomTracks();
struct customTrack *ct;
for (ct=ctList;  ct != NULL;  ct=ct->next)
    {
    if (sameString(ct->tdb->tableName, name))
	return ct;
    }
return NULL;
}

struct hTableInfo *ctToHti(struct customTrack *ct)
/* Create an hTableInfo from a customTrack. */
{
struct hTableInfo *hti;

if (ct == NULL)
    return(NULL);

AllocVar(hti);
hti->rootName = cloneString(ct->tdb->tableName);
hti->isPos = TRUE;
hti->isSplit = FALSE;
hti->hasBin = FALSE;
hti->type = cloneString(ct->tdb->type);
if (ct->fieldCount >= 3)
    {
    strncpy(hti->chromField, "chrom", 32);
    strncpy(hti->startField, "chromStart", 32);
    strncpy(hti->endField, "chromEnd", 32);
    }
if (ct->fieldCount >= 4)
    {
    strncpy(hti->nameField, "name", 32);
    }
if (ct->fieldCount >= 5)
    {
    strncpy(hti->scoreField, "score", 32);
    }
if (ct->fieldCount >= 6)
    {
    strncpy(hti->strandField, "strand", 32);
    }
if (ct->fieldCount >= 8)
    {
    strncpy(hti->cdsStartField, "thickStart", 32);
    strncpy(hti->cdsEndField, "thickEnd", 32);
    hti->hasCDS = TRUE;
    }
if (ct->fieldCount >= 12)
    {
    strncpy(hti->countField, "blockCount", 32);
    strncpy(hti->startsField, "chromStarts", 32);
    strncpy(hti->endsSizesField, "blockSizes", 32);
    hti->hasBlocks = TRUE;
    }

return(hti);
}

#ifdef NEVER
Some useful custom track routines from hgText:

void filterOptionsCustomTrack(char *table, char *tableId)
/* Print out an HTML table with form inputs for constraints on custom track */

boolean printTabbedBed(struct bed *bedList, struct slName *chosenFields,
		       boolean initialized)
/* Print out the chosen fields of a bedList. */

void doTabSeparatedCT(boolean allFields)

#endif /* NEVER */

struct slName *getBedFields(int fieldCount)
/* Get list of fields for bed of given size. */
{
struct slName *fieldList = NULL, *field;
if (fieldCount >= 3)
    {
    field = newSlName("chrom");
    slAddHead(&fieldList, field);
    field = newSlName("chromStart");
    slAddHead(&fieldList, field);
    field = newSlName("chromEnd");
    slAddHead(&fieldList, field);
    }
if (fieldCount >= 4)
    {
    field = newSlName("name");
    slAddHead(&fieldList, field);
    }
if (fieldCount >= 5)
    {
    field = newSlName("score");
    slAddHead(&fieldList, field);
    }
if (fieldCount >= 6)
    {
    field = newSlName("strand");
    slAddHead(&fieldList, field);
    }
if (fieldCount >= 8)
    {
    field = newSlName("thickStart");
    slAddHead(&fieldList, field);
    field = newSlName("thickEnd");
    slAddHead(&fieldList, field);
    }
if (fieldCount >= 12)
    {
    field = newSlName("reserved");
    slAddHead(&fieldList, field);
    field = newSlName("blockCount");
    slAddHead(&fieldList, field);
    field = newSlName("blockSizes");
    slAddHead(&fieldList, field);
    field = newSlName("chromStarts");
    slAddHead(&fieldList, field);
    }
slReverse(&fieldList);
return fieldList;
}

static void tabBedRow(struct bed *bed, struct slName *fieldList)
/* Print out named fields from bed. */
{
struct slName *field;
boolean needTab = FALSE;
for (field = fieldList; field != NULL; field = field->next)
    {
    char *type = field->name;
    if (needTab)
        hPrintf("\t");
    else
        needTab = TRUE;
    if (sameString(type, "chrom"))
        hPrintf("%s", bed->chrom);
    else if (sameString(type, "chromStart"))
        hPrintf("%u", bed->chromStart);
    else if (sameString(type, "chromEnd"))
        hPrintf("%u", bed->chromEnd);
    else if (sameString(type, "name"))
        hPrintf("%s", bed->name);
    else if (sameString(type, "score"))
        hPrintf("%d", bed->score);
    else if (sameString(type, "strand"))
        hPrintf("%s", bed->strand);
    else if (sameString(type, "thickStart"))
        hPrintf("%u", bed->thickStart);
    else if (sameString(type, "thickEnd"))
        hPrintf("%u", bed->thickEnd);
    else if (sameString(type, "reserved"))
        hPrintf("%u", bed->reserved);
    else if (sameString(type, "blockCount"))
        hPrintf("%u", bed->blockCount);
    else if (sameString(type, "blockSizes"))
	{
	unsigned i;
	for (i=0; i<bed->blockCount; ++i)
	    hPrintf("%u,", bed->blockSizes[i]);
	}
    else if (sameString(type, "chromStarts"))
	{
	unsigned i;
	for (i=0; i<bed->blockCount; ++i)
	    hPrintf("%u,", bed->chromStarts[i]);
	}
    else
        errAbort("Unrecognized bed field %s", type);
    }
hPrintf("\n");
}

void doTabOutCustomTracks(struct customTrack *ct, char *fields)
/* Print out selected fields from custom track.  If fields
 * is NULL, then print out all fields. */
{
struct bed *bedList, *bed;
struct slName *chosenFields, *field;
struct bedFilter *bf = NULL;
struct region *region;
boolean gotResults = FALSE;

#ifdef SOON
bf = constrainBedFields(NULL);
#endif

if (fields == NULL)
    chosenFields = getBedFields(ct->fieldCount);
else
    chosenFields = commaSepToSlNames(fields);
hPrintf("#");
for (field = chosenFields; field != NULL; field = field->next)
    {
    if (field != chosenFields)
        hPrintf("\t");
    hPrintf("%s", field->name);
    }
hPrintf("\n");

if (fullGenomeRegion())
    {
    for (bed = ct->bedList; bed != NULL; bed = bed->next)
        {
	if (bf == NULL)  // or passes filter
	    {
	    tabBedRow(bed, chosenFields);
	    gotResults = TRUE;
	    }
	}
    }
else
    {
    for (region = getRegions(); region != NULL; region = region->next)
	{
	for (bed = ct->bedList; bed != NULL; bed = bed->next)
	    {
	    if (sameString(bed->chrom, region->chrom))
		{
		if (region->end == 0 || 
		   (bed->chromStart < region->end && bed->chromEnd > region->start))
		    {
		    if (bf == NULL)  // or passes filter
			{
			tabBedRow(bed, chosenFields);
			gotResults = TRUE;
			}
		    }
		}
	    }
	}
    }
if (!gotResults)
    hPrintf("# No results returned from query.\n");
}

