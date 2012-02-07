/* Short short form of common.c. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "psLayHit.h"

#define sqlUnsigned atoi
/* String to unsigned. */

#define sqlSigned atoi
/* String to signed. */

#define cloneString strdup
/* Duplicate string onto heap. */

#define AllocVar(pt) (pt = malloc(sizeof(*pt)))
/* Shortcut to allocating a single variable on the heap and
 * assigning pointer to it. */


static void freeMem(void *vpt)
/* Default deallocator. */
{
if (vpt != NULL)
    free(vpt);
}

static void freez(void *vpt)
/* Pass address of pointer.  Will free pointer and set it 
 * to NULL. */
{
void **ppt = (void **)vpt;
freeMem(*ppt);
*ppt = NULL;
}


