/* imageV2 - API for creating the image V2 features. */
#include "common.h"
#include "hPrint.h"
#include "chromInfo.h"
#include "hdb.h"
#include "hui.h"
#include "jsHelper.h"
#include "imageV2.h"

static char const rcsid[] = "$Id: imageV2.c,v 1.12 2009/11/11 20:41:29 tdreszer Exp $";

struct imgBox   *theImgBox   = NULL; // Make this global for now to avoid huge rewrite
//struct image    *theOneImg   = NULL; // Make this global for now to avoid huge rewrite
struct imgTrack *curImgTrack = NULL; // Make this global for now to avoid huge rewrite
//struct imgSlice *curSlice    = NULL; // Make this global for now to avoid huge rewrite
//struct mapSet   *curMap      = NULL; // Make this global for now to avoid huge rewrite
//struct mapItem  *curMapItem  = NULL; // Make this global for now to avoid huge rewrite

//#define IMAGEv2_UI
#ifdef IMAGEv2_UI
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

struct mapItem *mapSetItemUpdate(struct mapSet *map,struct mapItem *item,char *link,char *title,int topLeftX,int topLeftY,int bottomRightX,int bottomRightY)
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
return item;
}

struct mapItem *mapSetItemAdd(struct mapSet *map,char *link,char *title,int topLeftX,int topLeftY,int bottomRightX,int bottomRightY)
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
slAddHead(&(map->items),item);
//warn("Added map(%s) item '%s' count:%d",map->name,title,slCount(map->items));
return map->items;
}

struct mapItem *mapSetItemUpdateOrAdd(struct mapSet *map,char *link,char *title,int topLeftX,int topLeftY,int bottomRightX,int bottomRightY)
/* Update or add a single mapItem */
{
struct mapItem *item = mapSetItemFind(map,topLeftX,topLeftY,bottomRightX,bottomRightY);
if(item != NULL)
    return mapSetItemUpdate(map,item,link,title,topLeftX,topLeftY,bottomRightX,bottomRightY);
else
    return mapSetItemAdd(map,link,title,topLeftX,topLeftY,bottomRightX,bottomRightY);
}

struct mapItem *mapSetItemFindOrAdd(struct mapSet *map,char *link,char *title,int topLeftX,int topLeftY,int bottomRightX,int bottomRightY)
/* Finds or adds the map item */
{
struct mapItem *item = mapSetItemFind(map,topLeftX,topLeftY,bottomRightX,bottomRightY);
if(item != NULL)
    return item;
else
    return mapSetItemAdd(map,link,title,topLeftX,topLeftY,bottomRightX,bottomRightY);
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
struct imgSlice *slice;
AllocVar(slice);
slice->map       = NULL; // This is the same as defaulting to slice->parentImg->map
return sliceUpdate(slice,type,img,title,width,height,offsetX,offsetY);
}

struct imgSlice *sliceUpdate(struct imgSlice *slice,enum sliceType type,struct image *img,char *title,int width,int height,int offsetX,int offsetY)
/* updates an already created slice */
{
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
    case isData:   return "data";
    case isSide:   return "side";
    case isCenter: return "center";
    default:       return "unknown";
    }
}

struct mapSet *sliceMapStart(struct imgSlice *slice,char *name,char *linkRoot)
/* Adds a slice specific map to a slice of an image. Map items can then be added to the returned pointer with mapSetItemAdd()*/
{
if(slice->parentImg == NULL)
    {
    warn("sliceAddMap() but slice has no image.");
    return NULL;
    }
if(slice->map != NULL && slice->map != slice->parentImg->map)
    {
    warn("sliceAddMap() but slice already has its own map. Being replaced.");
    mapSetFree(&(slice->map));
    }
char qualifiedName[128];
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
char qualifiedName[128];
safef(qualifiedName,sizeof(qualifiedName),"%s_%s",sliceTypeToString(slice->type),name);
return mapSetUpdate(map,qualifiedName,slice->parentImg,linkRoot);
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
if (slice->parentImg == NULL)
    {
    if (verbose)
        warn("slice(%s) has no image",sliceTypeToString(slice->type));
    return FALSE;
    }
if (slice->width == 0  || slice->width  > slice->parentImg->width)
    {
    if (verbose)
        warn("slice(%s) has an invalid width %d (image width %d)",
             sliceTypeToString(slice->type),slice->width,slice->parentImg->width);
    return FALSE;
    }
if (slice->height == 0) // FIXME: This may be a temporary solution to empty data slices
    {
    //if (verbose)
    //    warn("slice(%s) has an invalid height %d (image height %d)",
    //         sliceTypeToString(slice->type),slice->height,slice->parentImg->height);
    //return FALSE;
    return TRUE; // This may be valid (but is sloppy) when there is no data for the slice.
    }
if (slice->height > slice->parentImg->height)
    {
    if (verbose)
        warn("slice(%s) has an invalid height %d (image height %d)",
             sliceTypeToString(slice->type),slice->height,slice->parentImg->height);
    return FALSE;
    }
if (slice->offsetX >= slice->parentImg->width
||  slice->offsetY >= slice->parentImg->height)
    {
    if (verbose)
        warn("slice(%s) has an invalid X:%d or Y:%d offset (image width:%d height:%d)",
             sliceTypeToString(slice->type),slice->offsetX,slice->offsetY,
             slice->parentImg->width,slice->parentImg->height);
    return FALSE;
    }
if (slice->map != NULL)
    {
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
    freeMem(slice);
    *pSlice = NULL;
    }
}



/////////////////////// imgTracks

struct imgTrack *imgTrackStart(struct trackDb *tdb,char *name,char *db,char *chrom,int chromStart,int chromEnd,boolean plusStrand,boolean showCenterLabel,enum trackVisibility vis,int order)
/* Starts an image track which will contain all image slices needed to render one track
   Must completed by adding slices with imgTrackAddSlice() */
{
struct imgTrack *imgTrack;     //  gifTn.forHtml, pixWidth, mapName
AllocVar(imgTrack);
return imgTrackUpdate(imgTrack,tdb,name,db,chrom,chromStart,chromEnd,plusStrand,showCenterLabel,vis,order);
}

struct imgTrack *imgTrackUpdate(struct imgTrack *imgTrack,struct trackDb *tdb,char *name,char *db,char *chrom,int chromStart,int chromEnd,boolean plusStrand,boolean showCenterLabel,enum trackVisibility vis,int order)
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
imgTrack->showCenterLabel = showCenterLabel;
imgTrack->vis             = vis;
static int lastOrder = 900; // keep track of the order these images get added
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
#ifdef IMAGEv2_DRAG_REORDER
    imgTrack->reorderable = TRUE;
#else//ifndef IMAGEv2_DRAG_REORDER
    imgTrack->reorderable = FALSE;
#endif//ndef IMAGEv2_DRAG_REORDER
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

int imgTrackOrderCmp(const void *va, const void *vb)
/* Compare to sort on label. */
{
const struct imgTrack *a = *((struct imgTrack **)va);
const struct imgTrack *b = *((struct imgTrack **)vb);
return (a->order - b->order);
}

struct imgSlice *imgTrackSliceAdd(struct imgTrack *imgTrack,enum sliceType type, struct image *img,char *title,int width,int height,int offsetX,int offsetY)
/* Adds slices to an image track.  Expected are types: isData, isSide and isCenter */
{
struct imgSlice *slice = sliceCreate(type,img,title,width,height,offsetX,offsetY);
slAddHead(&(imgTrack->slices),slice);
return imgTrack->slices;
}

struct imgSlice *imgTrackSliceGetByType(struct imgTrack *imgTrack,enum sliceType type)
/* Gets a specific slice already added to an image track.  Expected are types: isData, isSide and isCenter */
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

int imgTrackAddMapItem(struct imgTrack *imgTrack,char *link,char *title,int topLeftX,int topLeftY,int bottomRightX,int bottomRightY)
/* Will add a map item it an imgTrack's appropriate slice's map
   Since a map item may span slices, the imgTrack is in the best position to determine where to put the map item
   returns count of map items added, which could be 0, 1 or more than one if item spans slices
   NOTE: Precedence is given to first map item when adding items with same coordinates! */
{
struct imgSlice *slice;
char *imgFile = NULL;               // name of file that hold the image

int count = 0;
for(slice = imgTrack->slices;slice != NULL;slice=slice->next)
    {
    if(imgFile == NULL)
        imgFile = slice->parentImg->file;
    else if(differentString(imgFile,slice->parentImg->file))
        {
        char * name = (imgTrack->name != NULL ? imgTrack->name : imgTrack->tdb != NULL ? imgTrack->tdb->tableName : imgFile);
        warn("imgTrackAddMapItem(%s) called, but not all slice images are the same for this track.",name);
        }
    if(topLeftX     < (slice->offsetX + slice->width)
    && bottomRightX >= slice->offsetX
    && topLeftY     < (slice->offsetY + slice->height)
    && bottomRightY >= slice->offsetY ) // some overlap
        {
        struct mapSet *map = sliceGetMap(slice,FALSE);
        if(map!=NULL)
        {          // NOTE: using find or add gives precedence to first of same coordinate map items added
            mapSetItemFindOrAdd(map,link,title,max(topLeftX,slice->offsetX),max(topLeftY,slice->offsetY),min(bottomRightX,slice->offsetX + slice->width),min(bottomRightY,slice->offsetY + slice->height));
            count++;
            }
        }
    }
//if(count>=2)
//    {
//    char * name = (imgTrack->name != NULL ? imgTrack->name : imgTrack->tdb != NULL ? imgTrack->tdb->tableName : imgFile);
//    warn("imgTrackAddMapItem(%s) called for map items stretching across %d slice(s).",name,count);
//    }
return count;
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
        warn("imgTrack has no tdb or name");
    return FALSE;
    }
char * name = (imgTrack->name != NULL ? imgTrack->name : imgTrack->tdb->tableName);
if (imgTrack->db == NULL)
    {
    if (verbose)
        warn("imgTrack(%s) has no db.",name);
    return FALSE;
    }
if (imgTrack->db == NULL)
    {
    if (verbose)
        warn("imgTrack(%s) has no chrom.",name);
    return FALSE;
    }
if (imgTrack->chromStart  >= imgTrack->chromEnd)
    {
    if (verbose)
        warn("imgTrack(%s) for %s.%s:%d-%d has bad genome range.",name,
             imgTrack->db,imgTrack->chrom,imgTrack->chromStart,imgTrack->chromEnd);
    return FALSE;
    }
if(imgTrack->slices == NULL)
    {
    if (verbose)
        warn("imgTrack(%s) has no slices.",name);
    return FALSE;
    }
// Can have no more than one of each type of slice
boolean found[isMaxSliceTypes] = { FALSE,FALSE,FALSE,FALSE};
struct imgSlice *slice = imgTrack->slices;
for(; slice != NULL; slice = slice->next )
    {
    if(found[slice->type])
        {
        if (verbose)
            warn("imgTrack(%s) found more than one slice of type %s.",
                 name,sliceTypeToString(slice->type));
        return FALSE;
        }
    found[slice->type] = TRUE;

    if(!sliceIsConsistent(slice,verbose))
        {
        if (verbose)
            warn("imgTrack(%s) has bad slice",name);
        return FALSE;
        }
    }
// This is not a requirement as the data portion could be empty (height==0) FIXME This still needs to be properly resolved
//if(!found[isData])
//    {
//    if (verbose)
//        warn("imgTrack(%s) has no DATA slice.",name);
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

struct imgTrack *imgBoxTrackAdd(struct imgBox *imgBox,struct trackDb *tdb,char *name,enum trackVisibility vis,boolean showCenterLabel,int order)
/* Adds an imgTrack to an imgBox.  The imgTrack needs to be extended with imgTrackAddSlice() */
{
struct imgTrack *imgTrack = imgTrackStart(tdb,name,imgBox->db,imgBox->chrom,imgBox->chromStart,imgBox->chromEnd,imgBox->plusStrand,showCenterLabel,vis,order);
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

struct imgTrack *imgBoxTrackFindOrAdd(struct imgBox *imgBox,struct trackDb *tdb,char *name,enum trackVisibility vis,boolean showCenterLabel,int order)
/* Find the imgTrack, or adds it if not found */
{
struct imgTrack *imgTrack = imgBoxTrackFind(imgBox,tdb,name);
if( imgTrack == NULL)
    imgTrack = imgBoxTrackAdd(imgBox,tdb,name,vis,showCenterLabel,order);
return imgTrack;
}

struct imgTrack *imgBoxTrackUpdateOrAdd(struct imgBox *imgBox,struct trackDb *tdb,char *name,enum trackVisibility vis,boolean showCenterLabel,int order)
/* Updates the imgTrack, or adds it if not found */
{
struct imgTrack *imgTrack = imgBoxTrackFind(imgBox,tdb,name);
if( imgTrack == NULL)
    return imgBoxTrackAdd(imgBox,tdb,name,vis,showCenterLabel,order);

return imgTrackUpdate(imgTrack,tdb,name,imgBox->db,imgBox->chrom,imgBox->chromStart,imgBox->chromEnd,imgBox->plusStrand,showCenterLabel,vis,order);
}

// TODO: Will we need this?
//boolean imgBoxTrackRemove(struct imgBox *imgBox,struct imgTrack *imgTrack)
//{
//return slRemoveEl(&(imgBox->imgTracks),imgTrack);
//}

void imgBoxTracksNormalizeOrder(struct imgBox *imgBox)
/* This routine sorts the imgTracks then forces tight ordering, so new tracks wil go to the end */
{
#ifdef IMAGEv2_DRAG_REORDER
slSort(&(imgBox->imgTracks), imgTrackOrderCmp);
struct imgTrack *imgTrack = NULL;
int lastOrder = 0;
for (imgTrack = imgBox->imgTracks; imgTrack != NULL; imgTrack = imgTrack->next )
    {
    if(imgTrack->reorderable)
        imgTrack->order = ++lastOrder;
    }
#else//ifndef IMAGEv2_DRAG_REORDER
slReverse(&(imgBox->imgTracks));
#endif//ndef IMAGEv2_DRAG_REORDER
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
struct imgTrack *imgTrack = NULL;
for (imgTrack = imgBox->imgTracks; imgTrack != NULL; imgTrack = imgTrack->next )
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
    // Every track must have slices
    if (imgTrack->slices == NULL)
        {
        if (verbose)
            warn("imgBox(%s.%s:%d-%d) has no slices",
                imgBox->db,imgBox->chrom,imgBox->chromStart,imgBox->chromEnd);
        return FALSE;
        }
    struct imgSlice *slice = NULL;
    for (slice = imgTrack->slices; slice != NULL; slice = slice->next )
        {
        if(!sliceIsConsistent(slice,verbose))
            {
            if (verbose)
                warn("imgBox(%s.%s:%d-%d) has bad slice",imgBox->db,imgBox->chrom,imgBox->chromStart,imgBox->chromEnd);
            return FALSE;
            }
        // Every slice must point to an image owned by the imgBox
        if(slIxFromElement(imgBox->images,slice->parentImg) == -1)
            {
            if (verbose)
                warn("imgBox(%s.%s:%d-%d) has slice(%s) for unknown image (%s)",
                    imgBox->db,imgBox->chrom,imgBox->chromStart,imgBox->chromEnd,
                    sliceTypeToString(slice->type),slice->parentImg->file);
            return FALSE;
            }
        }
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

void imageMapDraw(struct mapSet *map,char *name)
/* writes an image map as HTML */
{
//warn("Drawing map_%s %s",name,(map == NULL?"map is NULL":map->items == NULL?"map->items is NULL":"Should draw!"));
if(map == NULL || map->items == NULL)
    return;

hPrintf("  <MAP name='map_%s'>\n", name); // map_ prefix is implicit
struct mapItem *item = map->items;
for(;item!=NULL;item=item->next)
    {
    hPrintf("   <AREA SHAPE=RECT COORDS='%d,%d,%d,%d'",
           item->topLeftX, item->topLeftY, item->bottomRightX, item->bottomRightY);
    // TODO: remove static portion of the link and handle in js
    if(map->linkRoot != NULL)
        hPrintf(" HREF='%s%s'",map->linkRoot,(item->linkVar != NULL?item->linkVar:""));
    else if(item->linkVar != NULL)
        hPrintf(" HREF='%s'",item->linkVar);
    else
        warn("map item has no url!");

    if(item->title != NULL)
        hPrintf(" TITLE='%s'", item->title );
    hPrintf(">\n" );
    }
hPrintf("  </MAP>\n");
}

void sliceAndMapDraw(struct imgBox *imgBox,struct imgSlice *slice,char *name,boolean scrollHandle)
/* writes a slice of an image and any assocated image map as HTML */
{
if(slice==NULL || slice->height == 0)
    return;
// Adjustment for portal
int offsetX=slice->offsetX;
int width=slice->width;
if(imgBox->showPortal && imgBox->basesPerPixel > 0
&& (slice->type==isData || slice->type==isCenter))
    {
    offsetX += (imgBox->portalStart - imgBox->chromStart) / imgBox->basesPerPixel;
    width=imgBox->portalWidth;
    }

hPrintf(" <div style='width:%dpx; height:%dpx; overflow:hidden;'",width,slice->height);
#ifdef IMAGEv2_DRAG_SCROLL
if(imgBox->showPortal && slice->type==isData)
    {
    if(scrollHandle)
        hPrintf(" class='panDiv scroller'");
    else
        hPrintf(" class='panDiv'");
    }
#endif //def IMAGEv2_DRAG_SCROLL
hPrintf(">\n");

struct mapSet *map = sliceGetMap(slice,FALSE); // Could be the image map or slice specific
if(map)
    imageMapDraw(map,name);

hPrintf("  <IMG id='img_%s' src='%s' style='position:relative; left:-%dpx; top: -%dpx; border:0;'",
        name,slice->parentImg->file,offsetX,slice->offsetY);

if(map && map->items)
    hPrintf(" usemap='#map_%s'",name);
if(slice->type==isSide)
    hPrintf(" class='sideLab'");
else if(slice->type==isCenter)
    hPrintf(" class='centerLab'");
#ifdef IMAGEv2_DRAG_SCROLL
else if(slice->type==isData && imgBox->showPortal)
    hPrintf(" class='panImg' ondrag='{return false;}'");
#endif //def IMAGEv2_DRAG_SCROLL
//hPrintf(" title='[%s] width:%d  height: %d  offsetX: %d  offsetY: %d'",
//        sliceTypeToString(slice->type),slice->width,slice->height,slice->offsetX,slice->offsetY);
if(slice->title != NULL)
    hPrintf(" title='%s'",slice->title);           // Adds slice wide title
else if(slice->parentImg->title != NULL)
    hPrintf(" title='%s'",slice->parentImg->title);// Adds image wide title
hPrintf("></div>\n");
}

void imageBoxDraw(struct imgBox *imgBox)
/* writes a entire imgBox including all tracksas HTML */
{
if(!imgBoxIsComplete(imgBox,TRUE))
    return;
char name[128];

imgBoxTracksNormalizeOrder(imgBox);

hPrintf("<!---------------vvv IMAGEv2 vvv---------------->\n");
//commonCssStyles();
#ifdef IMAGEv2_DRAG_REORDER
jsIncludeFile("jquery.tablednd.js", NULL);
#endif//def IMAGEv2_DRAG_REORDER
hPrintf("<style type='text/css'>\n");
#ifdef IMAGEv2_DRAG_REORDER
hPrintf(".trDrag {opacity:0.4; padding:1px; background-color:red;}\n");// outline:red solid thin;}\n"); // opacity for FF, padding/bg for IE
hPrintf(".dragHandle {cursor: s-resize;}\n");
#endif//def IMAGEv2_DRAG_REORDER
hPrintf("div.dragZoom {cursor: text;}\n");
hPrintf("</style>\n");

#ifdef IMAGEv2_DRAG_SCROLL
if(imgBox->showPortal)
    {
    hPrintf("<script type='text/javascript'>var imgBoxPortal=true;");
    hPrintf("var imgBoxChromStart=%d;var imgBoxChromEnd=%d;var imgBoxWidth=%d;",
            imgBox->chromStart, imgBox->chromEnd,(imgBox->width - imgBox->sideLabelWidth));
    hPrintf("var imgBoxPortalStart=%d;var imgBoxPortalEnd=%d;var imgBoxPortalWidth=%d;",
            imgBox->portalStart, imgBox->portalEnd, imgBox->portalWidth);
    hPrintf("var imgBoxLeftLabel=%d;var imgBoxPortalOffsetX=%d;var imgBoxBasesPerPixel=%lf;</script>\n",
            (imgBox->plusStrand?imgBox->sideLabelWidth:0),
            (int)((imgBox->portalStart - imgBox->chromStart) / imgBox->basesPerPixel),imgBox->basesPerPixel);
    }
#endif//def IMAGEv2_DRAG_SCROLL

hPrintf("<TABLE id='imgTbl' border=0 cellspacing=0 cellpadding=0");
hPrintf(" width=%d",imgBox->showPortal?(imgBox->portalWidth+imgBox->sideLabelWidth):imgBox->width);
#ifdef IMAGEv2_DRAG_REORDER
hPrintf(" class='tableWithDragAndDrop'");
#endif//def IMAGEv2_DRAG_REORDER
hPrintf(" style='border:1px solid blue;border-collapse:separate'");
hPrintf(">\n");

struct imgTrack *imgTrack = imgBox->imgTracks;
for(;imgTrack!=NULL;imgTrack=imgTrack->next)
    {
    char *trackName = (imgTrack->name != NULL ? imgTrack->name : imgTrack->tdb->tableName );
    hPrintf("<TR id='tr_%s'%s>\n",trackName,
        (imgTrack->reorderable?" class='trDraggable'":" class='nodrop nodrag'"));
    // leftLabel
    if(imgBox->showSideLabel && imgBox->plusStrand)
        {
        safef(name,sizeof(name),"side_%s",trackName);
        hPrintf(" <TD id='td_%s'%s>\n",name,
            (imgTrack->reorderable?" class='dragHandle' title='Drag to reorder'":""));
        sliceAndMapDraw(imgBox,imgTrackSliceGetByType(imgTrack,isSide), name,FALSE);
        hPrintf(" </TD>");
        }
    // Main/Data image region
    hPrintf(" <TD id='td_data_%s' width=%d>\n", trackName, imgBox->width);
    hPrintf("  <input TYPE=HIDDEN name='%s_%s' value='%d'>\n",trackName,IMG_ORDER_VAR,imgTrack->order);
    // centerLabel
    if(imgTrack->showCenterLabel)
        {
        safef(name, sizeof(name), "center_%s", trackName);
        sliceAndMapDraw(imgBox,imgTrackSliceGetByType(imgTrack,isCenter), name,FALSE);
        //hPrintf("<BR>\n");
        }
    // data image
    safef(name, sizeof(name), "data_%s", trackName);
    sliceAndMapDraw(imgBox,imgTrackSliceGetByType(imgTrack,isData), name,(imgTrack->order>0));
    hPrintf(" </TD>");

    // rightLabel
    if(imgBox->showSideLabel && !imgTrack->plusStrand)
        {
        safef(name, sizeof(name), "side_%s", trackName);
        hPrintf(" <TD id='td_%s'%s>\n", name,
            (imgTrack->reorderable?" class='dragHandle' title='Drag to reorder'":""));
        sliceAndMapDraw(imgBox,imgTrackSliceGetByType(imgTrack,isSide), name,FALSE);
        hPrintf(" </TD>\n");
        }
    hPrintf("</TR>\n");
    }
hPrintf("</TABLE>\n");
hPrintf("<!---------------^^^ IMAGEv2 ^^^---------------->\n");
}

#endif//def IMAGEv2_UI
