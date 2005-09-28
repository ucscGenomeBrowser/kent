/* captionElement - this module helps us sort out 
 * which elements in a caption are true for all images
 * (panes) in a file, and which need to be reported
 * separately for each image. */

#include "common.h"
#include "captionElement.h"

struct captionElement *captionElementNew(int image, char *type, char *value)
/* Create new captionElement.  This takes over ownership of value's memory. */
{
struct captionElement *ce;
AllocVar(ce);
ce->image = image;
ce->type = type;
ce->value = value;
return ce;
}

void captionElementFree(struct captionElement **pCe)
/* Free up memory associated with caption element. */
{
struct captionElement *ce = *pCe;
if (ce != NULL)
    {
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


char *captionElementCommon(struct captionElement *list, char *type)
/* If the value of the caption element of given type is the same for 
 * all images, then return that value,  else return NULL. */
{
struct captionElement *ce;
char *common = NULL;
for (ce = list; ce != NULL; ce = ce->next)
    {
    if (sameString(ce->type, type))
        {
	if (common == NULL)
	    common = ce->value;
	else
	    {
	    if (!sameString(common, ce->value))
	        return NULL;
	    }
	}
    }
return common;
}

