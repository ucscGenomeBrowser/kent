#include "common.h"

/* fill a specified area of memory with zeroes */
void zeroBytes(vpt, count)
void *vpt;
int count;
{
char *pt = (char*)vpt;
while (--count>=0)
	*pt++=0;
}

/* Reverse the order of the bytes. */
void reverseBytes(bytes, length)
char *bytes;
long length;
{
long halfLen = (length>>1);
char *end = bytes+length;
char c;
while (--halfLen >= 0)
    {
    c = *bytes;
    *bytes++ = *--end;
    *end = c;
    }
}


/* Return how long the named file is in bytes. 
 * Return -1 if no such file. */
long fileSize(fileName)
char *fileName;
{
int fd;
long size;
fd = open(fileName, O_RDONLY, 0);
if (fd < 0)
    return -1;
size = lseek(fd, 0L, 2);
close(fd);
return size;
}

/** List managing routines. */

/* Count up elements in list. */
int slCount(list)
void *list;	/* Really better be a list... */
{
struct slList *pt = (struct slList *)list;
int len = 0;

while (pt != NULL)
    {
    len += 1;
    pt = pt->next;
    }
return len;
}

/* Return the ix'th element in list.  Returns NULL
 * if no such element. */
void *slElementFromIx(list, ix)
void *list;
int ix;
{
struct slList *pt = list;
int i;
for (i=0;i<ix;i++)
    {
    if (pt == NULL) return NULL;
    pt = pt->next;
    }
return pt;
}


/* Add new node to start of list.
 * Usage:
 *    slAddHead(&list, node);
 * where list and nodes are both pointers to structure
 * that begin with a next pointer. 
 */
void slAddHead(listPt, node)
void **listPt;
void *node;
{
struct slList **ppt = (struct slList **)listPt;
struct slList *n = (struct slList *)node;

n->next = *ppt;
*ppt = n;
}

/* Add new node to tail of list.
 * Usage:
 *    slAddTail(&list, node);
 * where list and nodes are both pointers to structure
 * that begin with a next pointer. 
 */
void slAddTail(listPt, node)
void **listPt;
void *node;
{
struct slList **ppt = (struct slList **)listPt;
struct slList *n = (struct slList *)node;

while (*ppt != NULL)
    {
    ppt = &((*ppt)->next);
    }
n->next = NULL;	
*ppt = n;
}

/* Free list */
void slFreeList(listPt)
void **listPt;
{
struct slList **ppt = (struct slList**)listPt;
struct slList *next = *ppt;
struct slList *el;

while (next != NULL)
    {
    el = next;
    next = el->next;
    free(el);
    }
*ppt = NULL;
}

void gentleFree(pt)
void *pt;
{
if (pt != NULL) free(pt);
}
