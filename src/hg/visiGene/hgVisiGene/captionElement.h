/* captionElement - this module helps us sort out 
 * which elements in a caption are true for all images
 * (panes) in a file, and which need to be reported
 * separately for each image. */

/* Copyright (C) 2005 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef CAPTIONELEMENT_H
#define CAPTIONELEMENT_H

struct captionElement
/* An element in a caption.  Info on one feature in one
 * image of an image file. */
     {
     struct captionElement *next;
     int image;		/* ID of image this is associated with. */
     char *type;	/* Feature type. Memory allocated here. */
     char *value;	/* Specific info.  Memory allocated here. */
     boolean hasHtml;	/* True if has html tags in it. */
     };

struct captionElement *captionElementNew(int image, char *type, char *value);
/* Create new captionElement.  This takes over ownership of value's memory. */

void captionElementFree(struct captionElement **pCe);
/* Free up memory associated with caption element. */

void captionElementFreeList(struct captionElement **pList);
/* Free up a list of captionElement. */

boolean captionElementTypeExists(struct captionElement *list, char *type);
/* Return TRUE if element of type exists in list. */

boolean captionElementCommon(struct captionElement *list, char *type,
	struct slInt *imageList);
/* If the value of the caption element of given type is the same for 
 * all images, then return TRUE. */

struct captionBundle
/* A set of caption elements specific for a particular
 * image pane, or general to all panes. */
     {
     struct captionBundle *next;
     int image;		/* ID of image pane, or 0 for general stuff */
     struct slRef *elements;	/* List of references to elements. */
     };

void captionBundleFree(struct captionBundle **pBundle);
/* Free up memory associated with captionBundle. */

void captionBundleFreeList(struct captionBundle **pList);
/* Free up a list of captionBundles. */

struct captionBundle *captionElementBundle(struct captionElement *ceList,
	struct slInt *imageList);
/* This organizes the caption elements into those common to all
 * images and those specific for an image.  The common bundle if
 * any will be the first bundle in the returned list. */

#endif /* CAPTIONELEMENT_H */

