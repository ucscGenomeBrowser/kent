#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>

#define TRUE 1
#define FALSE 0
#define boolean int
#define ArraySize(a) (sizeof(a)/sizeof((a)[0]))

#define uglyf printf  /* debugging printf */

void zeroBytes();     /* fill a specified area of memory with zeroes */
void fillBytes();     /* fill a specified area of memory with char */
void reverseBytes();  /* Reverse the order of the bytes. */
long fileSize();      /* Return the size of a file (from it's name) */



/* Some things to manage simple lists - structures that begin
 * with a pointer to the next element in the list. */
struct slList
    {
    struct slList *next;
    };

int slCount();   /* Return # of elements in list.  */
/* Parameters:
 *     list - pointer to first element in list.
 */


void *slElementFromIx();	/* Return a specific element from list. */
/* Parameters:
 *     list - pointer to first element in list.
 *     ix   - the index of element you want.
 * Returns: The ix'th element. NULL if list is too short. */


void slAddHead();
/* Add new node to start of list.
 * Usage:
 *    slAddHead(&list, node);
 * where list and nodes are both pointers to structure
 * that begin with a next pointer. 
 */

void slAddTail();
/* Add new node to tail of list.
 * Usage:
 *    slAddTail(&list, node);
 * where list and nodes are both pointers to structure
 * that begin with a next pointer. 
 */

void slReverseList();
/* Reverse order of list. 
 * Usage:
 *   slReverseList(&list);
 */

void slFreeList();
/* Free all elements in list and set list pointer to null. 
 * Usage:
 *    slFreeList(&list);
 */

void *needMemory();
/* Ask for memory (set to zeroes initially).
 * Exit program with error message if it's not there. */

#define AllocA(type) ((type*)needMemory(sizeof(type)))

void gentleFree();
/* check pointer for NULL before freeing. */
