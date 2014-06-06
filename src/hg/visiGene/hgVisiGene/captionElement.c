/* captionElement - this module helps us sort out 
 * which elements in a caption are true for all images
 * (panes) in a file, and which need to be reported
 * separately for each image. */

/* Copyright (C) 2005 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "captionElement.h"

struct captionElement *captionElementNew(int image, char *type, char *value)
/* Create new captionElement.  This takes over ownership of value's memory. */
{
struct captionElement *ce;
AllocVar(ce);
ce->image = image;
ce->type = cloneString(type);
ce->value = value;
return ce;
}

void captionElementFree(struct captionElement **pCe)
/* Free up memory associated with caption element. */
{
struct captionElement *ce = *pCe;
if (ce != NULL)
    {
    freeMem(ce->type);
    freeMem(ce->value);
    freez(pCe);
    }
}

void captionElementFreeList(struct captionElement **pList)
/* Free up a list of captionElement. */
{
struct captionElement *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    captionElementFree(&el);
    }
*pList = NULL;
}

boolean captionElementTypeExists(struct captionElement *list, char *type)
/* Return TRUE if element of type exists in list. */
{
struct captionElement *ce;
for (ce = list; ce != NULL; ce = ce->next)
    if (sameString(ce->type, type))
        return TRUE;
return FALSE;
}


boolean captionElementCommon(struct captionElement *list, char *type,
	struct slInt *imageList)
/* If the value of the caption element of given type is the same for 
 * all images, then return TRUE. */
{
struct captionElement *ce;
char *common = NULL;
int imageCount = slCount(imageList);
int sameCount = 0;
for (ce = list; ce != NULL; ce = ce->next)
    {
    if (sameString(ce->type, type))
        {
	if (common == NULL)
	    {
	    common = ce->value;
	    sameCount = 1;
	    }
	else
	    {
	    if (sameString(common, ce->value))
		sameCount += 1;
	    }
	}
    }
return sameCount == imageCount;
}

void captionBundleFree(struct captionBundle **pBundle)
/* Free up memory associated with captionBundle. */
{
struct captionBundle *bundle = *pBundle;
if (bundle != NULL)
    {
    slFreeList(&bundle->elements);
    freez(pBundle);
    }
}

void captionBundleFreeList(struct captionBundle **pList)
/* Free up a list of captionBundles. */
{
struct captionBundle *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    captionBundleFree(&el);
    }
*pList = NULL;
}

struct captionBundle *captionElementBundle(struct captionElement *ceList,
	struct slInt *imageList)
/* This organizes the caption elements into those common to all
 * images and those specific for an image.  The common bundle if
 * any will be the first bundle in the returned list. */
{
struct captionBundle *bundleList = NULL, *bundle;
int firstImage = imageList->val;
struct slInt *image;
struct captionElement *ce;

/* Build up common list. */
for (ce = ceList; ce != NULL; ce = ce->next)
    {
    if (ce->image == firstImage)
        {
	if (captionElementCommon(ceList, ce->type, imageList))
	    {
	    if (bundleList == NULL)
		AllocVar(bundleList);
	    refAdd(&bundleList->elements, ce);
	    }
	}
    }

/* Build up other lists. */
for (image = imageList; image != NULL; image = image->next)
    {
    int imageId = image->val;
    bundle = NULL;
    for (ce = ceList; ce != NULL; ce = ce->next)
        {
	if (ce->image == imageId)
	    {
	    if (!captionElementCommon(ceList, ce->type, imageList))
	        {
		if (bundle == NULL)
		    {
		    AllocVar(bundle);
		    bundle->image = imageId;
		    }
		refAdd(&bundle->elements, ce);
		}
	    }
	}
    if (bundle != NULL)
        {
	slAddHead(&bundleList, bundle);
	}
    }

/* Reverse lists since built adding to head. */
slReverse(&bundleList);
for (bundle = bundleList; bundle != NULL; bundle = bundle->next)
    slReverse(&bundle->elements);
return bundleList;
}

