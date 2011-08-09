/* imageV2 - API for creating the image V2 features. */
#include "common.h"
#include "hPrint.h"
#include "chromInfo.h"
#include "hdb.h"
#include "hui.h"
#include "jsHelper.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "imageV2.h"
#include "hgTracks.h"
#include "hgConfig.h"
#include "regexHelper.h"

static char const rcsid[] = "$Id: imageV2.c,v 1.32 2010/05/24 19:53:42 hiram Exp $";

struct imgBox   *theImgBox   = NULL; // Make this global for now to avoid huge rewrite
//struct image    *theOneImg   = NULL; // Make this global for now to avoid huge rewrite
struct imgTrack *curImgTrack = NULL; // Make this global for now to avoid huge rewrite
//struct imgSlice *curSlice    = NULL; // Make this global for now to avoid huge rewrite
//struct mapSet   *curMap      = NULL; // Make this global for now to avoid huge rewrite
//struct mapItem  *curMapItem  = NULL; // Make this global for now to avoid huge rewrite

/////////////////////////
// FLAT TRACKS
// A simplistic way of flattening the track list before building the image
// NOTE: Strategy is NOT to use imgBox->imgTracks, since this should be independednt of imageV2
/////////////////////////
#define IMGORD_CUSTOM_ONTOP
void flatTracksAdd(struct flatTracks **flatTracks,struct track *track,struct cart *cart)
// Adds one track into the flatTracks list
{
struct flatTracks *flatTrack;
AllocVar(flatTrack);
flatTrack->track = track;
char var[256];  // The whole reason to do this is to reorder tracks/subtracks in the image!
safef(var,sizeof(var),"%s_%s",track->tdb->track,IMG_ORDER_VAR);
flatTrack->order = cartUsualInt(cart, var,IMG_ANYORDER);
#ifdef IMGORD_CUSTOM_ONTOP
if(flatTrack->order >= IMG_ORDERTOP)
#else///ifndef IMGORD_CUSTOM_ONTOP
if(flatTrack->order >= IMG_ORDEREND)
#endif///ndef IMGORD_CUSTOM_ONTOP
    {
    cartRemove(cart,var);
    flatTrack->order = IMG_ANYORDER;
    }
#ifdef IMGORD_CUSTOM_ONTOP
static int topOrder  = IMG_ORDERTOP; // keep track of the order added to top of image
#endif///def IMGORD_CUSTOM_ONTOP
static int lastOrder = IMG_ORDEREND; // keep track of the order added and beyond end
if( flatTrack->order == IMG_ANYORDER)
    {
#ifdef IMGORD_CUSTOM_ONTOP
    if (track->customTrack)
        flatTrack->order = ++topOrder; // Custom tracks go to top
    else
#endif///def IMGORD_CUSTOM_ONTOP
        flatTrack->order = ++lastOrder;
    }

slAddHead(flatTracks,flatTrack);
}

int flatTracksCmp(const void *va, const void *vb)
// Compare to sort on flatTrack->order
{
const struct flatTracks *a = *((struct flatTracks **)va);
const struct flatTracks *b = *((struct flatTracks **)vb);
return (a->order - b->order);
}

#ifdef IMGORD_CUSTOM_ONTOP
void flatTracksSort(struct flatTracks **flatTracks)
// This routine sorts the imgTracks then forces tight ordering, so new tracks wil go to the end
{
// flatTracks list has 2 sets of "order": those already dragReordered (below IMG_ORDERTOP)
// and those not yet reordered (above).  Within those not yet dragReordered are 2 sets:
// Those that begin numbering at IMG_ORDERTOP and those that begin at IMG_ORDEREND.
// This routine must determine if there are any already dragOrdered, and if so, position the
// newcomers in place.  Newly appearing customTracks will appear at top, while newly appearing
// standard tracks appear at the end of the image.
int haveBeenOrderd = 0, imgOrdHighest=0; // Keep track of reordered count and position
int notYetOrdered = 0, toBeTopHighest=0; // Keep track of those to be reordered, and top ordered

// First determine what if anything needs to be rearranged.
struct flatTracks *oneTrack = *flatTracks;
for(;oneTrack!=NULL;oneTrack = oneTrack->next)
    {
    if (oneTrack->order <= IMG_ORDERTOP)
        {
        haveBeenOrderd++;
        if (imgOrdHighest < oneTrack->order )
            imgOrdHighest = oneTrack->order;
        }
    else
        {
        notYetOrdered++;
        if (oneTrack->order <= IMG_ORDEREND) // && oneTrack->order >= IMG_ORDERTOP
            {
            if (toBeTopHighest < oneTrack->order )
                toBeTopHighest = oneTrack->order;
            }
        }
    }

// If some have previously been dragOrdered AND some new ones need to be given an explicit order
if (haveBeenOrderd > 0 && notYetOrdered > 0)
    {
    char var[256];
    int gapOnTopNeeded = 0;
    if (toBeTopHighest > 0)
        {
        gapOnTopNeeded = toBeTopHighest - IMG_ORDERTOP;
        imgOrdHighest += gapOnTopNeeded; // Will be after this loop
        // Warning: Will need to throw away ALL previous orderings (even those not currently in image)!
        safef(var,sizeof(var),"*_%s",IMG_ORDER_VAR);
        cartRemoveLike(cart, var);
        }
    int gapFromOrderedToEnd = (IMG_ORDEREND - imgOrdHighest);  // This difference should be removed from any with IMG_ORDEREND
    for(oneTrack = *flatTracks;oneTrack!=NULL;oneTrack = oneTrack->next)
        {
        if (oneTrack->order <= IMG_FIXEDPOS)
            ;  // Untouchables
        else if (oneTrack->order <= IMG_ORDERTOP && gapOnTopNeeded > 0)
            {  // Already order tracks will need to be pushed down.
            oneTrack->order += gapOnTopNeeded;
            safef(var,sizeof(var),"%s_%s",oneTrack->track->track,IMG_ORDER_VAR);
            cartSetInt(cart, var, oneTrack->order);
            }
        else if (oneTrack->order >= IMG_ORDERTOP
             &&  oneTrack->order <  IMG_ORDEREND && gapOnTopNeeded > 0)
            {  // Unordered custom tracks will need to be added to top!
            oneTrack->order -= IMG_ORDERTOP; // Force to top
            safef(var,sizeof(var),"%s_%s",oneTrack->track->track,IMG_ORDER_VAR);
            cartSetInt(cart, var, oneTrack->order);
            }
        else if (oneTrack->order >= IMG_ORDEREND && gapFromOrderedToEnd)
            {  // Normal unordered tracks can fill in the trailing numbers
            oneTrack->order -= gapFromOrderedToEnd;
            safef(var,sizeof(var),"%s_%s",oneTrack->track->track,IMG_ORDER_VAR);
            cartSetInt(cart, var, oneTrack->order);
            }
        }
    }

if (flatTracks && *flatTracks)
    slSort(flatTracks, flatTracksCmp);
}

#else///ifndef IMGORD_CUSTOM_ONTOP

void flatTracksSort(struct flatTracks **flatTracks)
// This routine sorts the imgTracks then forces tight ordering, so new tracks wil go to the end
{
// flatTracks list has 2 sets of "order": those already dragReordered (below IMG_ORDEREND)
// and those not yet reordered (above).  It has been decided that adding new tracks to an
// existing order should always put the new tracks below existing and treat them as if they
// were reordered there.  Thus all new tracks should get an imgOrd below IMG_ORDEREND.
// The result is turning on a successive set of new tracks will have them appear below all others.
int imgOrdSet = 0;
boolean notYetOrdered = FALSE;
struct flatTracks *oneTrack = *flatTracks;
for(;oneTrack!=NULL;oneTrack = oneTrack->next)
    {
    if (oneTrack->order <= IMG_ORDEREND
    &&  imgOrdSet < oneTrack->order )
        imgOrdSet = oneTrack->order;
    else
        notYetOrdered = TRUE;
    }
if (imgOrdSet > 0 && notYetOrdered) // Image order has previously been set, so givem all imgOrds
    {
    imgOrdSet = (IMG_ORDEREND - imgOrdSet);  // This difference should be removed from any with imgOrdSet
    for(oneTrack = *flatTracks;oneTrack!=NULL;oneTrack = oneTrack->next)
        {
        if (oneTrack->order >= imgOrdSet)
            {
            oneTrack->order -= imgOrdSet;
            char var[256];
            safef(var,sizeof(var),"%s_%s",oneTrack->track->track,IMG_ORDER_VAR);
            cartSetInt(cart, var, oneTrack->order);
            }
        }
    }

if (flatTracks && *flatTracks)
    slSort(flatTracks, flatTracksCmp);
}
#endif///ndef IMGORD_CUSTOM_ONTOP


void flatTracksFree(struct flatTracks **flatTracks)
// Frees all memory used to support flatTracks (underlying tracks are untouched)
{
if(flatTracks && *flatTracks)
    {
    struct flatTracks *flatTrack;
    while((flatTrack = slPopHead(flatTracks)) != NULL)
        freeMem(flatTrack);
    }
}

// TODO: Move to trackDb.h and trackDbCustom.c
enum kindOfParent {
    kopChildless     = 0,
    kopFolder        = 1,
    kopComposite     = 2,
    kopMultiTrack    = 3,
    kopCompositeView = 4
};
enum kindOfChild {
    kocOrphan          = 0,
    kocFolderContent   = 1,
    kocCompositeChild  = 2,
    kocMultiTrackChild = 3
};

enum kindOfParent tdbKindOfParent(struct trackDb *tdb)
{
enum kindOfParent kindOfParent = kopChildless;
if (tdbIsFolder(tdb))
    kindOfParent = kopFolder;
else if (tdbIsComposite(tdb))
    kindOfParent = kopComposite;
else if (tdbIsMultiTrack(tdb))
    kindOfParent = kopMultiTrack;
else if (tdbIsCompositeView(tdb))   // NOTE: This should not be needed in js
    kindOfParent = kopCompositeView;
return kindOfParent;
}

enum kindOfChild tdbKindOfChild(struct trackDb *tdb)
{
enum kindOfChild kindOfChild = kocOrphan;
if (tdbIsFolderContent(tdb))
    kindOfChild = kocFolderContent;
else if (tdbIsCompositeChild(tdb))
    kindOfChild = kocCompositeChild;
else if (tdbIsMultiTrackChild(tdb))
    kindOfChild = kocMultiTrackChild;
return kindOfChild;
}

/////////////////////////
// JSON support.  Eventually the whole imgTbl could be written out as JSON


static void jsonTdbSettingsInit(struct jsonHashElement *settings)
// Inititializes trackDbJson
{
struct jsonHashElement *ele = newJsonHash(newHash(8));
jsonHashAddString(ele, "shortLabel", "ruler");
jsonHashAddString(ele, "longLabel", "Base Position Controls");
jsonHashAddNumber(ele, "canPack", 0);
jsonHashAddNumber(ele, "visibility", rulerMode);
jsonHashAddString(ele, "configureBy", "popup");
jsonHashAddNumber(ele, "kindOfParent", 0);
jsonHashAdd(settings, "ruler", (struct jsonElement *) ele);
}

void jsonTdbSettingsBuild(struct jsonHashElement *settings, struct track *track, boolean configurable)
// Adds trackDb settings to the jsonTdbSettings
{
struct jsonHashElement *ele = newJsonHash(newHash(8));
jsonHashAdd(settings, track->track, (struct jsonElement *) ele);
// track name and type
jsonHashAddString(ele, "type", track->tdb->type);

// Tell which kind of parent and which kind of child
enum kindOfParent kindOfParent = tdbKindOfParent(track->tdb);
enum kindOfChild  kindOfChild  = tdbKindOfChild(track->tdb);
jsonHashAddNumber(ele, "kindOfParent", kindOfParent);
jsonHashAddNumber(ele, "kindOfChild", kindOfChild);

// Tell something about the parent and/or children
if (kindOfChild != kocOrphan)
    {
    struct trackDb *parentTdb = (kindOfChild == kocFolderContent ? track->tdb->parent :tdbGetContainer(track->tdb));

    jsonHashAddString(ele, "parentTrack", parentTdb->track);
    jsonHashAddString(ele, "parentLabel", parentTdb->shortLabel);
    if (kindOfChild != kocFolderContent && !track->canPack)
        {
        jsonHashAddNumber(ele, "shouldPack", 0); // default vis is full, but pack is an option
        track->canPack = parentTdb->canPack;
        }
    }

// XXXX really s/d be numChildren
jsonHashAddNumber(ele, "hasChildren", slCount(track->tdb->subtracks));

// Configuring?
if (!configurable || track->hasUi == FALSE)
    jsonHashAddString(ele, "configureBy", "none");
else if (sameString(trackDbSettingClosestToHomeOrDefault(track->tdb, "configureByPopup",
    regexMatch(track->track, "^snp[0-9]+") || regexMatch(track->track, "^cons[0-9]+way") || regexMatch(track->track, "^multiz") ? "off" : "on"), "off"))
    jsonHashAddString(ele, "configureBy", "clickThrough");
else
    jsonHashAddString(ele, "configureBy", "popup");

// Remote access by URL?
if (sameWord(track->tdb->type, "remote") && trackDbSetting(track->tdb, "url") != NULL)
    jsonHashAddString(ele, "url", trackDbSetting(track->tdb, "url"));

// Close with some standard vars
jsonHashAddString(ele, "shortLabel", track->shortLabel);
jsonHashAddString(ele, "longLabel", track->longLabel);
jsonHashAddNumber(ele, "canPack", track->canPack);

if(track->limitedVis != track->visibility)
    jsonHashAddNumber(ele, "limitedVis", track->limitedVis);
jsonHashAddNumber(ele, "visibility", track->visibility);
}

void jsonTdbSettingsUse(struct jsonHashElement *settings)
{
// add the settings to the hgTracks output object
jsonHashAdd(jsonForClient, "trackDb", (struct jsonElement *) settings);
}

/////////////////////////
// IMAGEv2
// The new way to do images: PLEASE REFER TO imageV2.h FOR A DETAILED DESCRIPTION
/////////////////////////



/////////////////////// Maps

struct mapSet *mapSetStart(char *name,struct image *img,char *linkRoot)
/* Starts a map (aka mapSet) which is the seet of links and image locations used in HTML.
   Complete a map by adding items with mapItemAdd() */
{
struct mapSet *map;
AllocVar(map);
return mapSetUpdate(map,name,img,linkRoot);
}

struct mapSet *mapSetUpdate(struct mapSet *map,char *name,struct image *img,char *linkRoot)
/* Updates an existing map (aka mapSet) */
{
if(name != NULL && differentStringNullOk(name,map->name))
    {
    if(map->name != NULL)
        freeMem(map->name);
    map->name = cloneString(name);
    }
if(img != NULL && img != map->parentImg)
    map->parentImg = img;
if(linkRoot != NULL && differentStringNullOk(linkRoot,map->linkRoot))
    {
    if(map->linkRoot != NULL)
        freeMem(map->linkRoot);
    map->linkRoot = cloneString(linkRoot);
    }
return map;
}

struct mapItem *mapSetItemFind(struct mapSet *map,int topLeftX,int topLeftY,int bottomRightX,int bottomRightY)
/* Find a single mapItem based upon coordinates (within a pixel) */
{
struct mapItem *item;
for(item=map->items;item!=NULL;item=item->next)
    {
    if((abs(item->topLeftX     - topLeftX)     < 2)
    && (abs(item->topLeftY     - topLeftY)     < 2)
    && (abs(item->bottomRightX - bottomRightX) < 2)
    && (abs(item->bottomRightY - bottomRightY) < 2)) // coordinates within a pixel is okay
        return item;
    }
return NULL;
}

struct mapItem *mapSetItemUpdate(struct mapSet *map,struct mapItem *item,char *link,char *title,int topLeftX,int topLeftY,int bottomRightX,int bottomRightY,char *id)
/* Update a single mapItem */
{
if(title != NULL)
    item->title = cloneString(title);
if(link != NULL)
    {
    if(map->linkRoot != NULL && startsWith(map->linkRoot,link))
        item->linkVar = cloneString(link + strlen(map->linkRoot));
    else
        item->linkVar = cloneString(link);
    }
item->topLeftX     = topLeftX;
item->topLeftY     = topLeftY;
item->bottomRightX = bottomRightX;
item->bottomRightY = bottomRightY;
freeMem(item->id);
item->id           = cloneString(id);
return item;
}

struct mapItem *mapSetItemAdd(struct mapSet *map,char *link,char *title,int topLeftX,int topLeftY,int bottomRightX,int bottomRightY, char *id)
/* Add a single mapItem to a growing mapSet */
{
struct mapItem *item;
AllocVar(item);
if(title != NULL)
    item->title = cloneString(title);
if(link != NULL)
    {
    if(map->linkRoot != NULL && startsWith(map->linkRoot,link))
        item->linkVar = cloneString(link + strlen(map->linkRoot));
    else
        item->linkVar = cloneString(link);
    }
item->topLeftX     = topLeftX;
item->topLeftY     = topLeftY;
item->bottomRightX = bottomRightX;
item->bottomRightY = bottomRightY;
item->id           = cloneString(id);
slAddHead(&(map->items),item);
//warn("Added map(%s) item '%s' count:%d",map->name,title,slCount(map->items));
return map->items;
}

struct mapItem *mapSetItemUpdateOrAdd(struct mapSet *map,char *link,char *title,int topLeftX,int topLeftY,int bottomRightX,int bottomRightY, char *id)
/* Update or add a single mapItem */
{
struct mapItem *item = mapSetItemFind(map,topLeftX,topLeftY,bottomRightX,bottomRightY);
if(item != NULL)
    return mapSetItemUpdate(map,item,link,title,topLeftX,topLeftY,bottomRightX,bottomRightY, id);
else
    return mapSetItemAdd(map,link,title,topLeftX,topLeftY,bottomRightX,bottomRightY, id);
}

struct mapItem *mapSetItemFindOrAdd(struct mapSet *map,char *link,char *title,int topLeftX,int topLeftY,int bottomRightX,int bottomRightY, char *id)
/* Finds or adds the map item */
{
struct mapItem *item = mapSetItemFind(map,topLeftX,topLeftY,bottomRightX,bottomRightY);
if(item != NULL)
    return item;
else
    return mapSetItemAdd(map,link,title,topLeftX,topLeftY,bottomRightX,bottomRightY,id);
}

void mapItemFree(struct mapItem **pItem)
/* frees all memory assocated with a single mapItem */
{
if(pItem != NULL && *pItem != NULL)
    {
    struct mapItem *item = *pItem;
    if(item->title != NULL)
        freeMem(item->title);
    if(item->linkVar != NULL)
        freeMem(item->linkVar);
    if(item->id != NULL)
        freeMem(item->id);
    freeMem(item);
    *pItem = NULL;
    }
}

boolean mapItemConsistentWithImage(struct mapItem *item,struct image *img,boolean verbose)
/* Test whether a map item is consistent with the image it is supposed to be for */
{
if ((item->topLeftX     < 0 || item->topLeftX     >= img->width)
||  (item->bottomRightX < 0 || item->bottomRightX >  img->width  || item->bottomRightX < item->topLeftX)
||  (item->topLeftY     < 0 || item->topLeftY     >= img->height)
||  (item->bottomRightY < 0 || item->bottomRightY >  img->height || item->bottomRightY < item->topLeftY))
    {
    if (verbose)
        warn("mapItem has coordinates (topX:%d topY:%d botX:%d botY:%d) outside of image (width:%d height:%d)",
             item->topLeftX,item->topLeftY,item->bottomRightX,item->bottomRightY,img->width,img->height);
    return FALSE;
    }
return TRUE;
}

static struct dyString *addIndent(struct dyString **dy,int indent)
/* beginning indent for show functions */
{
struct dyString *myDy = (dy?*dy:NULL);
if(dy == NULL || *dy == NULL)
    myDy = newDyString(256);
else
    dyStringAppend(myDy,"<br>");
dyStringAppend(myDy,"<code>");
int times = indent;
for(;times>0;times--)
    dyStringAppend(myDy,"&nbsp;");
return myDy;
}

static void mapItemShow(struct dyString **dy,struct mapItem *item,int indent)
/* show the map item */
{
if(item)
    {
    struct dyString *myDy = addIndent(dy,indent);
    dyStringPrintf(myDy,"mapItem: title:%s topX:%d topY:%d botX:%d botY:%d",
             (item->title?item->title:""),
            item->topLeftX,item->topLeftY,item->bottomRightX,item->bottomRightY);
    addIndent(&myDy,indent);
    dyStringPrintf(myDy,"&nbsp;&nbsp;linkVar:%s",
            (item->linkVar?item->linkVar:""));
    if(dy == NULL)
        warn("%s",dyStringCannibalize(&myDy));
    else
        *dy = myDy;
    }
}


static void mapShow(struct dyString **dy,struct mapSet *map,int indent)
/* show the map */
{
if(map && map->items) // No map items then why bother?
    {
    struct dyString *myDy = addIndent(dy,indent);
    dyStringPrintf(myDy,"map: name:%s",(map->name?map->name:""));
    if(map->linkRoot)
        dyStringPrintf(myDy," linkRoot:%s",map->linkRoot);
    if(dy == NULL)
        warn("%s",dyStringCannibalize(&myDy));

    indent++;
    struct mapItem *item = map->items;
    for (;item != NULL; item = item->next)
        mapItemShow(dy,item,indent);

    if(dy != NULL)
        *dy = myDy;
    }
}

boolean mapSetIsComplete(struct mapSet *map,boolean verbose)
/* Tests the completeness and consistency of this map (mapSet) */
{
if (map == NULL)
    {
    if (verbose)
        warn("map is NULL");
    return FALSE;
    }
if (map->parentImg == NULL)
    {
    if (verbose)
        warn("map is missing a parant image.");
    return FALSE;
    }
if (map->parentImg->file == NULL)
    {
    if (verbose)
        warn("map has image which has no file.");
    return FALSE;
    }
if (map->items == NULL) // This is okay
    {
    //if (verbose)
    //    warn("map(%s) has no items.",(map->name?map->name:map->parentImg->file));
    //return FALSE;
    return TRUE; // Accept this as legitimate
    }
struct mapItem *item = map->items;
for (;item != NULL; item = item->next)
    {
    if(!mapItemConsistentWithImage(item,map->parentImg,verbose))
        return FALSE;
    if(item->linkVar == NULL && map->linkRoot == NULL)
        {
        if (verbose)
            warn("item for map(%s) has no link.",(map->name?map->name:map->parentImg->file));
        return FALSE;
        }
    }
return TRUE;
}

void mapSetFree(struct mapSet **pMap)
/* frees all memory (including items) assocated with a single mapSet */
{
if(pMap != NULL && *pMap != NULL)
    {
    struct mapSet *map = *pMap;
    struct mapItem *item = NULL;
    while((item = slPopHead(&(map->items))) != NULL )
        mapItemFree(&item);
    freeMem(map->name);
    // Don't free parentImg, as it should be freed independently
    freeMem(map->linkRoot);
    freeMem(map);
    *pMap = NULL;
    }
}



/////////////////////// Images

struct image *imgCreate(char *gif,char *title,int width,int height)
/* Creates a single image container.
   A map map be added with imgMapStart(),mapSetItemAdd() */
{
struct image *img;
AllocVar(img);
if(gif != NULL)
    img->file   = cloneString(gif);
if(title != NULL)
    img->title  = cloneString(title);
img->height = height;
img->width  = width;
img->map    = NULL;
return img;
}

struct mapSet *imgMapStart(struct image *img,char *name,char *linkRoot)
/* Starts a map associated with an image. Map items can then be added to the returned pointer with mapSetItemAdd() */
{
if(img->map != NULL)
    {
    warn("imgAddMap() but map already exists.  Being replaced.");
    mapSetFree(&(img->map));
    }
img->map = mapSetStart(name,img,linkRoot);
return img->map;
}

struct mapSet *imgGetMap(struct image *img)
/* Gets the map associated with this image. Map items can then be added to the map with mapSetItemAdd() */
{
return img->map;
}

static void imgShow(struct dyString **dy,struct image *img,char *prefix,int indent)
/* show the img */
{
if(img)
    {
    struct dyString *myDy = addIndent(dy,indent);
    dyStringPrintf(myDy,"%simg: title:%s file:%s width:%d height:%d",(prefix?prefix:""),
            (img->title?img->title:""),(img->file?img->file:""),img->width,img->height);
    indent++;
    mapShow(&myDy,img->map,indent);
    if(dy == NULL)
        warn("%s",dyStringCannibalize(&myDy));
    else
        *dy = myDy;
    }
}

void imgFree(struct image **pImg)
/* frees all memory assocated with an image (including a map) */
{
if(pImg != NULL && *pImg != NULL)
    {
    struct image *img = *pImg;
    mapSetFree(&(img->map));
    freeMem(img->file);
    freeMem(img->title);
    freeMem(img);
    *pImg = NULL;
    }
}



/////////////////////// Slices

struct imgSlice *sliceCreate(enum sliceType type,struct image *img,char *title,int width,int height,int offsetX,int offsetY)
/* Creates of a slice which is a portion of an image.
   A slice specific map map be added with sliceMapStart(),mapSetItemAdd() */
{
if (height <= 0 || width <= 0)
    return NULL;
struct imgSlice *slice;
AllocVar(slice);
slice->map       = NULL; // This is the same as defaulting to slice->parentImg->map
return sliceUpdate(slice,type,img,title,width,height,offsetX,offsetY);
}

struct imgSlice *sliceUpdate(struct imgSlice *slice,enum sliceType type,struct image *img,char *title,int width,int height,int offsetX,int offsetY)
/* updates an already created slice */
{
//if(width==0 || height==0)
//    return NULL;
slice->type      = type;
if(img != NULL && slice->parentImg != img)
    slice->parentImg = img;
if(title != NULL && differentStringNullOk(title,slice->title))
    {
    if(slice->title != NULL)
        freeMem(slice->title);
    slice->title   = cloneString(title);
    }
slice->width     = width;
slice->height    = height;
slice->offsetX   = offsetX;
slice->offsetY   = offsetY;
return slice;
}

char *sliceTypeToString(enum sliceType type)
/* Translate enum slice type to string */
{
switch(type)
    {
    case stData:   return "data";
    case stSide:   return "side";
    case stCenter: return "center";
    case stButton: return "button";
    default:       return "unknown";
    }
}

static char *sliceTypeToClass(enum sliceType type)
/* Translate enum slice type to the class */
{
switch (type)
    {
    case stSide:   return "sideLab";
    case stCenter: return "cntrLab";
    case stButton: return "button";
    case stData:   return "dataImg";
    default:       return "unknown";
    }
}


struct imgSlice *sliceAddLink(struct imgSlice *slice,char *link,char *title)
/* Adds a slice wide link.  The link and map are mutually exclusive */
{
if(slice->map != NULL)
    {
    warn("sliceAddLink() but slice already has its own map. Being replaced.");
    mapSetFree(&(slice->map));
    }
if(slice->link != NULL)
    {
    warn("sliceAddLink() but slice already has a link. Being replaced.");
    freeMem(slice->link);
    }
slice->link = cloneString(link);
if(slice->title != NULL)  // OK to replace title
    freeMem(slice->title);
slice->title = cloneString(title);
return slice;
}

struct mapSet *sliceMapStart(struct imgSlice *slice,char *name,char *linkRoot)
/* Adds a slice specific map to a slice of an image. Map items can then be added to the returned pointer with mapSetItemAdd()*/
{
if(slice->parentImg == NULL)
    {
    warn("sliceAddMap() but slice has no image.");
    return NULL;
    }
if(slice->link != NULL)
    {
    warn("sliceAddMap() but slice already has a link. Being replaced.");
    freeMem(slice->link);
    slice->link = NULL;
    }
if(slice->map != NULL && slice->map != slice->parentImg->map)
    {
    warn("sliceAddMap() but slice already has its own map. Being replaced.");
    mapSetFree(&(slice->map));
    }
char qualifiedName[256];
safef(qualifiedName,sizeof(qualifiedName),"%s_%s",sliceTypeToString(slice->type),name);
slice->map = mapSetStart(qualifiedName,slice->parentImg,linkRoot);
return slice->map;
}

struct mapSet *sliceGetMap(struct imgSlice *slice,boolean sliceSpecific)
/* Gets the map associate with a slice which may be sliceSpecific or it map belong to the slices' image.
   Map items can then be added to the map returned with mapSetItemAdd() */
{
if(!sliceSpecific && slice->map == NULL && slice->parentImg != NULL)
        return slice->parentImg->map;
return slice->map;
}

struct mapSet *sliceMapFindOrStart(struct imgSlice *slice,char *name,char *linkRoot)
/* Finds the slice specific map or starts it */
{
if(slice==NULL)
    return NULL;
struct mapSet *map = sliceGetMap(slice,TRUE); // Must be specific to this slice
if (map == NULL)
    map = sliceMapStart(slice,name,linkRoot);
return map;
}

struct mapSet *sliceMapUpdateOrStart(struct imgSlice *slice,char *name,char *linkRoot)
/* Updates the slice specific map or starts it */
{
struct mapSet *map = sliceGetMap(slice,TRUE); // Must be specific to this slice
if (map == NULL)
    return sliceMapStart(slice,name,linkRoot);
char qualifiedName[256];
safef(qualifiedName,sizeof(qualifiedName),"%s_%s",sliceTypeToString(slice->type),name);
return mapSetUpdate(map,qualifiedName,slice->parentImg,linkRoot);
}

static void sliceShow(struct dyString **dy,struct imgSlice *slice,int indent)
/* show the slice */
{
if(slice)
    {
    struct dyString *myDy = addIndent(dy,indent);
    dyStringPrintf(myDy,"slice(%s): title:%s width:%d height:%d offsetX:%d offsetY:%d",
            sliceTypeToString(slice->type),(slice->title?slice->title:""),
            slice->width,slice->height,slice->offsetX,slice->offsetY);
    if(slice->link)
        {
        addIndent(&myDy,indent);
        dyStringPrintf(myDy,"&nbsp;&nbsp;link:%s",slice->link);
        }
    indent++;
    imgShow(&myDy,slice->parentImg,"parent ", indent); // Currently we just have the one image
    mapShow(&myDy,slice->map,indent);
    if(dy == NULL)
        warn("%s",dyStringCannibalize(&myDy));
    else
        *dy = myDy;
    }
}

boolean sliceIsConsistent(struct imgSlice *slice,boolean verbose)
/* Test whether the slice and it's associated image and map are consistent with each other */
{
if (slice == NULL)
    {
    if (verbose)
        warn("slice is NULL");
    return FALSE;
    }
if (slice->parentImg == NULL && slice->type != stButton)
    {
    if (verbose)
        warn("slice(%s) has no image",sliceTypeToString(slice->type));
    return FALSE;
    }
if ( slice->width == 0
||  (slice->parentImg && slice->width  > slice->parentImg->width))
    {
    if (verbose)
        warn("slice(%s) has an invalid width %d (image width %d)",
             sliceTypeToString(slice->type),slice->width,slice->parentImg->width);
    return FALSE;
    }
if (slice->height == 0) // FIXME: This may be a temporary solution to empty data slices
    {
    if (verbose)
        warn("slice(%s) has an invalid height %d (image height %d)",
             sliceTypeToString(slice->type),slice->height,slice->parentImg->height);
    return FALSE;
    //return TRUE; // This may be valid (but is sloppy) when there is no data for the slice.
    }
if (slice->parentImg && slice->height > slice->parentImg->height)
    {
    if (verbose)
        warn("slice(%s) has an invalid height %d (image height %d)",
             sliceTypeToString(slice->type),slice->height,slice->parentImg->height);
    return FALSE;
    }
if ( slice->parentImg
&&  (slice->offsetX >= slice->parentImg->width
||   slice->offsetY >= slice->parentImg->height))
    {
    if (verbose)
        warn("slice(%s) has an invalid X:%d or Y:%d offset (image width:%d height:%d)",
             sliceTypeToString(slice->type),slice->offsetX,slice->offsetY,
             slice->parentImg->width,slice->parentImg->height);
    return FALSE;
    }
if (slice->link != NULL && slice->map != NULL)
    {
    warn("slice(%s) has both link and map of links",sliceTypeToString(slice->type));
    return FALSE;
    }
if (slice->map != NULL)
    {
    //if(slice->map->items == NULL)
    //    mapSetFree(&slice->map); // An empty map is ok but should be removed.
    //else
    if(!mapSetIsComplete(slice->map,verbose))
        {
        warn("slice(%s) has bad map",sliceTypeToString(slice->type));
        return FALSE;
        }

    struct mapItem *item = slice->map->items;
    for (;item != NULL; item = item->next)
        {
        if(!mapItemConsistentWithImage(item,slice->parentImg,verbose))
            {
            warn("slice(%s) map is inconsistent with slice image",sliceTypeToString(slice->type));
            return FALSE;
            }
        }
    }
return TRUE;
}

void sliceFree(struct imgSlice **pSlice)
/* frees all memory assocated with a slice (not including the image or a map belonging to the image) */
{
if(pSlice != NULL && *pSlice != NULL)
    {
    struct imgSlice *slice = *pSlice;
    // Don't free parentImg: remember that a slice is a portion of an image
    struct mapSet *map = sliceGetMap(slice,TRUE);// Only one that belongs to slice, not image
    if(map != NULL)
        mapSetFree(&map);
    freeMem(slice->title);
    freeMem(slice->link);
    freeMem(slice);
    *pSlice = NULL;
    }
}



/////////////////////// imgTracks

struct imgTrack *imgTrackStart(struct trackDb *tdb,char *name,char *db,char *chrom,int chromStart,int chromEnd,boolean plusStrand,boolean hasCenterLabel,enum trackVisibility vis,int order)
/* Starts an image track which will contain all image slices needed to render one track
   Must completed by adding slices with imgTrackAddSlice() */
{
struct imgTrack *imgTrack;     //  gifTn.forHtml, pixWidth, mapName
AllocVar(imgTrack);
imgTrack->centerLabelSeen = clAlways;
return imgTrackUpdate(imgTrack,tdb,name,db,chrom,chromStart,chromEnd,plusStrand,hasCenterLabel,vis,order);
}

struct imgTrack *imgTrackUpdate(struct imgTrack *imgTrack,struct trackDb *tdb,char *name,char *db,char *chrom,int chromStart,int chromEnd,boolean plusStrand,boolean hasCenterLabel,enum trackVisibility vis,int order)
/* Updates an already existing image track */
{
if(tdb != NULL && tdb != imgTrack->tdb)
    imgTrack->tdb    = tdb;
if(name != NULL && differentStringNullOk(imgTrack->name,name))
    {
    if(imgTrack->name != NULL)
        freeMem(imgTrack->name);
    imgTrack->name   = cloneString(name);
    }
if(db != NULL && db != imgTrack->db)
    imgTrack->db     = db;        // NOTE: Not allocated
if(chrom != NULL && chrom != imgTrack->chrom)
    imgTrack->chrom  = chrom;     // NOTE: Not allocated
imgTrack->chromStart = chromStart;
imgTrack->chromEnd   = chromEnd;
imgTrack->plusStrand = plusStrand;
imgTrack->hasCenterLabel = hasCenterLabel;
imgTrack->vis             = vis;
static int lastOrder = IMG_ORDEREND; // keep track of the order these images get added
if(order == IMG_FIXEDPOS)
    {
    imgTrack->reorderable = FALSE;
    if(name != NULL && sameString(RULER_TRACK_NAME,name))
        imgTrack->order = 0;
    else
        imgTrack->order = 9999;
    }
else
    {
    imgTrack->reorderable = TRUE;
    if(order == IMG_ANYORDER)
        {
        if(imgTrack->order <= 0)
            imgTrack->order = ++lastOrder;
        }
    else if(imgTrack->order != order)
        imgTrack->order = order;
    }
return imgTrack;
}

void imgTrackMarkForAjaxRetrieval(struct imgTrack *imgTrack,boolean ajaxRetrieval)
/* Updates the imgTrack to trigger an ajax callback from the html client to get this track */
{
imgTrack->ajaxRetrieval = ajaxRetrieval;
}

int imgTrackOrderCmp(const void *va, const void *vb)
/* Compare to sort on imgTrack->order */
{
const struct imgTrack *a = *((struct imgTrack **)va);
const struct imgTrack *b = *((struct imgTrack **)vb);
return (a->order - b->order);
}

struct imgSlice *imgTrackSliceAdd(struct imgTrack *imgTrack,enum sliceType type, struct image *img,char *title,int width,int height,int offsetX,int offsetY)
/* Adds slices to an image track.  Expected are types: stData, stButton, stSide and stCenter */
{
struct imgSlice *slice = sliceCreate(type,img,title,width,height,offsetX,offsetY);
if(slice)
    slAddHead(&(imgTrack->slices),slice);
return imgTrack->slices;
//slAddTail(&(imgTrack->slices),slice);
//return slice;
}

struct imgSlice *imgTrackSliceGetByType(struct imgTrack *imgTrack,enum sliceType type)
/* Gets a specific slice already added to an image track.  Expected are types: stData, stButton, stSide and stCenter */
{
struct imgSlice *slice;
for(slice = imgTrack->slices;slice != NULL;slice=slice->next)
    {
    if(slice->type == type)
        return slice;
    }
return NULL;
}

struct imgSlice *imgTrackSliceFindOrAdd(struct imgTrack *imgTrack,enum sliceType type, struct image *img,char *title,int width,int height,int offsetX,int offsetY)
/* Find the slice or adds it */
{
struct imgSlice *slice = imgTrackSliceGetByType(imgTrack,type);
if (slice == NULL)
    slice = imgTrackSliceAdd(imgTrack,type,img,title,width,height,offsetX,offsetY);
return slice;
}

struct imgSlice *imgTrackSliceUpdateOrAdd(struct imgTrack *imgTrack,enum sliceType type, struct image *img,char *title,int width,int height,int offsetX,int offsetY)
/* Updates the slice or adds it */
{
struct imgSlice *slice = imgTrackSliceGetByType(imgTrack,type);
if (slice == NULL)
    return imgTrackSliceAdd(imgTrack,type,img,title,width,height,offsetX,offsetY);
return sliceUpdate(slice,type,img,title,width,height,offsetX,offsetY);
}

struct mapSet *imgTrackGetMapByType(struct imgTrack *imgTrack,enum sliceType type)
/* Gets the map assocated with a specific slice belonging to the imgTrack */
{
struct imgSlice *slice = imgTrackSliceGetByType(imgTrack,type);
if(slice == NULL)
    return NULL;
return sliceGetMap(slice,FALSE); // Map could belong to image or could be slice specific
}

int imgTrackAddMapItem(struct imgTrack *imgTrack,char *link,char *title,int topLeftX,int topLeftY,int bottomRightX,int bottomRightY, char *id)
/* Will add a map item to an imgTrack's appropriate slice's map.  Since a map item may span
 * slices, the imgTrack is in the best position to determine where to put the map item
 * returns count of map items added, which could be 0, 1 or more than one if item spans slices
 * NOTE: Precedence is given to first map item when adding items with same coordinates! */
{
struct imgSlice *slice;
char *imgFile = NULL;               // name of file that hold the image

int count = 0;
for(slice = imgTrack->slices;slice != NULL;slice=slice->next)
    {
    if(slice->type == stButton) // Buttons don't have maps.  Overlap will be ignored!
        continue;
    if(slice->parentImg != NULL)
        {
        if(imgFile == NULL)
            imgFile = slice->parentImg->file;
        //else if(differentString(imgFile,slice->parentImg->file))
        //    {
        //    char * name = (imgTrack->name != NULL ? imgTrack->name : imgTrack->tdb != NULL ? imgTrack->tdb->track : imgFile);
        //    warn("imgTrackAddMapItem(%s) called, but not all slice images are the same for this track.",name);
        //    }
        // Not a valid warning!  Side image and data image may be different!!!
        }
    if(topLeftX     < (slice->offsetX + slice->width-1)
    && bottomRightX > (slice->offsetX + 1)
    && topLeftY     < (slice->offsetY + slice->height-1)
    && bottomRightY > (slice->offsetY + 1)) // Overlap of a pixel or 2 is tolerated
        {
        struct mapSet *map = sliceGetMap(slice,FALSE);
        if(map!=NULL)
            {          // NOTE: using find or add gives precedence to first of same coordinate map items added
            mapSetItemFindOrAdd(map,link,title,max(topLeftX,slice->offsetX),max(topLeftY,slice->offsetY),min(bottomRightX,slice->offsetX + slice->width),min(bottomRightY,slice->offsetY + slice->height), id);
            count++;
            }
        else
        {  // FIXME: This is assuming that if there is no map then the entire slice should get the link!
            char * name = (imgTrack->name != NULL ? imgTrack->name : imgTrack->tdb != NULL ? imgTrack->tdb->track : imgFile);
            warn("imgTrackAddMapItem(%s,%s) mapItem(lx:%d,rx:%d) is overlapping slice:%s(lx:%d,rx:%d)",name,title,topLeftX,bottomRightX,
                 sliceTypeToString(slice->type),slice->offsetX,(slice->offsetX + slice->width - 1));
            sliceAddLink(slice,link,title);
            count++;
            }
        }
    }
//if(count>=2)
//    {
//    char * name = (imgTrack->name != NULL ? imgTrack->name : imgTrack->tdb != NULL ? imgTrack->tdb->track : imgFile);
//    warn("imgTrackAddMapItem(%s) called for map items stretching across %d slice(s).",name,count);
//    }
return count;
}

static char *centerLabelSeenToString(enum centerLabelSeen seen)
/* Translate enum slice type to string */
{
switch(seen)
    {
    case clAlways: return "always";
    case clNowSeen:return "now";
    case clNotSeen:return "notNow";
    default:       return "unknown";
    }
}

static void imgTrackShow(struct dyString **dy,struct imgTrack *imgTrack,int indent)
/* show the imgTrack */
{
if(imgTrack)
    {
    struct dyString *myDy = addIndent(dy,indent);
    dyStringPrintf(myDy,"imgTrack: name:%s tdb:%s",
            (imgTrack->name?imgTrack->name:""),(imgTrack->tdb && imgTrack->tdb->track?imgTrack->tdb->track:""));
    if(imgTrack->hasCenterLabel)
        dyStringPrintf(myDy," centerLabel:%s",centerLabelSeenToString(imgTrack->centerLabelSeen));
    if(imgTrack->reorderable)
        dyStringPrintf(myDy," reorderable");
    if(imgTrack->ajaxRetrieval)
        dyStringPrintf(myDy," ajaxRetrieval");
    dyStringPrintf(myDy," order:%d vis:%s",imgTrack->order,hStringFromTv(imgTrack->vis));
    if(dy == NULL)
        warn("%s",dyStringCannibalize(&myDy));

    indent++;
    struct imgSlice *slice = imgTrack->slices;
    for(; slice != NULL; slice = slice->next )
        sliceShow(dy,slice,indent);

    if(dy != NULL)
        *dy = myDy;
    }
}

boolean imgTrackIsComplete(struct imgTrack *imgTrack,boolean verbose)
/* Tests the completeness and consistency of this imgTrack (including slices) */
{
if (imgTrack == NULL)
    {
    if (verbose)
        warn("imgTrack is NULL");
    return FALSE;
    }
if (imgTrack->tdb == NULL && imgTrack->name == NULL)
    {
    if (verbose)
        {
        warn("imgTrack has no tdb or name");
        imgTrackShow(NULL,imgTrack,0);
        }
    return FALSE;
    }
char * name = (imgTrack->name != NULL ? imgTrack->name : imgTrack->tdb->track);
if (imgTrack->db == NULL)
    {
    if (verbose)
        {
        warn("imgTrack(%s) has no db.",name);
        imgTrackShow(NULL,imgTrack,0);
        }

    return FALSE;
    }
if (imgTrack->db == NULL)
    {
    if (verbose)
        {
        warn("imgTrack(%s) has no chrom.",name);
        imgTrackShow(NULL,imgTrack,0);
        }
    return FALSE;
    }
if (imgTrack->chromStart  >= imgTrack->chromEnd)
    {
    if (verbose)
        {
        warn("imgTrack(%s) for %s.%s:%d-%d has bad genome range.",name,
             imgTrack->db,imgTrack->chrom,imgTrack->chromStart,imgTrack->chromEnd);
        imgTrackShow(NULL,imgTrack,0);
        }
    return FALSE;
    }
if(imgTrack->slices == NULL)
    {
    if (verbose)
        {
        warn("imgTrack(%s) has no slices.",name);
        imgTrackShow(NULL,imgTrack,0);
        }
    return FALSE;
    }
// Can have no more than one of each type of slice
if(imgTrack->slices && imgTrack->slices->type == stData)
   slReverse(&imgTrack->slices);
boolean found[stMaxSliceTypes] = { FALSE,FALSE,FALSE,FALSE};
struct imgSlice *slice = imgTrack->slices;
for(; slice != NULL; slice = slice->next )
    {
    if(found[slice->type])
        {
        if (verbose)
            {
            warn("imgTrack(%s) found more than one slice of type %s.",
                 name,sliceTypeToString(slice->type));
            imgTrackShow(NULL,imgTrack,0);
            }
        return FALSE;
        }
    found[slice->type] = TRUE;

    if(!sliceIsConsistent(slice,verbose))
        {
        if (verbose)
            {
            warn("imgTrack(%s) has bad slice",name);
            imgTrackShow(NULL,imgTrack,0);
            }
        return FALSE;
        }
    }
// This is not a requirement as the data portion could be empty (height==0) FIXME This still needs to be properly resolved
//if(!found[stData])
//    {
//    if (verbose)
//        {
//        warn("imgTrack(%s) has no DATA slice.",name);
//        imgTrackShow(NULL,imgTrack,0);
//        }
//    return FALSE;
//    }

return TRUE;
}

void imgTrackFree(struct imgTrack **pImgTrack)
/* frees all memory assocated with an imgTrack (including slices) */
{
if(pImgTrack != NULL && *pImgTrack != NULL)
    {
    struct imgTrack *imgTrack = *pImgTrack;
    struct imgSlice *slice;
    while((slice = slPopHead(&(imgTrack->slices))) != NULL )
        sliceFree(&slice);
    freeMem(imgTrack->name);
    freeMem(imgTrack);
    *pImgTrack = NULL;
    }
}



/////////////////////// Image Box

struct imgBox *imgBoxStart(char *db,char *chrom,int chromStart,int chromEnd,boolean plusStrand,int sideLabelWidth,int width)
/* Starts an imgBox which should contain all info needed to draw the hgTracks image with multiple tracks
   The image box must be completed using imgBoxImageAdd() and imgBoxTrackAdd() */
{
struct imgBox * imgBox;     //  gifTn.forHtml, pixWidth, mapName
AllocVar(imgBox);
if(db != NULL)
    imgBox->db         = cloneString(db);     // NOTE: Is allocated
if(chrom != NULL)
    imgBox->chrom      = cloneString(chrom);  // NOTE: Is allocated
imgBox->chromStart     = chromStart;
imgBox->chromEnd       = chromEnd;
imgBox->plusStrand     = plusStrand;
imgBox->showSideLabel  = (sideLabelWidth != 0);
imgBox->sideLabelWidth = sideLabelWidth;
imgBox->images         = NULL;
imgBox->bgImg          = NULL;
imgBox->width          = width;
imgBox->showPortal     = FALSE;
//int oneThird = (chromEnd - chromStart)/3; // TODO: Currently defaulting to 1/3 of image width
imgBox->portalStart    = chromStart;// + oneThird;
imgBox->portalEnd      = chromEnd;// - oneThird;
imgBox->portalWidth    = chromEnd - chromStart;
imgBox->basesPerPixel  = ((double)imgBox->chromEnd - imgBox->chromStart)/(imgBox->width - imgBox->sideLabelWidth);
return imgBox;
}

boolean imgBoxPortalDefine(struct imgBox *imgBox,int *chromStart,int *chromEnd,int *imgWidth,double imageMultiple)
/* Defines the portal of the imgBox.  The portal is the initial viewable region when dragScroll is being used.
   the new chromStart,chromEnd and imgWidth are returned as OUTs, while the portal becomes the initial defined size
   returns TRUE if successfully defined as having a portal */
{
if( (int)imageMultiple == 0)
#ifdef IMAGEv2_DRAG_SCROLL_SZ
    imageMultiple = IMAGEv2_DRAG_SCROLL_SZ;
#else//ifndef IMAGEv2_DRAG_SCROLL_SZ
    imageMultiple = 1;
#endif//ndef IMAGEv2_DRAG_SCROLL_SZ

imgBox->portalStart = imgBox->chromStart;
imgBox->portalEnd   = imgBox->chromEnd;
imgBox->portalWidth = imgBox->width - imgBox->sideLabelWidth;
imgBox->showPortal  = FALSE; // Guilty until proven innocent

int positionWidth = (int)((imgBox->portalEnd - imgBox->portalStart) * imageMultiple);
*chromStart = imgBox->portalStart - (int)(((imageMultiple - 1)/2) * (imgBox->portalEnd - imgBox->portalStart));
if( *chromStart < 0)
    *chromStart = 0;
*chromEnd = *chromStart + positionWidth;
struct chromInfo *chrInfo = hGetChromInfo(imgBox->db,imgBox->chrom);
if(chrInfo == NULL)
    {
    *chromStart = imgBox->chromStart;
    *chromEnd   = imgBox->chromEnd;
    return FALSE;
    }
if (*chromEnd > (int)(chrInfo->size))  // Bound by chrom length
    {
    *chromEnd = (int)(chrInfo->size);
    *chromStart = *chromEnd - positionWidth;
    if (*chromStart < 0)
        *chromStart = 0;
    }
// TODO: Normalize to power of 10 boundary
// Normalize portal ends
int diff = *chromStart - imgBox->portalStart;
if(diff < 10 && diff > -10)
    *chromStart = imgBox->portalStart;
diff = *chromEnd - imgBox->portalEnd;
if(diff < 10 && diff > -10)
    *chromEnd = imgBox->portalEnd;

double growthOfImage = (*chromEnd - *chromStart)/(imgBox->portalEnd - imgBox->portalStart);
*imgWidth = (imgBox->portalWidth * growthOfImage) + imgBox->sideLabelWidth;

//if(imgBox->portalStart < *chromStart || imgBox->portalEnd > *chromEnd
//|| imgBox->portalWidth > *imgWidth)
//    {
//    *imgWidth   = imgBox->width;  // Undo damage
//    *chromStart = imgBox->chromStart;
//    *chromEnd   = imgBox->chromEnd;
//    return FALSE;
//    }
imgBox->width      = *imgWidth;
imgBox->chromStart = *chromStart;
imgBox->chromEnd   = *chromEnd;
imgBox->basesPerPixel = ((double)imgBox->chromEnd - imgBox->chromStart)/(imgBox->width - imgBox->sideLabelWidth);
imgBox->showPortal    = TRUE;
//warn("portal(%d,%d,%d)\nimage(%d,%d,%d) growth:%G",imgBox->portalStart,imgBox->portalEnd,imgBox->portalWidth,
//     imgBox->chromStart,imgBox->chromEnd,(imgBox->width - imgBox->sideLabelWidth),growthOfImage);

return imgBox->showPortal;
}

boolean imgBoxPortalRemove(struct imgBox *imgBox,int *chromStart,int *chromEnd,int *imgWidth)
/* Will redefine the imgBox as the portal dimensions and return the dimensions as OUTs.
   Returns TRUE if a portal was defined in the first place */
{
if(imgBox->showPortal == FALSE)
    {
    *chromStart=imgBox->chromStart;  // return to original coordinates
    *chromEnd  =imgBox->chromEnd;
    *imgWidth  =imgBox->width;
    return FALSE;
    }
*chromStart=imgBox->chromStart=imgBox->portalStart;  // return to original coordinates
*chromEnd  =imgBox->chromEnd  =imgBox->portalEnd;
*imgWidth  =imgBox->width     = (imgBox->portalWidth + imgBox->sideLabelWidth);
imgBox->showPortal = FALSE;
return TRUE;
}

boolean imgBoxPortalDimensions(struct imgBox *imgBox,int *chromStart,int *chromEnd,int *imgWidth,int *sideLabelWidth,int *portalStart,int *portalEnd,int *portalWidth,double *basesPerPixel)
/* returns the imgBox portal dimensions in the OUTs  returns TRUE if portal defined */
{
if ( chromStart )
    *chromStart      = imgBox->chromStart;
if ( chromEnd )
    *chromEnd        = imgBox->chromEnd;
if ( imgWidth )
    *imgWidth        = imgBox->width;
if ( sideLabelWidth )
    *sideLabelWidth  = imgBox->sideLabelWidth;
if(imgBox->showPortal)
    {
    if ( portalStart )
        *portalStart = imgBox->portalStart;
    if ( portalEnd   )
        *portalEnd   = imgBox->portalEnd;
    if ( portalWidth )
        *portalWidth = imgBox->portalWidth + imgBox->sideLabelWidth;
    }
else
    {
    if ( portalStart )
        *portalStart = imgBox->chromStart;
    if ( portalEnd   )
        *portalEnd   = imgBox->chromEnd;
    if ( portalWidth )
        *portalWidth = imgBox->width;
    }
if ( basesPerPixel   )
    *basesPerPixel   = imgBox->basesPerPixel;
return imgBox->showPortal;
}

struct image *imgBoxImageAdd(struct imgBox *imgBox,char *gif,char *title,int width,int height,boolean backGround)
/* Adds an image to an imgBox.  The image may be extended with imgMapStart(),mapSetItemAdd() */
{
struct image *img = imgCreate(gif,title,width,height);
if(backGround)
    {
    if(imgBox->bgImg != NULL)
        {
        warn("imgBoxImageAdd() for background but already exists. Being replaced.");
        imgFree(&(imgBox->bgImg));
        }
        imgBox->bgImg = img;
        return imgBox->bgImg;
    }
slAddHead(&(imgBox->images),img);
return imgBox->images;
}

struct image *imgBoxImageFind(struct imgBox *imgBox,char *gif)
/* Finds a specific image already added to this imgBox */
{
struct image *img = NULL;
for (img = imgBox->images; img != NULL; img = img->next )
    {
    if (sameOk(img->file,gif))
        return img;
    }
return NULL;
}

// TODO: Will we need this?
//boolean imgBoxImageRemove(struct imgBox *imgBox,struct image *img)
//{
//return slRemoveEl(&(imgBox->images),img);
//}

struct imgTrack *imgBoxTrackAdd(struct imgBox *imgBox,struct trackDb *tdb,char *name,enum trackVisibility vis,boolean hasCenterLabel,int order)
/* Adds an imgTrack to an imgBox.  The imgTrack needs to be extended with imgTrackAddSlice() */
{
struct imgTrack *imgTrack = imgTrackStart(tdb,name,imgBox->db,imgBox->chrom,imgBox->chromStart,imgBox->chromEnd,imgBox->plusStrand,hasCenterLabel,vis,order);
slAddHead(&(imgBox->imgTracks),imgTrack);
return imgBox->imgTracks;
}

struct imgTrack *imgBoxTrackFind(struct imgBox *imgBox,struct trackDb *tdb,char *name)
/* Finds a specific imgTrack already added to this imgBox */
{
struct imgTrack *imgTrack = NULL;
for (imgTrack = imgBox->imgTracks; imgTrack != NULL; imgTrack = imgTrack->next )
    {
    if (name != NULL && sameOk(name,imgTrack->name))
        return imgTrack;
    else if (imgTrack->tdb == tdb)
        return imgTrack;
    }
return NULL;
}

struct imgTrack *imgBoxTrackFindOrAdd(struct imgBox *imgBox,struct trackDb *tdb,char *name,enum trackVisibility vis,boolean hasCenterLabel,int order)
/* Find the imgTrack, or adds it if not found */
{
struct imgTrack *imgTrack = imgBoxTrackFind(imgBox,tdb,name);
if( imgTrack == NULL)
    imgTrack = imgBoxTrackAdd(imgBox,tdb,name,vis,hasCenterLabel,order);
return imgTrack;
}

struct imgTrack *imgBoxTrackUpdateOrAdd(struct imgBox *imgBox,struct trackDb *tdb,char *name,enum trackVisibility vis,boolean hasCenterLabel,int order)
/* Updates the imgTrack, or adds it if not found */
{
struct imgTrack *imgTrack = imgBoxTrackFind(imgBox,tdb,name);
if( imgTrack == NULL)
    return imgBoxTrackAdd(imgBox,tdb,name,vis,hasCenterLabel,order);

return imgTrackUpdate(imgTrack,tdb,name,imgBox->db,imgBox->chrom,imgBox->chromStart,imgBox->chromEnd,imgBox->plusStrand,hasCenterLabel,vis,order);
}

// TODO: Will we need this?
//boolean imgBoxTrackRemove(struct imgBox *imgBox,struct imgTrack *imgTrack)
//{
//return slRemoveEl(&(imgBox->imgTracks),imgTrack);
//}

void imgBoxTracksNormalizeOrder(struct imgBox *imgBox)
/* This routine sorts the imgTracks */
{
slSort(&(imgBox->imgTracks), imgTrackOrderCmp);
}

void imgBoxShow(struct dyString **dy,struct imgBox *imgBox,int indent)
/* show the imgBox */
{
if(imgBox)
    {
    struct dyString *myDy = addIndent(dy,indent);
    dyStringPrintf(myDy,"imgBox: %s.%s:%d-%d %c width:%d basePer:%g sideLabe:%s w:%d portal:%s %d-%d w:%d",
            (imgBox->db?imgBox->db:""),(imgBox->chrom?imgBox->chrom:""),
            imgBox->chromStart,imgBox->chromEnd,(imgBox->plusStrand?'+':'-'),
            imgBox->width,imgBox->basesPerPixel,(imgBox->showSideLabel?"Yes":"No"),imgBox->sideLabelWidth,
            (imgBox->showPortal?"Yes":"No"),imgBox->portalStart,imgBox->portalEnd,imgBox->portalWidth);
    indent++;
    struct image *img;
    for(img=imgBox->images;img!=NULL;img=img->next)
        imgShow(&myDy,img,"data ",indent);
    if(imgBox->bgImg)
        imgShow(&myDy,imgBox->bgImg,"bgnd ",indent);
    if(dy == NULL)
        warn("%s",dyStringCannibalize(&myDy));

    struct imgTrack *imgTrack = NULL;
    for (imgTrack = imgBox->imgTracks; imgTrack != NULL; imgTrack = imgTrack->next )
        imgTrackShow(dy,imgTrack,indent);

    if(dy != NULL)
        *dy = myDy;
    }
}

int imgBoxDropEmpties(struct imgBox *imgBox)
/* Empty imageTracks (without slices) is not an error but they should be dropped.
   returns remaining current track count */
{
if (imgBox == NULL)
    return 0;
struct imgTrack *imgTrack = imgBox->imgTracks;
while(imgTrack != NULL)
    {
    if(imgTrack->slices == NULL)
        {
        slRemoveEl(&(imgBox->imgTracks),imgTrack);
        imgTrackFree(&imgTrack);
        imgTrack = imgBox->imgTracks; // start over
        continue;
        }
    imgTrack = imgTrack->next;
    }
return slCount(imgBox->imgTracks);
}

boolean imgBoxIsComplete(struct imgBox *imgBox,boolean verbose)
/* Tests the completeness and consistency of an imgBox. */
{
if (imgBox == NULL)
    {
    if (verbose)
        warn("No imgBox.");
    return FALSE;
    }
if (imgBox->db == NULL)
    {
    if (verbose)
        warn("imgBox has no db.");
    return FALSE;
    }
if (imgBox->db == NULL)
    {
    if (verbose)
        warn("imgBox has no chrom.");
    return FALSE;
    }
if (imgBox->chromStart  >= imgBox->chromEnd)
    {
    if (verbose)
        warn("imgBox(%s.%s:%d-%d) has bad genome range.",imgBox->db,imgBox->chrom,imgBox->chromStart,imgBox->chromEnd);
    return FALSE;
    }
if (imgBox->portalStart >= imgBox->portalEnd
||  imgBox->portalStart <  imgBox->chromStart
||  imgBox->portalEnd   >  imgBox->chromEnd  )
    {
    if (verbose)
        warn("imgBox(%s.%s:%d-%d) has bad portal range: %d-%d",imgBox->db,imgBox->chrom,imgBox->chromStart,imgBox->chromEnd,imgBox->portalStart,imgBox->portalEnd);
    return FALSE;
    }

// Must have images
if (imgBox->images == NULL)
    {
    if (verbose)
        warn("imgBox(%s.%s:%d-%d) has no images",imgBox->db,imgBox->chrom,imgBox->chromStart,imgBox->chromEnd);
    return FALSE;
    }
// Must have tracks
if (imgBox->imgTracks == NULL)
    {
    if (verbose)
        warn("imgBox(%s.%s:%d-%d) has no imgTracks",imgBox->db,imgBox->chrom,imgBox->chromStart,imgBox->chromEnd);
    return FALSE;
    }
struct imgTrack *imgTrack = imgBox->imgTracks;
while(imgTrack != NULL)
    {
    if(!imgTrackIsComplete(imgTrack,verbose))
        {
        if (verbose)
            warn("imgBox(%s.%s:%d-%d) has bad track - being skipped.",imgBox->db,imgBox->chrom,imgBox->chromStart,imgBox->chromEnd);
        slRemoveEl(&(imgBox->imgTracks),imgTrack);
        imgTrackFree(&imgTrack);
        imgTrack = imgBox->imgTracks; // start over
        continue;
        //return FALSE;
        }
    if(differentWord(imgTrack->db,           imgBox->db)
    || differentWord(imgTrack->chrom,        imgBox->chrom)
    ||               imgTrack->chromStart != imgBox->chromStart
    ||               imgTrack->chromEnd   != imgBox->chromEnd
    ||               imgTrack->plusStrand != imgBox->plusStrand)
        {
        if (verbose)
            warn("imgBox(%s.%s:%d-%d) has inconsistent imgTrack for %s.%s:%d-%d",
                  imgBox->db,  imgBox->chrom,  imgBox->chromStart,  imgBox->chromEnd,
                imgTrack->db,imgTrack->chrom,imgTrack->chromStart,imgTrack->chromEnd);
        return FALSE;
        }
    struct imgSlice *slice = NULL;
    for (slice = imgTrack->slices; slice != NULL; slice = slice->next )
        {
        // Every slice that has an image must point to an image owned by the imgBox
        if(slice->parentImg && (slIxFromElement(imgBox->images,slice->parentImg) == -1))
            {
            if (verbose)
                warn("imgBox(%s.%s:%d-%d) has slice(%s) for unknown image (%s)",
                    imgBox->db,imgBox->chrom,imgBox->chromStart,imgBox->chromEnd,
                    sliceTypeToString(slice->type),slice->parentImg->file);
            return FALSE;
            }
        }
    imgTrack = imgTrack->next;
    }
return TRUE;
}

void imgBoxFree(struct imgBox **pImgBox)
/* frees all memory assocated with an imgBox (including images and imgTracks) */
{
if(pImgBox != NULL && *pImgBox != NULL)
    {
    struct imgBox *imgBox = *pImgBox;
    struct imgTrack *imgTrack = NULL;
    while((imgTrack = slPopHead(&(imgBox->imgTracks))) != NULL )
        imgTrackFree(&imgTrack);
    struct image *img = NULL;
    while((img = slPopHead(&(imgBox->images))) != NULL )
        imgFree(&img);
    imgFree(&(imgBox->bgImg));
    freeMem(imgBox->db);
    freeMem(imgBox->chrom);
    freeMem(imgBox);
    *pImgBox = NULL;
    }
}



/////////////////////// imageV2 UI API

static boolean imageMapDraw(struct mapSet *map,char *name)
/* writes an image map as HTML */
{
//warn("Drawing map_%s %s",name,(map == NULL?"map is NULL":map->items == NULL?"map->items is NULL":"Should draw!"));
if(map == NULL || map->items == NULL)
    return FALSE;

slReverse(&(map->items)); // These must be reversed so that they are printed in the same order as created!

hPrintf("  <MAP name='map_%s'>", name); // map_ prefix is implicit
struct mapItem *item = map->items;
for(;item!=NULL;item=item->next)
    {
    hPrintf("\n   <AREA SHAPE=RECT COORDS='%d,%d,%d,%d' class='area'",
           item->topLeftX, item->topLeftY, item->bottomRightX, item->bottomRightY);
    // TODO: remove static portion of the link and handle in js
    if(map->linkRoot != NULL)
        {
        if(skipToSpaces(item->linkVar))
            hPrintf(" HREF=%s%s",map->linkRoot,(item->linkVar != NULL?item->linkVar:""));
        else
            hPrintf(" HREF='%s%s'",map->linkRoot,(item->linkVar != NULL?item->linkVar:""));
        }
    else if(item->linkVar != NULL)
        {
        if(skipToSpaces(item->linkVar))
            hPrintf(" HREF=%s",item->linkVar);
	else if(startsWith("/cgi-bin/hgGene", item->linkVar)) // redmine #4151
                 hPrintf(" HREF='..%s'",item->linkVar);
             else
                 hPrintf(" HREF='%s'",item->linkVar);
        }
    else
        warn("map item has no url!");

    if(item->title != NULL && strlen(item->title) > 0)
        hPrintf(" TITLE='%s'", htmlEncode(item->title) );
    if(item->id != NULL)
        hPrintf(" id='%s'", item->id);
    hPrintf(">" );
    }
hPrintf("</MAP>\n");
return TRUE;
}

static void imageDraw(struct imgBox *imgBox,struct imgTrack *imgTrack,struct imgSlice *slice,char *name,int offsetX,int offsetY,boolean useMap)
/* writes an image as HTML */
{
if(slice->parentImg && slice->parentImg->file != NULL)
    {
    hPrintf("  <IMG id='img_%s' src='%s' style='left:-%dpx; top: -%dpx;'",
            name,slice->parentImg->file,offsetX,offsetY);
    // Problem: dragScroll beyond left shows ugly leftLabel!
    // Tried clip:rect() but this only works with position:absolute!
    // May need to split image betweeen side label and data!!! That is a big change.

    if(useMap)
        hPrintf(" usemap='#map_%s'",name);
    hPrintf(" class='sliceImg %s",sliceTypeToClass(slice->type));
    if(slice->type==stData && imgBox->showPortal)
        hPrintf(" panImg'");
    else
        hPrintf("'");
    if(slice->title != NULL)
        hPrintf(" title='%s'", htmlEncode(slice->title) );           // Adds slice wide title
    else if(slice->parentImg->title != NULL)
        hPrintf("' title='%s'", htmlEncode(slice->parentImg->title) );// Adds image wide title
    if(slice->type==stData || slice->type==stCenter)
        hPrintf(" ondrag='{return false;}'");
    hPrintf(">");
    }
else
    {
    int height = slice->height;
    // Adjustment for centerLabel Conditional
    if (imgTrack->centerLabelSeen == clNotSeen
    &&  (slice->type == stSide || slice->type == stButton))
        {
        struct imgSlice *centerSlice = imgTrackSliceGetByType(imgTrack,stCenter);
        if (centerSlice != NULL)
            height -= centerSlice->height;
        }
    hPrintf("  <p id='p_%s' style='height:%dpx;",name,height);
    if(slice->type==stButton)
        {
        char *trackName = imgTrack->name;
        if(trackName == NULL)
            {
            struct trackDb * tdb = imgTrack->tdb;
            if(tdbIsCompositeChild(tdb))
                tdb = tdbGetComposite(tdb);
            trackName = tdb->track;
            }
        hPrintf(" width:9px; display:none;' class='%s %sbtn btnN'></p>",trackName,(slice->link == NULL?"inset ":""));
        }
    else
        hPrintf("width:%dpx;'></p>",slice->width);
    }
}

// FF does not support newline code and '...' looks bad without newlines
#define NEWLINE_ENCODED " &#x0A;"
#define NEWLINE_NOT_SUPPORTED " - "
#define NEWLINE_TO_USE(browser) ((browser) == btFF ? NEWLINE_NOT_SUPPORTED : NEWLINE_ENCODED)
#define ELLIPSIS_TO_USE(browser) ((browser) == btFF ? "" : "...")

static void sliceAndMapDraw(struct imgBox *imgBox,struct imgTrack *imgTrack,enum sliceType sliceType,char *name,boolean scrollHandle)
/* writes a slice of an image and any assocated image map as HTML */
{
if(imgBox==NULL || imgTrack==NULL)
    return;
struct imgSlice *slice = imgTrackSliceGetByType(imgTrack,sliceType);
if(slice==NULL || slice->height == 0)
    return;

boolean useMap=FALSE;
int offsetX=slice->offsetX;
int offsetY=slice->offsetY;
int height = slice->height;
int width=slice->width;
if(slice->parentImg)
    {
    // Adjustment for centerLabel Conditional
    if (imgTrack->centerLabelSeen == clNotSeen
    &&  (sliceType == stSide || sliceType == stButton))
        {
        struct imgSlice *centerSlice = imgTrackSliceGetByType(imgTrack,stCenter);
        if (centerSlice != NULL)
            {
            height -= centerSlice->height;
            offsetY += centerSlice->height;
            }
        }
    // Adjustment for portal
    if(imgBox->showPortal && imgBox->basesPerPixel > 0
    && (sliceType==stData || sliceType==stCenter))
        {
        offsetX += (imgBox->portalStart - imgBox->chromStart) / imgBox->basesPerPixel;
        width=imgBox->portalWidth;
        }
    hPrintf("  <div style='width:%dpx; height:%dpx;",width,height);
    if (sliceType == stCenter && imgTrack->centerLabelSeen == clNotSeen)
        hPrintf(" display:none;");
    hPrintf("' class='sliceDiv %s",sliceTypeToClass(slice->type));

    #ifdef IMAGEv2_DRAG_SCROLL
    if(imgBox->showPortal && (sliceType==stData || sliceType==stCenter))
        hPrintf(" panDiv%s",(scrollHandle?" scroller":""));
    #endif //def IMAGEv2_DRAG_SCROLL
    hPrintf("'>\n");
    }
struct mapSet *map = sliceGetMap(slice,FALSE); // Could be the image map or slice specific
if(map)
    useMap = imageMapDraw(map,name);
else if(slice->link != NULL)
    {
    if(skipToSpaces(slice->link) != NULL)
        hPrintf("  <A HREF=%s",slice->link);
    else
        hPrintf("  <A HREF='%s'",slice->link);
    if (slice->title != NULL)
        {
        if (sliceType == stButton)
            {
            enum browserType browser = cgiClientBrowser(NULL,NULL,NULL);
            char *newLine = NEWLINE_TO_USE(browser);
            char *ellipsis = ELLIPSIS_TO_USE(browser);
            if(imgTrack->reorderable)
                hPrintf(" TITLE='%s%sclick or right click to configure%s%sdrag to reorder%s'",htmlEncode(slice->title), newLine,
                    ellipsis, newLine,(tdbIsCompositeChild(imgTrack->tdb)?" highlighted subtracks":"") );
            else
                hPrintf(" TITLE='%s%sclick or right click to configure%s'",htmlEncode(slice->title), newLine, ellipsis);
            }
        else
            hPrintf(" TITLE='Click for: &#x0A;%s'", htmlEncode(slice->title) );
        }
    hPrintf(">\n" );
    }

imageDraw(imgBox,imgTrack,slice,name,offsetX,offsetY,useMap);
if(slice->link != NULL)
    hPrintf("</A>");

if(slice->parentImg)
    hPrintf("</div>");
}

void imageBoxDraw(struct imgBox *imgBox)
/* writes a entire imgBox including all tracksas HTML */
{
if(imgBox->imgTracks == NULL)  // Not an error to have an empty image
    return;
imgBoxDropEmpties(imgBox);
boolean verbose = (hIsPrivateHost());   // Warnings for hgwdev only
if(!imgBoxIsComplete(imgBox,verbose)) // dorps empties as okay
    return;
char name[256];

imgBoxTracksNormalizeOrder(imgBox);
//if(verbose)
//    imgBoxShow(NULL,imgBox,0);

hPrintf("<!-- - - - - - - - vvv IMAGEv2 vvv - - - - - - - -->\n");  // DANGER FF interprets '--' as end of comment, not '-->'
jsIncludeFile("jquery.tablednd.js", NULL);
if(imgBox->bgImg)
    {
    int offset = 0;
    if(imgBox->showSideLabel && imgBox->plusStrand)
        {
        struct imgSlice *slice = imgTrackSliceGetByType(imgBox->imgTracks,stData);
        if(slice)
            offset = (slice->offsetX * -1);  // This works because the ruler has a slice
         }
    hPrintf("<style type='text/css'>\n");
    if(offset != 0)
        hPrintf("td.tdData {background-image:url(\"%s\");background-repeat:repeat-y;background-position:%dpx;}\n",imgBox->bgImg->file,offset);
    else
        hPrintf("td.tdData {background-image:url(\"%s\");background-repeat:repeat-y;}\n",imgBox->bgImg->file);
    hPrintf("</style>\n");
    }

#ifdef IMAGEv2_DRAG_SCROLL
if(imgBox->showPortal)
    {
    // Let js code know what's up
    int chromSize = hChromSize(database, chromName);
    jsonHashAddNumber( jsonForClient,"chromStart",   1);
    jsonHashAddNumber( jsonForClient,"chromEnd",     chromSize);
    jsonHashAddBoolean(jsonForClient,"imgBoxPortal",       TRUE);
    jsonHashAddNumber( jsonForClient,"imgBoxWidth",        (imgBox->width - imgBox->sideLabelWidth));
    jsonHashAddNumber( jsonForClient,"imgBoxPortalStart",  imgBox->portalStart);
    jsonHashAddNumber( jsonForClient,"imgBoxPortalEnd",    imgBox->portalEnd);
    jsonHashAddNumber( jsonForClient,"imgBoxPortalWidth",  imgBox->portalWidth);
    jsonHashAddNumber( jsonForClient,"imgBoxLeftLabel",    (imgBox->plusStrand?imgBox->sideLabelWidth:0));
    jsonHashAddNumber( jsonForClient,"imgBoxPortalOffsetX",(long)((imgBox->portalStart - imgBox->chromStart) / imgBox->basesPerPixel));
    jsonHashAddDouble( jsonForClient,"imgBoxBasesPerPixel",imgBox->basesPerPixel);
    }
else
#endif//def IMAGEv2_DRAG_SCROLL
    jsonHashAddBoolean(jsonForClient,"imgBoxPortal",       FALSE);

hPrintf("<TABLE id='imgTbl' border=0 cellspacing=0 cellpadding=0 BGCOLOR='%s'",COLOR_WHITE);//COLOR_RED); // RED to help find bugs
hPrintf(" width=%d",imgBox->showPortal?(imgBox->portalWidth+imgBox->sideLabelWidth):imgBox->width);
hPrintf(" class='tableWithDragAndDrop'");
hPrintf(" style='border:1px solid blue;border-collapse:separate;'>\n");

struct jsonHashElement *jsonTdbVars = newJsonHash(newHash(8));
jsonTdbSettingsInit(jsonTdbVars);

char *newLine = NEWLINE_TO_USE(cgiClientBrowser(NULL,NULL,NULL));
struct imgTrack *imgTrack = imgBox->imgTracks;
for(;imgTrack!=NULL;imgTrack=imgTrack->next)
    {
    char *trackName = (imgTrack->name != NULL ? imgTrack->name : imgTrack->tdb->track );
    struct track *track = hashFindVal(trackHash, trackName);
    if(track)
        jsonTdbSettingsBuild(jsonTdbVars, track, TRUE);
    hPrintf("<TR id='tr_%s' abbr='%d' class='imgOrd%s%s%s'>\n",trackName,imgTrack->order,
        (imgTrack->reorderable?" trDraggable":" nodrop nodrag"),
        (imgTrack->centerLabelSeen != clAlways?" clOpt":""),
        (imgTrack->ajaxRetrieval ?" mustRetrieve":""));

    if(imgBox->showSideLabel && imgBox->plusStrand)
        {
        // button
        safef(name, sizeof(name), "btn_%s", trackName);
        hPrintf(" <TD id='td_%s'%s>\n",name,(imgTrack->reorderable?" class='dragHandle'":""));
        sliceAndMapDraw(imgBox,imgTrack,stButton,name,FALSE);
        hPrintf("</TD>\n");
        // leftLabel
        safef(name,sizeof(name),"side_%s",trackName);
        if (imgTrack->reorderable)
            hPrintf(" <TD id='td_%s' class='dragHandle tdLeft' title='%s%sdrag to reorder'>\n",name,htmlEncode(imgTrack->tdb->longLabel),newLine);
        else
            hPrintf(" <TD id='td_%s' class='tdLeft'>\n",name);
        sliceAndMapDraw(imgBox,imgTrack,stSide,name,FALSE);
        hPrintf("</TD>\n");
        }

    // Main/Data image region
#ifdef IMAGEv2_DRAG_SCROLL
    hPrintf(" <TD id='td_data_%s' title='click & drag to scroll; shift+click & drag to zoom' width=%d class='tdData'>\n", trackName, imgBox->width);
#else///ifndef IMAGEv2_DRAG_SCROLL
    hPrintf(" <TD id='td_data_%s' width=%d class='tdData'>\n", trackName, imgBox->width);
#endif//ndef IMAGEv2_DRAG_SCROLL
    // centerLabel
    if(imgTrack->hasCenterLabel)
        {
        safef(name, sizeof(name), "center_%s", trackName);
        sliceAndMapDraw(imgBox,imgTrack,stCenter,name,TRUE);
        hPrintf("\n");
        }
    // data image
    safef(name, sizeof(name), "data_%s", trackName);
    sliceAndMapDraw(imgBox,imgTrack,stData,name,(imgTrack->order>0));
    hPrintf("</TD>\n");

    if(imgBox->showSideLabel && !imgTrack->plusStrand)
        {
        // rightLabel
        safef(name, sizeof(name), "side_%s", trackName);
        if (imgTrack->reorderable)
            hPrintf(" <TD id='td_%s' class='dragHandle tdRight' title='%s%sdrag to reorder'>\n",name,htmlEncode(imgTrack->tdb->longLabel),newLine);
        else
            hPrintf(" <TD id='td_%s' class='tdRight'>\n",name);
        sliceAndMapDraw(imgBox,imgTrack,stSide,name,FALSE);
        hPrintf("</TD>\n");
        // button
        safef(name, sizeof(name), "btn_%s", trackName);
        hPrintf(" <TD id='td_%s'%s>\n",name,(imgTrack->reorderable?" class='dragHandle'":""));
        sliceAndMapDraw(imgBox,imgTrack,stButton, name,FALSE);
        hPrintf("</TD>\n");
        }
    hPrintf("</TR>\n");
    }
hPrintf("</TABLE>\n");
hPrintf("<!-- - - - - - - - ^^^ IMAGEv2 ^^^ - - - - - - - -->\n");  // DANGER FF interprets '--' as end of comment, not '-->'
jsonTdbSettingsUse(jsonTdbVars);
}
