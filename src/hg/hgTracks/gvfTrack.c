/* gvfTrack.c - display variants from GVF (http://www.sequenceontology.org/gvf.html) */

#include "common.h"
#include "hgTracks.h"
#include "imageV2.h"
#include "hdb.h"
#include "bed8Attrs.h"

static Color gvfColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Color item by var_type attribute, according to Deanna Church's document
 * SvRepresentation2.doc attached to redmine #34. */
{
struct bed8Attrs *gvf = item;
Color dbVarUnknown = hvGfxFindColorIx(hvg, 0xb2, 0xb2, 0xb2);
int ix = stringArrayIx("var_type", gvf->attrTags, gvf->attrCount);
if (ix < 0)
    return dbVarUnknown;
char *varType = gvf->attrVals[ix];
if (sameString(varType, "CNV"))
    return MG_BLACK;
else if (sameString(varType, "Gain"))
    return hvGfxFindColorIx(hvg, 0x00, 0x00, 0xff);
else if (sameString(varType, "Loss"))
    return hvGfxFindColorIx(hvg, 0xff, 0x00, 0x00);
else if (sameString(varType, "Insertion"))
    return hvGfxFindColorIx(hvg, 0xff, 0xcc, 0x00);
else if (sameString(varType, "Complex"))
    return hvGfxFindColorIx(hvg, 0x99, 0xcc, 0xff);
else if (sameString(varType, "Unknown"))
    return dbVarUnknown;
else if (sameString(varType, "Other"))
    return hvGfxFindColorIx(hvg, 0xcc, 0x99, 0xff);
else if (sameString(varType, "Inversion"))
    return hvGfxFindColorIx(hvg, 0x99, 0x33, 0xff); // Needs pattern
else if (sameString(varType, "LOH"))
    return hvGfxFindColorIx(hvg, 0x00, 0x00, 0xff); // Needs pattern
else if (sameString(varType, "Everted"))
    return hvGfxFindColorIx(hvg, 0x66, 0x66, 0x66); // Needs pattern
else if (sameString(varType, "Transchr"))
    return hvGfxFindColorIx(hvg, 0xb2, 0xb2, 0xb2); // Plus black vert. bar at broken end
else if (sameString(varType, "UPD"))
    return hvGfxFindColorIx(hvg, 0x00, 0xff, 0xff); // Needs pattern
return dbVarUnknown;
}

// Globals used for sorting:
static struct hash *nameHash = NULL;

static char *getAttributeVal(const struct bed8Attrs *gvf, char *tag)
/* Return value corresponding to tag or NULL.  Don't free result. */
{
int ix = stringArrayIx(tag, gvf->attrTags, gvf->attrCount);
if (ix >= 0)
    return(gvf->attrVals[ix]);
return NULL;
}

static int gvfHierCmp(const void *va, const void *vb)
/* Sort GVF so that children follow their parents, and children of the same parent 
 * are sorted by var_type -- otherwise things are sorted by size,chromStart,name. */
{
const struct bed8Attrs *a = *((struct bed8Attrs **)va);
const struct bed8Attrs *b = *((struct bed8Attrs **)vb);
char *aParentName = getAttributeVal(a, "Parent");
char *bParentName = getAttributeVal(b, "Parent");
if (bParentName != NULL && sameString(bParentName, a->name))
    return -1;
else if (aParentName != NULL && sameString(aParentName, b->name))
    return 1;
else if (aParentName != NULL && bParentName != NULL && sameString(aParentName, bParentName))
    return strcmp(getAttributeVal(a, "var_type"), getAttributeVal(b, "var_type"));
else
    {
    const struct bed8Attrs *bedCmpA = a, *bedCmpB = b;
    if (aParentName != NULL)
	bedCmpA = hashFindVal(nameHash, aParentName);
    if (bParentName != NULL)
	bedCmpB = hashFindVal(nameHash, bParentName);
    int diff = 0;
    if (bedCmpA != NULL && bedCmpB != NULL)
	{
	// no need to compare chrom here
	diff = ((bedCmpB->chromEnd - bedCmpB->chromStart) -
		(bedCmpA->chromEnd - bedCmpA->chromStart));
	if (diff == 0)
	    diff = bedCmpA->chromStart - bedCmpB->chromStart;
	}
    if (diff == 0)
	diff = strcmp(aParentName, bParentName);
    return diff;
    }
}

static void gvfLoad(struct track *tg)
/* Load GVF items from a bed8Attrs database table. */
{
struct sqlConnection *conn = hAllocConn(database);
int rowOffset;
struct sqlResult *sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL, &rowOffset);
char **row;
struct bed8Attrs *list = NULL;
// if we someday get dense GVF tracks, might consider upping the default hash size:
if (nameHash == NULL)
    nameHash= hashNew(0);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct bed8Attrs *gvf = bed8AttrsLoad(row+rowOffset);
    slAddHead(&list, gvf);
    hashAdd(nameHash, gvf->name, gvf);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slSort(&list, gvfHierCmp);
tg->items = list;
}

void gvfMethods(struct track *tg)
/* Load GVF variant data. */
{
bedMethods(tg);
tg->canPack = TRUE;
tg->loadItems = gvfLoad;
tg->itemColor = gvfColor;
tg->nextPrevExon = simpleBedNextPrevEdge;
}
