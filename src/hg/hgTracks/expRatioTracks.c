/* expRatioTracks - Tracks that display microarray results as ratios. */

#include "common.h"
#include "obscure.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "cheapcgi.h"
#include "expRecord.h"
#include "microarray.h"
#include "spaceSaver.h"

Color shadesOfLowe1[10+1];
Color shadesOfLowe2[10+1];
Color shadesOfLowe3[10+1];

struct simpleClinical 
    {
    char *er;
    char *pr;
    };

char *erFilter = UCSF_DEMO_ER_DEFAULT;
char *prFilter = UCSF_DEMO_PR_DEFAULT;
struct hash *clinicalHash;

struct hash *getClinicalData(char *tableName)
{
struct hash *ret = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
struct simpleClinical *clinicalItem = NULL;

ret = newHash(16);
safef(query, sizeof(query), "select id, er, pr from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(clinicalItem);
    clinicalItem->er = cloneString(row[1]);
    clinicalItem->pr = cloneString(row[2]);
    hashAdd(ret, cloneString(row[0]), clinicalItem);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}


static void loadFiltersAndHash(char *tableName)
{
erFilter = cartUsualString(cart, UCSF_DEMO_ER, UCSF_DEMO_ER_DEFAULT);
prFilter = cartUsualString(cart, UCSF_DEMO_PR, UCSF_DEMO_PR_DEFAULT);
clinicalHash = getClinicalData(tableName);
}

boolean ucsfdemoMatch(char *thisER, char *thisPR)
{
if (sameString(erFilter, "no filter") && sameString(prFilter, "no filter")) return TRUE;
if (sameString(erFilter, "no filter") && sameString(prFilter, thisPR)) return TRUE;
if (sameString(erFilter, thisER) && sameString(prFilter, "no filter")) return TRUE;
if (sameString(erFilter, thisER) && sameString(prFilter, thisPR)) return TRUE;

return FALSE;
}

int packCountRowsUCSFDemo(struct track *tg, int maxCount, 
			  boolean withLabels, boolean allowOverflow)
/* Return packed height. */
/* Cloned from packCountRowsOverflow() in hgTracks.c. */
{
struct simpleClinical *clinicalItem;
struct spaceSaver *ss;
struct slList *item;
MgFont *font = tl.font;
int extraWidth = tl.mWidth * 2;
long long start, end;
double scale = (double)insideWidth/(winEnd - winStart);
spaceSaverFree(&tg->ss);
ss = tg->ss = spaceSaverNew(0, insideWidth, maxCount);
loadFiltersAndHash("phenBreastTumors");
for (item = tg->items; item != NULL; item = item->next)
    {
    int baseStart = tg->itemStart(tg, item);
    int baseEnd = tg->itemEnd(tg, item);
    char *name = tg->itemName(tg, item);	
    struct hashEl *hel = hashLookup(clinicalHash, name);
    if (hel == NULL) continue;
    clinicalItem = (struct simpleClinical *)hel->val;
    if (!ucsfdemoMatch(clinicalItem->er, clinicalItem->pr)) continue;
    if (baseStart < winEnd && baseEnd > winStart)
        {
	if (baseStart <= winStart)
	    start = 0;
	else
	    start = round((double)(baseStart - winStart)*scale);
	if (!tg->drawName && withLabels)
	    start -= mgFontStringWidth(font,
				       tg->itemName(tg, item)) + extraWidth;
	if (baseEnd >= winEnd)
	    end = insideWidth;
	else
	    end = round((baseEnd - winStart)*scale);
	if (start < 0) start = 0;
	if (spaceSaverAddOverflow(ss, start, end, item, allowOverflow) == NULL)
	    break;
	}
    }
spaceSaverFinish(ss);
return ss->rowCount;
}

int ucsfdemoTotalHeight(struct track *tg, enum trackVisibility vis)
{
int rows = 0;
double maxHeight = maximumTrackHeight(tg);

loadFiltersAndHash("phenBreastTumors");

switch (vis)
    {
    case tvFull:
        {
        struct slList *item;
        struct simpleClinical *clinicalItem;
        for (item = tg->items; item != NULL; item = item->next)
            {
            char *name = tg->itemName(tg, item);	
            struct hashEl *hel = hashLookup(clinicalHash, name);
            if (hel != NULL)
                {
                clinicalItem = (struct simpleClinical *)hel->val;
	        if (ucsfdemoMatch(clinicalItem->er, clinicalItem->pr))
	            rows++;
                }
            }
	}
	break;
    case tvPack:
	{
	rows = packCountRowsUCSFDemo(tg, floor(maxHeight/tg->lineHeight)+1, TRUE, FALSE);
	break;
	}
    case tvSquish:
        {
	tg->heightPer = tg->heightPer/2;
	if ((tg->heightPer & 1) == 0)
	    tg->heightPer -= 1;
	tg->lineHeight = tg->heightPer + 1;
	rows = packCountRowsUCSFDemo(tg, floor(maxHeight/tg->lineHeight)+1, FALSE, FALSE);
	break;
	}
    case tvDense:
    default:
        rows = 1;
	break;
    }
tg->height = rows * tg->lineHeight;
return tg->height;
}



void mapBoxHcTwoItems(struct hvGfx *hvg, int start, int end, int x, int y, int width, int height, 
	char *track, char *item1, char *item2, char *statusLine)
/* Print out image map rectangle that would invoke the htc (human track click)
 * program. */
{
char *encodedItem1 = cgiEncode(item1);
char *encodedItem2 = cgiEncode(item2);
x = hvGfxAdjXW(hvg, x, width);
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x, y, x+width, y+height);
hPrintf("HREF=\"%s&o=%d&t=%d&g=%s&i=%s&i2=%s&c=%s&l=%d&r=%d&db=%s&pix=%d\" ", 
       hgcNameAndSettings(), start, end, track, encodedItem1, encodedItem2,chromName, winStart, winEnd, 
       database, tl.picWidth);
hPrintf("TITLE=\"%s\">\n", statusLine); 
freeMem(encodedItem1);
freeMem(encodedItem2);
}

int affyUclaNormIndexForName(char *string)
/* Return the index in sorting as provided by Allen Day at UCLA. */
{
int i = 0;
char *tissues[] = {"adipose tissue", "abdominal aorta", "cartilage",
		   "ligament", "tendon", "adrenal gland", "pancreas",
		   "thyroid gland", "pituitary gland", "bone marrow",
		   "lymph node", "spleen", "thymus", "mammary gland",
		   "body skin", "cardiac muscle", "skeletal muscle",
		   "brain", "thalamus", "telencephalon", "amygdala",
		   "caudate nucleus", "cerebral cortex", "frontal cortex",
		   "occipital cortex", "parietal cortex", "temporal cortex",
		   "auditory cortex", "cerebral white matter", "corpus callosum",
		   "cerebellum", "spinal cord", "colon", "small intestine",
		   "ileum", "intestine mucosa", "salivary gland",
		   "oesophagus", "stomach", "liver", "kidney", "ureter",
		   "urinary bladder", "ovary", "placenta", "uterus",
		   "prostate gland", "testis", "lung", "trachea", "monocyte",
		   "chondrocyte", "granulocyte"};

for(i = 0; i < ArraySize(tissues); i++)
    {
    if(sameWord(tissues[i], string))
	return i;
    }
/* If no match. */
/* warn("Can't match %s in affyUclaNorm.", string); */
return -1;
}

void lfsMapItemName(struct track *tg, struct hvGfx *hvg, void *item, char *itemName, char *mapItemName, int start, int end, 
		    int x, int y, int width, int height)
{
if(tg->visibility != tvDense && tg->visibility != tvHide)
    mapBoxHcTwoItems(hvg, start, end, x,y, width, height, tg->mapName, itemName, itemName, itemName);
}


struct linkedFeaturesSeries *lfsFromMsBedSimple(struct bed *bedList, char *name)
/* create a lfs containing all beds on a single line */
{
struct linkedFeaturesSeries *lfs = NULL, *lfsList = NULL;
struct linkedFeatures *lf;
struct bed *bed = NULL;

if(bedList == NULL)
    return NULL;

for(bed = bedList; bed != NULL; bed = bed->next)
    {
    AllocVar(lfs);
    lfs->name = cloneString(bed->name);
    lf = lfFromBed(bed);
    /* lf->tallStart = bed->chromStart;
       lf->tallEnd = bed->chromEnd; */

    /* get overall score from bed15 */
    lfs->grayIx = bed->score;
    lf->score = bed->score;

    lfs->start = lf->start;
    lfs->end = lf->end;
    slAddHead(&lfs->features, lf);  
    slAddHead(&lfsList, lfs);
    }
return lfsList;
}

void expRecordMapTypes(struct hash *expIndexesToNames, struct hash *indexes, int *numIndexes, 
		       struct expRecord *erList,  int index, char *filter, int filterIndex)
/* creates two hashes which contain a mapping from 
   experiment to type and from type to lists of experiments */
{
struct expRecord *er = NULL;
struct slRef *val=NULL;
struct slInt *sr=NULL, *srList = NULL;
struct hash *seen = newHash(2);
char buff[256];
int unique = 0;
for(er = erList; er != NULL; er = er->next)
    {
    if ((filterIndex == -1) || (sameString(filter, er->extras[filterIndex])))
        {
	char *name;
	if (index >= 0 && er->numExtras > index)
	    name = er->extras[index];
	else
	    name = er->name;
	val = hashFindVal(seen, name);
	if (val == NULL)
	    {
	    /* if this type is new 
	       save the index for this type */
	    AllocVar(val);
	    snprintf(buff, sizeof(buff), "%d", unique);
	    hashAdd(expIndexesToNames, buff, name);
	    val->val = cloneString(buff);
	    hashAdd(seen, name, val);

	    /* save the indexes associated with this index */
	    AllocVar(sr);
	    sr->val = er->id;
	    hashAdd(indexes, buff, sr);
	    unique++;
	    }
	else
	    {
	    /* if this type has been seen before 
	       tack the new index on the end of the list */
	    AllocVar(sr);
	    srList = hashMustFindVal(indexes, val->val);
	    sr->val = er->id;
	    slAddTail(&srList,sr);
	    }
	}
    }

hashTraverseVals(seen, freeMem);
hashFree(&seen);
*numIndexes = unique;
}

int lfsSortByPos(const void *va, const void *vb)    
/* used for slSorting linkedFeaturesSeries */
{
const struct linkedFeaturesSeries *a = *((struct linkedFeaturesSeries **)va);
const struct linkedFeaturesSeries *b = *((struct linkedFeaturesSeries **)vb);
int diff = a->start - b->start;
if(diff == 0)
    diff = a->end - b->end;
if(diff == 0)
    diff = affyUclaNormIndexForName(a->name) -  affyUclaNormIndexForName(b->name);
return diff;
}

int lfsSortByName(const void *va, const void *vb)    
/* used for slSorting linkedFeaturesSeries */
{
const struct linkedFeaturesSeries *a = *((struct linkedFeaturesSeries **)va);
const struct linkedFeaturesSeries *b = *((struct linkedFeaturesSeries **)vb);
return(strcmp(a->name, b->name));
}

int nci60LfsSortByName(const void *va, const void *vb)    
/* used for slSorting linkedFeaturesSeries */
{
const struct linkedFeaturesSeries *a = *((struct linkedFeaturesSeries **)va);
const struct linkedFeaturesSeries *b = *((struct linkedFeaturesSeries **)vb);
/* make sure that the duplicate and nsclc end up at the end */
if(sameString(a->name, "DUPLICATE"))
    return 1;
if(sameString(a->name, "NSCLC"))
    return 1;
if(sameString(b->name, "DUPLICATE"))
    return -1;
if(sameString(b->name, "NSCLC"))
    return -1;
return(strcmp(a->name, b->name));
}

struct linkedFeaturesSeries *msBedGroupByIndex(struct bed *bedList, char *database, 
	char *table, int expIndex, char *filter, int filterIndex) 
/* Groups bed expScores in multiple scores bed by the expIndex 
 * in the expRecord->extras array. Makes use of hashes to remember 
 * numerical index of experiments, as hard to do in a list. 
 * If expIndex is -1, then use name instead of extras[expIndex] */
{
struct linkedFeaturesSeries *lfsList = NULL, **lfsArray;
struct linkedFeatures *lf = NULL;
struct sqlConnection *conn;
struct hash *indexes;
struct hash *expTypes;
struct hash *expIndexesToNames;
int numIndexes = 0, currentIndex, i;
struct expRecord *erList = NULL, *er=NULL;
struct slInt *srList = NULL, *sr=NULL;
char buff[256];
struct bed *bed;

/* traditionally if there is nothing to show
   show nothing .... */
if(bedList == NULL)
    return NULL;

/* otherwise if we're goint to do some filtering
   set up the data structures */
conn = sqlConnect(database);
indexes = newHash(6);
expTypes = newHash(6);
expIndexesToNames = newHash(6);

/* load the experiment information */
snprintf(buff, sizeof(buff), "select * from %s order by id asc", table);
erList = expRecordLoadByQuery(conn, buff);
if(erList == NULL)
    errAbort("hgTracks::msBedGroupByIndex() - can't get any records for %s in table %s\n", buff, table);
sqlDisconnect(&conn);

/* build hash to map experiment ids to types */
for(er = erList; er != NULL; er = er->next)
    {
    char *name;
    if (expIndex >= 0 && er->numExtras > expIndex)
        name = er->extras[expIndex];
    else
        name = er->name;
    snprintf(buff, sizeof(buff), "%d", er->id);
    hashAdd(expTypes, buff, name);
    }
/* get the number of indexes and the experiment values associated
   with each index */
expRecordMapTypes(expIndexesToNames, indexes, &numIndexes, erList, expIndex, filter, filterIndex);
if(numIndexes == 0)
    errAbort("hgTracks::msBedGroupByIndex() - numIndexes can't be 0");
lfsArray = needMem(sizeof(struct linkedFeaturesSeries*) * numIndexes);
/* initialize our different tissue linkedFeatureSeries) */
for(i=0;i<numIndexes;i++)
    {    
    char *name=NULL;
    AllocVar(lfsArray[i]);
    snprintf(buff, sizeof(buff), "%d", i);
    name = hashMustFindVal(expIndexesToNames, buff);	
    lfsArray[i]->name = cloneString(name);
    }
/* for every bed we need to group together the tissue specific
 scores in that bed */
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    /* for each tissue we need to average the scores together */
    for(i=0; i<numIndexes; i++) 
	{
	float aveScores = 0;
	int aveCount =0;

	/* get the indexes of experiments that we want to average 
	 in form of a slRef list */
	snprintf(buff, sizeof(buff), "%d", i);
	srList = hashMustFindVal(indexes, buff);
	currentIndex = srList->val;

	/* create the linked features */
	lf = lfFromBed(bed);

	/* average the scores together to get the ave score for this
	   tissue type */
	for(sr = srList; sr != NULL; sr = sr->next)
	    {
	    currentIndex = sr->val;
	    if( bed->expScores[currentIndex] != -10000) 
		{
		aveScores += bed->expScores[currentIndex];
		aveCount++;
		}
	    }

	/* if there were some good values do the average 
	   otherwise mark as missing */
	if(aveCount != 0)
	    lf->score = aveScores/aveCount;
	else
	    lf->score = -10000;
	
	/* add this linked feature to the correct 
	   linkedFeaturesSeries */
	slAddHead(&lfsArray[i]->features, lf);
	}
    }
/* Summarize all of our linkedFeatureSeries in one linkedFeatureSeries list */
for(i=0; i<numIndexes; i++)
    {
    slAddHead(&lfsList, lfsArray[i]);
    }

hashTraverseVals(indexes, freeMem);
expRecordFreeList(&erList);
freeHash(&indexes);
freeHash(&expTypes);
freeHash(&expIndexesToNames);
slReverse(&lfsList);
return lfsList;
}

static void lfsFromAffyBed(struct track *tg)
/* filters the bedList stored at tg->items
into a linkedFeaturesSeries as determined by
filter type */
{
struct bed *bedList = NULL;
char varName[128];
char *affyMap;
enum affyOptEnum affyType;

bedList = tg->items;
safef(varName, sizeof(varName), "%s.%s", tg->mapName, "type");
affyMap = cartUsualString(cart, varName, affyEnumToString(affyTissue));
affyType = affyStringToEnum(affyMap);
if(tg->limitedVis == tvDense)
    {
    tg->items = lfsFromMsBedSimple(bedList, "Affymetrix");
    }
else if(affyType == affyTissue)
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", tg->expTable, affyTissue, NULL, -1);
    slSort(&tg->items,lfsSortByName);
    }
else if(affyType == affyId)
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", tg->expTable, affyId, NULL, -1);
    }
else if(affyType == affyChipType)
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", tg->expTable, affyChipType, NULL, -1);
    }
else
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", tg->expTable, affyAllData, affyMap, 1);
    slSort(&tg->items,lfsSortByName);
    }
bedFreeList(&bedList);
}

static void lfsFromBedAndGrouping(struct track *tg, struct maGrouping *combineGroup)
/* This is sort of a replacement of msBedGroupByIndex. */
/* It's meant to be the default microarray track filter */
/* for tracks using the new microarrayGroups.ra scheme. */
{
struct linkedFeaturesSeries *lfsList = NULL;
struct bed *bedList = tg->items;
int i;
int numRows = combineGroup->numGroups;
char newLongLabel[512];
if (bedList && (bedList->expCount != combineGroup->size))
    errAbort("Error: %s grouping has bad size (%d).  Expected %d", combineGroup->name, combineGroup->size, bedList->expCount);
maBedClumpGivenGrouping(bedList, combineGroup);
/* Initialize the lfs array first. */
for (i = 0; i < numRows; i++)
    {
    struct linkedFeaturesSeries *lfs;
    struct bed *bed;
    char customName[16];
    safef(customName, sizeof(customName), "Array %d", i+1);
    AllocVar(lfs);
    if (combineGroup == NULL)
	lfs->name = cloneString(customName);
    else
	lfs->name = cloneString(combineGroup->names[i]);
    for (bed = bedList; bed != NULL; bed = bed->next)
	{
	struct linkedFeatures *lf = lfFromBed(bed);
	lf->score = bed->expScores[i];
	slAddHead(&lfs->features, lf);
	}
    slReverse(&lfs->features);
    slAddHead(&lfsList, lfs);
    }
slReverse(&lfsList);
tg->items = lfsList;
/* Change the longLabel */
if (!sameString(combineGroup->description, "Custom Track"))
    {
    safef(newLongLabel, sizeof(newLongLabel), "%s - %s", tg->longLabel, combineGroup->description);
    tg->longLabel = cloneString(newLongLabel);
    }
}

void lfsFromExpRatio(struct track *tg)
/* Make the lfs out of the track by calling lfsFromBedAndGrouping */
{
struct customTrack *ct = tg->customPt;
struct maGrouping *grouping = NULL;
if (ct != NULL)
    grouping = maGetGroupingFromCt(ct);
else
    {
    struct microarrayGroups *groups = maGetTrackGroupings(database, tg->tdb);    
    grouping = maCombineGroupingFromCart(groups, cart, tg->mapName);
    }
lfsFromBedAndGrouping(tg, grouping);
}

void lfsFromAffyUclaNormBed(struct track *tg)
/* filters the bedList stored at tg->items
into a linkedFeaturesSeries as determined by
filter type */
{
struct linkedFeaturesSeries *lfsList = NULL, *lfs = NULL, *lfsNew = NULL;
struct linkedFeatures *lf = NULL, *lfNext = NULL;
struct bed *bedList= NULL;
enum trackVisibility vis = tg->visibility;
bedList = tg->items;

if(tg->limitedVis == tvDense || tg->limitedVis == tvPack || tg->limitedVis == tvSquish)
    {
    tg->items = lfsFromMsBedSimple(bedList, "Affymetrix");
    }
else 
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "affyUclaNormExps", 2, NULL, -1);
    }

/* Unroll the series into individual linked features, we're going to
   use the packing code to piece them toghether.  At some point this
   track should be moved to just using the linkedFeature data type
   instead of the linkedFeaturesSeries data type, but for backward
   compatability we are still using linkedFeaturesSeries.  Performance
   implementation should be minimal as we only hit this code when
   there are less than 10 items in window.
*/
if(tg->limitedVis == tvFull) 
    {
    for(lfs = tg->items; lfs != NULL; lfs = lfs->next)
	{
	for(lf = lfs->features; lf != NULL; lf = lfNext)
	    {
	    lfNext = lf->next;
	    lfsNew  = CloneVar(lfs);
	    lfsNew->name = cloneString(lfs->name);
	    lfsNew->features = NULL;
	    lfsNew->next = NULL;
	    lfsNew->start = lf->start;
	    lfsNew->end = lf->end;
	    slAddHead(&lfsNew->features, lf);
	    slAddHead(&lfsList, lfsNew);
	    }
	}
    slReverse(&lfsList);
    tg->items = lfsList;
    /* Set a flag to indicate that ratio colors should be used. */
    tg->customInt = 1;
    slSort(&tg->items,lfsSortByPos);
    }
if(vis == tvFull)
    tg->visibility = tvPack;
limitVisibility(tg);
if(vis == tvFull)
    tg->visibility = tvFull;
bedFreeList(&bedList);
}

static void lfsFromAffyGenericBed(struct track *tg)
/* filters the bedList stored at tg->items
 * into a linkedFeaturesSeries as determined by
 * filter type */
{
struct bed *bedList= NULL;

bedList = tg->items;
if (tg->limitedVis == tvDense)
    {
    tg->items = lfsFromMsBedSimple(bedList, "Affymetrix");
    }
else if (tg->limitedVis == tvPack || tg->limitedVis == tvSquish)
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", tg->expTable, affyTissue, NULL, -1);
    }
else 
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", tg->expTable, -1, NULL, -1);
    }
bedFreeList(&bedList);
}

void lfsFromNci60Bed(struct track *tg)
/* filters the bedList stored at tg->items
into a linkedFeaturesSeries as determined by
filter type */
{
struct bed *bedList= NULL;
char *nci60Map = cartUsualString(cart, "nci60.type", nci60EnumToString(0));
enum nci60OptEnum nci60Type = nci60StringToEnum(nci60Map);
bedList = tg->items;

if(tg->limitedVis == tvDense)
    {
    tg->items = lfsFromMsBedSimple(bedList, "NCI 60");
    }
else if(nci60Type == nci60Tissue)
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "nci60Exps", 1, NULL, -1);
    slSort(&tg->items,nci60LfsSortByName);
    }
else if(nci60Type == nci60All)
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "nci60Exps", 0, NULL, -1);
    }
else
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "nci60Exps", 0, nci60Map, 1);
    slSort(&tg->items,lfsSortByName);
    }
bedFreeList(&bedList);
}

struct bed *rosettaFilterByExonType(struct bed *bedList)
/* remove beds from list depending on user preference for 
   seeing confirmed and/or predicted exons */
{
struct bed *bed=NULL, *tmp=NULL, *tmpList=NULL;
char *exonTypes = cartUsualString(cart, "rosetta.et", rosettaExonEnumToString(0));
enum rosettaExonOptEnum et = rosettaStringToExonEnum(exonTypes);

if(et == rosettaAllEx)
    return bedList;

/* go through and remove appropriate beds */
for(bed = bedList; bed != NULL; )
    {
    if(et == rosettaConfEx)
	{
	tmp = bed->next;
	if(bed->name[strlen(bed->name) -2] == 't')
	    slSafeAddHead(&tmpList, bed);
	else
	    bedFree(&bed);
	bed = tmp;
	}
    else if(et == rosettaPredEx)
	{
	tmp = bed->next;
	if(bed->name[strlen(bed->name) -2] == 'p')
	    slSafeAddHead(&tmpList, bed);
	else
	    bedFree(&bed);
	bed = tmp;
	}
    }
slReverse(&tmpList);
return tmpList;
}

void lfsFromRosettaBed(struct track *tg)
/* filters the bedList stored at tg->items
into a linkedFeaturesSeries as determined by
filter type */
{
struct linkedFeaturesSeries *lfsList = NULL;
struct bed *bedList= NULL;
char *rosettaMap = cartUsualString(cart, "rosetta.type", rosettaEnumToString(0));
enum rosettaOptEnum rosettaType = rosettaStringToEnum(rosettaMap);
bedList = tg->items;

bedList = rosettaFilterByExonType(bedList);

/* determine how to display the experiments */
if(tg->limitedVis == tvDense)
    {
    tg->items = lfsFromMsBedSimple(bedList, "Rosetta");
    }
else if(rosettaType == rosettaAll)
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "rosettaExps", 0, NULL, -1);
    }
else if(rosettaType == rosettaPoolOther)
    {
    lfsList = msBedGroupByIndex(bedList, "hgFixed", "rosettaExps", 1, NULL, -1);
    lfsList->name=cloneString("Common Reference");
    lfsList->next->name=cloneString("Other Exps");
    tg->items = lfsList;
    }
else 
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "rosettaExps", 0, rosettaMap, 1);
    }    
bedFreeList(&bedList);
}

void lfsFromCghNci60Bed(struct track *tg)
{
struct bed *bedList= NULL;
char *cghNci60Map = cartUsualString(cart, "cghNci60.type", cghoeEnumToString(0));
enum cghNci60OptEnum cghNci60Type = cghoeStringToEnum(cghNci60Map);

bedList = tg->items;
if(tg->limitedVis == tvDense)
    {
    tg->items = lfsFromMsBedSimple(bedList, "CGH NCI 60");
    }
else if (cghNci60Type == cghoeTissue)
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "cghNci60Exps", 1, NULL, -1);
    }
else if (cghNci60Type == cghoeAll)
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "cghNci60Exps", 0, NULL, -1);
    }
else
    {
    tg->items = msBedGroupByIndex(bedList, "hgFixed", "cghNci60Exps", 0, cghNci60Map, 1);
    }
bedFreeList(&bedList);
}

struct linkedFeaturesSeries *lfsFromMsBed(struct track *tg, struct bed *bedList)
/* create a linkedFeatureSeries from a bed list making each
   experiment a different linkedFeaturesSeries */
{
struct linkedFeaturesSeries *lfsList = NULL, *lfs;
struct linkedFeatures *lf;
struct bed *bed = NULL;
int i=0;
if(tg->limitedVis == tvDense)
    {
    lfsList = lfsFromMsBedSimple(bedList, tg->shortLabel);
    }
else 
    {
    /* for each experiment create a linked features series */
    for(i = 0; i < bedList->expCount; i++) 
	{
	char buff[256];
	AllocVar(lfs);
	if(bedList != NULL)
	    {
	    snprintf(buff, sizeof(buff), "%d", bedList->expIds[i]);
	    lfs->name = cloneString(buff);
	    }
	else
	    lfs->name = cloneString(tg->shortLabel);
      	for(bed = bedList; bed != NULL; bed = bed->next)
	    {
	    lf = lfFromBed(bed);
	    lf->tallStart = bed->chromStart; 
	    lf->tallEnd = bed->chromEnd; 
	    lf->score = bed->expScores[i];
	    slAddHead(&lfs->features, lf);
	    }
	slReverse(&lfs->features);
	slAddHead(&lfsList, lfs);
	}
    slReverse(&lfsList);
    }
return lfsList;
}


Color cghNci60Color(struct track *tg, void *item, struct hvGfx *hvg ) 
{
struct linkedFeatures *lf = item;
float val = lf->score;
float absVal = fabs(val);
int colorIndex = 0;
float maxDeviation = 1.0;
char *colorScheme = cartUsualString(cart, "cghNci60.color", "gr");
/* colorScheme should be stored somewhere not looked up every time... */

/* Make sure colors available */
if(!exprBedColorsMade)
    makeRedGreenShades(hvg);
if(val == -10000)
    return shadesOfGray[5];

if(tg->visibility == tvDense) 
    /* True value stored as integer in score field and was multiplied by 100 */ 
    absVal = absVal/100;

/* Check on mode */
if (tg->visibility == tvFull)
    {
    maxDeviation = 0.7;
    } 
 else 
    {
    maxDeviation = 0.5;
    }

/* cap the value to be less than or equal to maxDeviation */
if(absVal > maxDeviation)
    absVal = maxDeviation;

/* project the value into the number of colors we have.  
 *   * i.e. if val = 1.0 and max is 2.0 and number of shades is 16 then index would be
 * 1 * 15 /2.0 = 7.5 = 7
 */
colorIndex = (int)(absVal * maxRGBShade/maxDeviation);
if(val < 0) 
    if (sameString(colorScheme, "gr")) 
        return shadesOfRed[colorIndex];
    else
        return shadesOfGreen[colorIndex];    
else 
    {
    if (sameString(colorScheme, "gr"))
	return shadesOfGreen[colorIndex];
    else if (sameString(colorScheme, "rg"))
        return shadesOfRed[colorIndex];
    else
	return shadesOfBlue[colorIndex];
    }
}


Color expressionScoreColor(struct track *tg, float val, struct hvGfx *hvg,
		 float denseMax, float fullMax) 
/* Does the score->color conversion for various microarray tracks */
/* NOTE: item is a linkedFeatures struct */
{
float absVal = fabs(val);
int colorIndex = 0;
float maxDeviation = 1.0;
char colorVarName[256];
boolean redGreen = TRUE;
safef(colorVarName, sizeof(colorVarName), "%s.color", tg->tdb->tableName);
if (!sameString(cartUsualString(cart, colorVarName, "redGreen"), "redGreen"))
    redGreen = FALSE;

/* if val is error value show make it gray */
if(val <= -10000)
    return shadesOfGray[5];

/* we approximate a float by storing it as an int,
   thus to bring us back to right scale divide by 1000.
   i.e. 1.27 was stored as 1270 and needs to be converted to 1.27 */
 
/* if(tg->limitedVis == tvDense) */
/*     absVal = absVal/1000; */
 
if(!exprBedColorsMade)
    makeRedGreenShades(hvg);

/* cap the value to be less than or equal to maxDeviation */
if (tg->limitedVis == tvFull || tg->limitedVis == tvPack || tg->limitedVis == tvSquish)
    maxDeviation = fullMax;
else 
    maxDeviation = denseMax;

/* cap the value to be less than or equal to maxDeviation */

if(absVal > maxDeviation)
    absVal = maxDeviation;

/* project the value into the number of colors we have.  
 * i.e. if val = 1.0 and max is 2.0 and number of shades is 16 then index would be
 * 1 * 15 /2.0 = 7.5 = 7
 */
colorIndex = (int)(absVal * maxRGBShade/maxDeviation);
if(val > 0) 
    return (redGreen ? shadesOfRed[colorIndex] : shadesOfYellow[colorIndex]); 
else 
    return (redGreen ? shadesOfGreen[colorIndex] : shadesOfBlue[colorIndex]);
}

void makeLoweShades(struct hvGfx *hvg) 
/* Allocate the  shades of Red, Green and Blue */
{
static struct rgbColor black = {0, 0, 0};
static struct rgbColor shade1 = {120, 255, 255};
static struct rgbColor shade2 = {80,200, 255};
static struct rgbColor shade3 = {0,60, 255};
hvGfxMakeColorGradient(hvg, &black, &shade1, 11, shadesOfLowe1);
hvGfxMakeColorGradient(hvg, &black, &shade2, 11, shadesOfLowe2);
hvGfxMakeColorGradient(hvg, &black, &shade3, 11, shadesOfLowe3);

}

/*For Lowe Lab arrays with M and A values*/
Color loweExpressionScoreColor(struct track *tg, float val, struct hvGfx *hvg,
		 float denseMax, float fullMax) 
/* Does the score->color conversion for various microarray tracks */
/* NOTE: item is a linkedFeatures struct */
{
float absVal = fabs(val);
int colorIndex = 0;
float maxDeviation = 1.0;
static char *colorSchemes[] = { "rg", "rb" };
static char *colorScheme = NULL;
static int colorSchemeFlag = -1;
int addednumber=10000;
/*If the value is >5000 it is an A value subtract the added number to get A
value*/
if(val>5000){ absVal=fabs(absVal-addednumber);}
/* set up the color scheme items if not done yet */
if(colorScheme == NULL)
    colorScheme = cartUsualString(cart, "exprssn.color", "rg");
if(colorSchemeFlag == -1)
    colorSchemeFlag = stringArrayIx(colorScheme, colorSchemes, ArraySize(colorSchemes));

/* if val is error value show make it gray */
if(val <= (-1*addednumber))
    return shadesOfGray[5];

/* we approximate a float by storing it as an int,
   thus to bring us back to right scale divide by 1000.
   i.e. 1.27 was stored as 1270 and needs to be converted to 1.27 */
 
if(tg->limitedVis == tvDense)
    absVal = absVal/1000;
 makeLoweShades(hvg);
if(!exprBedColorsMade)
    makeRedGreenShades(hvg);

/* cap the value to be less than or equal to maxDeviation */
if (tg->limitedVis == tvFull || tg->limitedVis == tvPack || tg->limitedVis == tvSquish)

    maxDeviation = fullMax;
else 
    maxDeviation = denseMax;

/* cap the value to be less than or equal to maxDeviation */
if(val > 5000)maxDeviation=3;
if(absVal > maxDeviation)
    absVal = maxDeviation;

/* project the value into the number of colors we have.  
 * i.e. if val = 1.0 and max is 2.0 and number of shades is 16 then index would be
 * 1 * 15 /2.0 = 7.5 = 7
 */
colorIndex = (int)(absVal * maxRGBShade/maxDeviation);
if(val > 0) 
	if(val == addednumber+1){
		return shadesOfLowe1[9];
	}
	else if(val == addednumber+2){
		return  shadesOfLowe2[9];
	}
	else if(val == addednumber+3){
		return shadesOfLowe3[9];
	}
	else{
  return shadesOfRed[colorIndex];
	 }
else 
    {
    if(colorSchemeFlag == 0)
	return shadesOfGreen[colorIndex];
    else 
	return shadesOfBlue[colorIndex];
    }
}

Color expressionColor(struct track *tg, void *item, struct hvGfx *hvg,
		 float denseMax, float fullMax) 
/* Does the score->color conversion for various microarray tracks */
{
struct linkedFeatures *lf = item;
return expressionScoreColor(tg, lf->score, hvg, denseMax, fullMax);
}

/*For Lowe Lab arrays with M and A values*/
Color loweExpressionColor(struct track *tg, void *item, struct hvGfx *hvg,
		 float denseMax, float fullMax) 
/* Does the score->color conversion for various microarray tracks */
{
struct linkedFeatures *lf = item;
return loweExpressionScoreColor(tg, lf->score, hvg, denseMax, fullMax);
}

Color nci60Color(struct track *tg, void *item, struct hvGfx *hvg)
/* Does the score->color conversion for various microarray tracks */
{
return expressionColor(tg, item, hvg, 1.0, 2.6);
}

Color getColorForAffyExpssn(float val, float max)
/* Return the correct color for a given score */
{
int colorIndex = 0;
int offset = 0;   /* really there is no dynamic range below 64 (2^6) */

if(val == -10000)
    return shadesOfGray[5];
val = fabs(val);

/* take the log for visualization */
if(val > 0)
    val = logBase2(val);
else
    val = 0;

/* scale offset down to 0 */
if(val > offset) 
    val = val - offset;
else
    val = 0;

if (max <= 0) 
    errAbort("hgTracks::getColorForAffyExpssn() maxDeviation can't be zero\n"); 
max = logBase2(max);
max = max - offset;
if(max < 0)
    errAbort("hgTracks::getColorForAffyExpssn() - Max val should be greater than 0 but it is: %g", max);
    
if(val > max) 
    val = max;
colorIndex = (int)(val * maxShade/max);
return shadesOfSea[colorIndex];
}

Color affyColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Does the score->color conversion for affymetrix arrays */
{
struct linkedFeatures *lf = item;
float score = lf->score;
if(tg->visibility == tvDense)
    score = score/10;
if(!exprBedColorsMade)
    makeRedGreenShades(hvg);
return getColorForAffyExpssn(score, 262144/16); /* 262144 == 2^18 */
}

Color affyRatioColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Does the score->color conversion for affymetrix arrays using ratios,
 * if dense do an intensity color in blue based on score value otherwise do
 * red/green display from expScores */
{
struct linkedFeatures *lf = item;
float score = lf->score;
if(!exprBedColorsMade)
    makeRedGreenShades(hvg);
if(tg->limitedVis == tvDense || tg->limitedVis == tvPack || tg->limitedVis == tvSquish)
    {
    score = score/10;
    return getColorForAffyExpssn(score, 262144/16); /* 262144 == 2^18 */
    }
else
    {
    return expressionColor(tg, item, hvg, 1.0, 3.0);
    }
}

Color expRatioColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Does the score->color conversion  */
{
struct linkedFeatures *lf;
struct linkedFeaturesSeries *lfs;
if(!exprBedColorsMade)
    makeRedGreenShades(hvg);
if(tg->visibility == tvDense)
    {
    lfs = item;
    if (trackDbSetting(tg->tdb, EXP_COLOR_DENSE))
        /* scaled by 1000.  Brighten up just a bit... */
        return expressionScoreColor(tg, lfs->grayIx * 1100, hvg, tg->expScale, 
                                                        tg->expScale);
    else
        return MG_BLACK;
    }
else
    {
    lf = item;
    return expressionColor(tg, item, hvg, tg->expScale, tg->expScale);
    }
}

Color affyUclaNormColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Does the score->color conversion for affymetrix arrays using ratios,
 * if dense do an intensity color in blue based on score value otherwise do
 * red/green display from expScores */
{
struct linkedFeatures *lf = item;
if(!exprBedColorsMade)
    makeRedGreenShades(hvg);
if(tg->customInt != 1)
    {
    return shadesOfSea[grayInRange(lf->score, 0, 1000)];
    }
else
    {
    return expressionColor(tg, item, hvg, 1.0, 3.0);
    }
}

/*For Lowe Lab arrays with M and A values*/
Color loweRatioColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Does the score->color conversion  */
{
struct linkedFeatures *lf;
struct linkedFeaturesSeries *lfs;
if(!exprBedColorsMade)
    makeRedGreenShades(hvg);
if(tg->visibility == tvDense)
    {
    lfs = item;
    if (trackDbSetting(tg->tdb, EXP_COLOR_DENSE))
        /* scaled by 1000.  Brighten up just a bit... */
        return loweExpressionScoreColor(tg, lfs->grayIx * 1100, hvg, tg->expScale, 
                                                        tg->expScale);
    else
        return MG_BLACK;
    }
else
    {
    lf = item;
    return loweExpressionColor(tg, item, hvg, tg->expScale, tg->expScale);
    }
}

void fixLfs(struct track *tg)
/* Little function called at the loadMultScoresBed and the ct one */
/* to polish the lfs up. */
{
struct linkedFeatures *lf;
struct linkedFeaturesSeries *lfs;
for(lfs = tg->items; lfs != NULL; lfs = lfs->next) 
    /* Set the beginning and end of each linkedFeaturesSeries. */
    {
    lfs->start = BIGNUM;
    lfs->end = 0;
    for(lf = lfs->features; lf != NULL; lf = lf->next) 
	{
	if(lf->start < lfs->start)
	    lfs->start = lf->start;
	if(lf->end > lfs->end)
	    lfs->end = lf->end;
	}
    }
}

void loadMultScoresBed(struct track *tg)
/* Convert bed info in window to linked feature. */
{
// FIXME: dbProfile would be a better name
struct sqlConnection *conn = hAllocConnTrack(database, tg->tdb);
struct sqlResult *sr;
char **row;
int rowOffset;
int itemCount =0;
struct bed *bedList = NULL, *bed;
enum trackVisibility vis = tg->visibility;
sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, 15);
    slAddHead(&bedList, bed);
    itemCount++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&bedList);

#ifdef NEVER
/* A lot of filters will blow up the number of items in a 
   track, limit the number of items we will do in "full" mode. */
if(vis == tvFull)
    {
    if(itemCount > 10)
	{
	vis = tg->visibility;
	tg->visibility = tvPack;
	}
    }
#endif /* NEVER */

tg->limitedVis = tg->visibility;
/* run the filter if it exists, otherwise use default */
if(tg->trackFilter != NULL)
    {
    /* let the filter do the assembly of the linkedFeaturesList */
    tg->items = bedList;
    tg->trackFilter(tg);
    }
else
    {
    /* use default behavior of one row for each experiment */
    tg->items = lfsFromMsBed(tg, bedList);
    bedFreeList(&bedList);
    }
fixLfs(tg);
/* Put back our spoofed visibility. */
tg->visibility = vis;
}

static void ctLoadMultScoresBed(struct track *tg)
/* Convert bed info in window to linked feature. */
{
struct customTrack *ct = tg->customPt;
if (!ct)
    errAbort("tg->customPt set incorrectly");
if (ct->dbTrack)
    {
    if (ct->fieldCount != 15)
	errAbort("customTrack->fieldCount != 15");
    ct->bedList = ctLoadMultScoresBedDb(ct, chromName, winStart, winEnd);
    }
tg->items = ct->bedList;
tg->trackFilter(tg);
fixLfs(tg);
}

char *rosettaName(struct track *tg, void *item)
/* Return Abbreviated rosetta experiment name */
{
struct linkedFeaturesSeries *lfs = item;
char *full = NULL;
static char abbrev[32];
char *tmp = strstr(lfs->name, "_vs_");
if(tmp != NULL) 
    {
    tmp += 4;
    full = tmp = cloneString(tmp);
    tmp = strstr(tmp, "_(");
    if(tmp != NULL)
	*tmp = '\0';
    strncpy(abbrev, full, sizeof(abbrev));
    freez(&full);
    }
else if(lfs->name != NULL) 
    {
    strncpy(abbrev, lfs->name, sizeof(abbrev));
    }
else 
    {
    strncpy(abbrev, tg->shortLabel, sizeof(abbrev));
    }
return abbrev;
}

void loadMaScoresBed(struct track *tg)
/* load up bed15 data types into linkedFeaturesSeries and then set the noLines
   flag on each one */
{
struct linkedFeaturesSeries *lfs;

loadMultScoresBed(tg);
for(lfs = tg->items; lfs != NULL; lfs = lfs->next)
    {
    lfs->noLine = TRUE;
    }
}

void expRatioDrawLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width, int height, 
	boolean withCenterLabels, MgFont *font,
	Color color, enum trackVisibility vis)
/* Because I want the labels to appear in pack mode, and make the display */
/* identical to full mode, there's this custom leftLabels function. */
{
int y = yOff;

if (sameString(tg->mapName, "CGHBreastCancerUCSF"))
    loadFiltersAndHash("phenBreastTumors");

if (isWithCenterLabels(tg))
    y += mgFontLineHeight(font);
if ((vis == tvFull) || (vis == tvPack))
    {
    struct slList *item;
    /* for some probably good reason the clipping is different in pack */
    /* mode.  This resets it to being the same as full mode. */
    if (vis == tvPack)
	{
	hvGfxUnclip(hvg);
	hvGfxSetClip(hvg, xOff, yOff, width, height);
	}
    /* Go through and print each label, no mystery here. */
    /* Apply filtering here */
    for (item = tg->items; item != NULL; item = item->next)
	{
	char *name = tg->itemName(tg, item);	
	int itemHeight = tg->itemHeight(tg, item);
	if (sameString(tg->mapName, "CGHBreastCancerUCSF"))
	    {
	    struct hashEl *hel = hashLookup(clinicalHash, name);
	    struct simpleClinical *clinicalItem;
	    if (hel != NULL)
	        {
		clinicalItem = (struct simpleClinical *)hel->val;
		if (ucsfdemoMatch(clinicalItem->er, clinicalItem->pr))
		    {
	            hvGfxTextRight(hvg, leftLabelX, y, width - 1, itemHeight, color, font, name);
	            y += itemHeight;
		    }
                }
	    }
	else
	    {
	    hvGfxTextRight(hvg, leftLabelX, y, width - 1, itemHeight, color, font, name);
	    y += itemHeight;
	    }
	}
    }
else if (vis == tvDense)
    /* In dense mode it's just the shortLabel. */
    {
    hvGfxTextRight(hvg, leftLabelX, y, width - 1, tg->lineHeight, color, font, tg->shortLabel);
    }
}

static void expRatioMapBoxes(struct track *tg, struct hvGfx *hvg, int seqStart, int seqEnd, int xOff, int yOff, int width)
/* This function makes clickable mapboxes on the browser window for a */
/* microarray track.  */
{
struct linkedFeaturesSeries *marrays;
struct linkedFeatures *probes;
double scale = scaleForWindow(width, seqStart, seqEnd);
int lineHeight = tg->lineHeight;
int y = yOff;
int nExps;
int nProbes;
int totalHeight;
marrays = tg->items;
if (!marrays || !marrays->features)
    errAbort("Somethings wrong with making the mapboxes for a microarray track.");
probes = marrays->features;
nProbes = slCount(probes);
nExps = slCount(marrays);
totalHeight = nExps * lineHeight;
if ((nProbes > MICROARRAY_CLICK_LIMIT) && 
    !(sameString(tg->mapName, "expRatioUCSFDemo") || sameString(tg->mapName, "cnvLungBroadv2")  || sameString(tg->mapName, "CGHBreastCancerUCSF")  || sameString(tg->mapName, "expBreastCancerUCSF")) )
    {
    int xOffRc = hvGfxAdjXW(hvg, xOff, insideWidth);
    hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", xOffRc, y, xOffRc+insideWidth, y+totalHeight);
    hPrintf("HREF=\"%s&g=%s&c=%s&l=%d&r=%d&db=%s&i=zoomInMore\" ", 
	    hgcNameAndSettings(), tg->mapName, chromName, winStart, winEnd, database);
    hPrintf("TITLE=\"zoomInMore\">\n");
     }
else
    {
    struct linkedFeatures *probe;
    for (probe = probes; probe != NULL; probe = probe->next)
	{
	int x1 = round((double)((int)probe->start-winStart)*scale);
	int x2 = round((double)((int)probe->end-winStart)*scale);
	int w;
	if (x1 < 0)
	    x1 = 0;
	if (x2 > insideWidth-1) 
	    x2 = insideWidth-1;
	w = x2 - x1 + 1;
	if (sameString(tg->mapName, "expRatioUCSFDemo") || sameString(tg->mapName, "cnvLungBroadv2")  || sameString(tg->mapName, "expBreastCancerUCSF"))
	    {
            struct slList *item;
            for (item = tg->items; item != NULL; item = item->next)
                {
                char *name = tg->itemName(tg, item);
                mapBoxHcTwoItems(hvg, winStart, winEnd, xOff, y, winEnd-winStart, lineHeight, tg->mapName, name, name, name); 
                y = y + lineHeight;
                }
	    break;
	    }
	else if (sameString(tg->mapName, "CGHBreastCancerUCSF"))
	    {
            struct slList *item;
	    struct hashEl *hel = NULL;
	    struct simpleClinical *clinicalItem;
            loadFiltersAndHash("phenBreastTumors");
            for (item = tg->items; item != NULL; item = item->next)
                {
                char *name = tg->itemName(tg, item);
		// look up name in clinical hash
		hel = hashLookup(clinicalHash, name);
		if (hel != NULL)
		    {
		    clinicalItem = (struct simpleClinical *)hel->val;
		    if (ucsfdemoMatch(clinicalItem->er, clinicalItem->pr))
		        {
                        mapBoxHcTwoItems(hvg, winStart, winEnd, xOff, y, winEnd-winStart, lineHeight, tg->mapName, name, name, name); 
                        y = y + lineHeight;
			}
		    }
                }
	    break;
	    }
	else 
	    mapBoxHcTwoItems(hvg, probe->start, probe->end, x1+xOff, y, w, totalHeight, tg->mapName, probe->name, probe->name, probe->name);
	}
    }
}

static void expRatioSetupPixelArrays(struct track *tg, int **pPixCountArray, 
				     float ***pPixScoreArray, double scale)
/* This makes an array that keeps track of how many items there are at */
/* a given pixel on the track. This is important technique for speeding */
/* up the track when it's zoomed out far and there's a lot of stuff */
/* being drawn. */
{
int **pixCountArray;
float **pixScoreArray;
/* Make 2 two-dimensional arrays.  Both are M x N, where M is the number */
/* of tissues or arrays, and N is the number of pixels in the window.    */
struct linkedFeaturesSeries *expLfs;
int nExps = slCount(tg->items);
int i;
if (nExps < 0)
    return;
AllocArray(pixScoreArray, nExps);
AllocArray(pixCountArray, nExps);
for (i = 0, expLfs = tg->items; (i < nExps) && (expLfs != NULL); i++, expLfs = expLfs->next)
    /* Go through each "row" in the display.  For each feature in the row, */
    /* calculate the value in the corresponding pixel and add another tally */
    /* to that pixel too. */ 
    {
    struct linkedFeatures *lfProbe;
    AllocArray(pixScoreArray[i], insideWidth);
    AllocArray(pixCountArray[i], insideWidth);
    for (lfProbe = expLfs->features; lfProbe != NULL; lfProbe = lfProbe->next)
	{
	int x1 = round((double)((int)lfProbe->start-winStart)*scale);
	int x2 = round((double)((int)lfProbe->end-winStart)*scale);
	int w, j;
	if (x1 < 0)
	    x1 = 0;
	if (x2 > insideWidth-1) 
	    x2 = insideWidth-1;
	w = x2 - x1;
	if (w == 0)
	    w = 1;
	for (j = 0; j < w; j++)
	    {
	    if ((pixCountArray[i][x1+j] == 0) || 
		(pixScoreArray[i][x1+j] == MICROARRAY_MISSING_DATA) ||
		((fabs(lfProbe->score) > fabs(pixScoreArray[i][x1+j])) && 
		 (lfProbe->score != MICROARRAY_MISSING_DATA)))
		pixScoreArray[i][x1+j] = lfProbe->score;
	    pixCountArray[i][x1+j]++;
	    }
	}
    }
/* Clean up.  I guess it's not really necessary to have all the rows of that */
/* count array since they're all the same. */
*pPixCountArray = pixCountArray[0];
for (i = 1; i < nExps; i++)
    freeMem(pixCountArray[i]);
freeMem(pixCountArray);
*pPixScoreArray = pixScoreArray;
}

void expRatioDrawItemsWithExons(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw the microarray measurements and show exons.  A cart var should be */
/* on to access this function instead of directly. */
{
double scale = scaleForWindow(width, seqStart, seqEnd);
int lineHeight = tg->lineHeight;
int y = yOff;
struct slList *item;
for (item = tg->items; item != NULL; item = item->next)
    {
    if(tg->itemColor != NULL) 
	color = tg->itemColor(tg, item, hvg);
    linkedFeaturesSeriesDrawAt(tg, item, hvg, xOff, y, scale, font, color, vis);
    y += lineHeight;
    } 
}

void expRatioDrawItems(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw the microarray measurements, and do it a lot faster than */
/* genericDrawItems would. */
{
double scale = scaleForWindow(width, seqStart, seqEnd);
int lineHeight = tg->lineHeight;
int y = yOff;
int heightPer = tg->heightPer;
float **pixScoreArray = NULL;
int *pixCountArray = NULL;
int nExps;
struct linkedFeaturesSeries *marrayList = tg->items;
int i, j;
char exonDrawCartName[512];
char *drawExons = trackDbSetting(tg->tdb, "expDrawExons");
boolean drawExonChecked = FALSE;


safef(exonDrawCartName, sizeof(exonDrawCartName), "%s.expDrawExons", tg->mapName);
drawExonChecked = cartCgiUsualBoolean(cart, exonDrawCartName, FALSE);
if (drawExons && sameWord(drawExons, "on") && drawExonChecked)
    {
    expRatioDrawItemsWithExons(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
			       font, color, vis);
    }
else
    {
    /* Create an array as large as the browser window (in pixels). */
    /* Draw the array with all the rows in pack, full, or squish. */
    if ((marrayList == NULL) || (marrayList->features == NULL))
	return;
    nExps = slCount(marrayList);
    expRatioSetupPixelArrays(tg, &pixCountArray, &pixScoreArray, scale);
    if (vis == tvDense)
	{
	/* Average the pixel scores together. */
	for (i = 0; i < insideWidth; i++)
	    {
	    if (pixCountArray[i] > 0)
		{
		float biggest = 0;
		int goodMeasures = 0;
		Color theColor;
		for (j = 0; j < nExps; j++)
		    if ((pixScoreArray[j][i] != MICROARRAY_MISSING_DATA)
			&& (fabs(pixScoreArray[j][i]) > fabs(biggest)))
			{
			goodMeasures++;
			biggest = pixScoreArray[j][i];
			}
		if (goodMeasures == 0)
		    biggest = MICROARRAY_MISSING_DATA;
		theColor = expressionScoreColor(tg, biggest, hvg, tg->expScale, tg->expScale);
		hvGfxLine(hvg, xOff + i, y, xOff + i, y + heightPer - 1, theColor);
		}
	    }
	}
    else
	{
	if (sameString(tg->mapName, "CGHBreastCancerUCSF"))
            loadFiltersAndHash("phenBreastTumors");
        struct slList *item;
	for (item = tg->items, i = 0; item != NULL && i < nExps; item = item->next, i++)
	    {
	    /* filter */
	    if (sameString(tg->mapName, "CGHBreastCancerUCSF"))
	        {
	        struct hashEl *hel = NULL;
	        struct simpleClinical *clinicalItem;
                char *name = tg->itemName(tg, item);
		// look up name in clinical hash
		hel = hashLookup(clinicalHash, name);
		if (hel != NULL)
		    {
		    clinicalItem = (struct simpleClinical *)hel->val;
		    if (!ucsfdemoMatch(clinicalItem->er, clinicalItem->pr))
		        continue;
		    }
                }

	    for (j = 0; j < insideWidth; j++)
		{
		if (pixCountArray[j] > 0)
		    {
		    Color theColor = expressionScoreColor(tg, pixScoreArray[i][j], hvg, tg->expScale, tg->expScale);
		    hvGfxLine(hvg, xOff + j, y, xOff + j, y + heightPer - 1, theColor);
		    }
		}
	    y += lineHeight;
	    }
	}
/* Make the clickable mapboxes in full or pack. */
    if ((vis == tvFull) || (vis == tvPack))
	expRatioMapBoxes(tg, hvg, seqStart, seqEnd, xOff, yOff, width);
    for (i = 0; i < nExps; i++)
	freeMem(pixScoreArray[i]);
    freeMem(pixScoreArray);
    freeMem(pixCountArray);
    }
}

void rosettaMethods(struct track *tg)
/* methods for Rosetta track using bed track */
{
linkedFeaturesSeriesMethods(tg);
tg->itemColor = nci60Color;
tg->loadItems = loadMaScoresBed;
tg->trackFilter = lfsFromRosettaBed;
tg->itemName = rosettaName;
tg->mapItem = lfsMapItemName;
tg->mapsSelf = TRUE;
}

void nci60Methods(struct track *tg)
/* set up special methods for NCI60 track and tracks with multiple
   scores in general */
{
linkedFeaturesSeriesMethods(tg);
tg->itemColor = nci60Color;
tg->loadItems = loadMaScoresBed;
tg->trackFilter = lfsFromNci60Bed ;
tg->mapItem = lfsMapItemName;
tg->mapsSelf = TRUE;
}

void cghNci60Methods(struct track *tg)
/* set up special methods for CGH NCI60 track */
{
linkedFeaturesSeriesMethods(tg);
tg->itemColor = cghNci60Color;
tg->loadItems = loadMultScoresBed;
tg->trackFilter = lfsFromCghNci60Bed;
}

void affyMethods(struct track *tg)
/* set up special methods for NCI60 track and tracks with multiple
   scores in general */
{
linkedFeaturesSeriesMethods(tg);
tg->itemColor = affyColor;
tg->loadItems = loadMaScoresBed;
tg->trackFilter = lfsFromAffyBed;
tg->mapItem = lfsMapItemName;
tg->mapsSelf = TRUE;
}

void expRatioMethods(struct track *tg)
/* Set up methods for expRatio type tracks in general. */
{
struct trackDb *tdb = tg->tdb;
char *expScale = trackDbRequiredSetting(tdb, "expScale");
linkedFeaturesSeriesMethods(tg);
tg->expScale = atof(expScale);
tg->itemColor = expRatioColor;
tg->loadItems = loadMaScoresBed;
tg->trackFilter = lfsFromAffyGenericBed;
tg->mapItem = lfsMapItemName;
tg->mapsSelf = TRUE;
}

void affyRatioMethods(struct track *tg)
/* set up special methods for NCI60 track and tracks with multiple
   scores in general */
{
expRatioMethods(tg);
tg->itemColor = affyRatioColor;
tg->trackFilter = lfsFromAffyBed;
}

void affyUclaNormMethods(struct track *tg)
/* Set up special methods for the affyUcla normal tissue track
   scores in general */
{
expRatioMethods(tg);
tg->itemColor = affyUclaNormColor;
tg->trackFilter = lfsFromAffyUclaNormBed;
}

/*For Lowe Lab arrays with M and A values*/
void loweExpRatioMethods(struct track *tg)
/* Set up methods for expRatio type tracks in general. */
{
expRatioMethods(tg);
tg->itemColor = loweRatioColor;

}

void expRatioMethodsFromDotRa(struct track *tg)
/* Special methods for tracks using new microarrayGroups.ra files. */
{
expRatioMethods(tg);
tg->drawItems = expRatioDrawItems;
tg->drawLeftLabels = expRatioDrawLeftLabels;
tg->trackFilter = lfsFromExpRatio;
if (sameString(tg->mapName, "CGHBreastCancerUCSF"))
    tg->totalHeight = ucsfdemoTotalHeight;
}

void expRatioMethodsFromCt(struct track *tg)
/* Set up methods for expRatio type tracks from custom track. */
{
expRatioMethods(tg);
tg->drawItems = expRatioDrawItems;
tg->drawLeftLabels = expRatioDrawLeftLabels;
tg->loadItems = ctLoadMultScoresBed;
tg->trackFilter = lfsFromExpRatio;
}

/* Probes tracks */

struct slList *illuminaProbesAlmostLoad(char **row)
/* Sort of an intermediary function to accommodate some */
/* general linkedFeatures loader. */
{
return (struct slList *)bedLoad12(row);
}

void illuminaProbesFree(struct slList **pItem)
/* Frees my bed. (used by loadLinkedFeaturesWithLoaders). */
{
struct bed **pBed = (struct bed **)pItem;
bedFree(pBed);
}

struct linkedFeatures *lfFromIlluminaProbesBed(struct slList *item)
/* converts illumina probes bed to linkedFeatures, and also puts */
/* that info in itemRgb in the extra field so we can use it later. */
/* (used by loadLinkedFeaturesWithLoaders). */
{
struct bed *theBed = (struct bed *)item;
struct linkedFeatures *lf = lfFromBed(theBed);
lf->extra = intToPt(theBed->itemRgb);
return lf;
}

void illuminaProbesLoadItems(struct track *tg)
/* Just call the flexible linkedFeatures loader with a few custom fns. */
{
loadLinkedFeaturesWithLoaders(tg, illuminaProbesAlmostLoad, lfFromIlluminaProbesBed,
			      illuminaProbesFree, NULL, NULL, NULL);
}

Color illuminaProbesColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to illumina item */
{
struct linkedFeatures *thisItem = item;
int colorInt = ptToInt(thisItem->extra);
if (colorInt == 1)
    return shadesOfBrown[thisItem->grayIx];
if (colorInt == 2)
    return shadesOfSea[thisItem->grayIx];
return MG_RED;
}

void illuminaProbesMethods(struct track *tg)
/* Methods for the Illumina Probes track. */
{
linkedFeaturesMethods(tg);
tg->itemColor = illuminaProbesColor;
tg->loadItems = illuminaProbesLoadItems;
}
