/* captionElement - this module helps us sort out 
 * which elements in a caption are true for all images
 * (panes) in a file, and which need to be reported
 * separately for each image. */

#ifndef CAPTIONELEMENT_H
#define CAPTIONELEMENT_H

struct captionElement
/* An element in a caption.  Info on one feature in one
 * image of an image file. */
     {
     struct captionElement *next;
     int image;		/* ID of image this is associated with. */
     char *type;	/* Feature type. Memory not allocated here. */
     char *value;	/* Specific info.  Memory allocated here. */
     };

struct captionElement *captionElementNew(int image, char *type, char *value);
/* Create new captionElement.  This takes over ownership of value's memory. */

void captionElementFree(struct captionElement **pCe);
/* Free up memory associated with caption element. */

void captionElementFreeList(struct captionElement **pList);
/* Free up a list of captionElement. */

boolean captionElementTypeExists(struct captionElement *list, char *type);
/* Return TRUE if element of type exists in list. */

char *captionElementCommon(struct captionElement *list, char *type);
/* If the value of the caption element of given type is the same for 
 * all images, then return that value,  else return NULL. */

#endif /* CAPTIONELEMENT_H */

