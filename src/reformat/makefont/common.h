#ifndef COMMON_H	/* Wrapper to avoid including this twice. */
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>

/* Let's pretend C has a boolean type. */
#define TRUE 1
#define FALSE 0
#define boolean int

/* Some other type synonyms */
#define UBYTE unsigned char  /* Wants to be unsigned 8 bits. */
#define WORD short	     /* Wants to be signed 16 bits. */

/* How big is this array? */
#define ArraySize(a) (sizeof(a)/sizeof((a)[0]))

#define uglyf printf  /* debugging printf */

void zeroBytes(void *vpt, int count);     
/* fill a specified area of memory with zeroes */

void reverseBytes(char *bytes, long length);
/* Reverse the order of the bytes. */

long fileSize(char *fileName);
/* Return how long the named file is in bytes. 
 * Return -1 if no such file. */



/* Some things to manage simple lists - structures that begin
 * with a pointer to the next element in the list. */
struct slList
    {
    struct slList *next;
    };

int slCount(void *list); 
/* Return # of elements in list.  */

void *slElementFromIx(void *list, int ix);
/* Return the ix'th element in list.  Returns NULL
 * if no such element. */


void slAddHead(void *listPt, void *node);
/* Add new node to start of list.
 * Usage:
 *    slAddHead(&list, node);
 * where list and nodes are both pointers to structure
 * that begin with a next pointer. 
 */

void slAddTail(void *listPt, void *node);
/* Add new node to tail of list.
 * Usage:
 *    slAddTail(&list, node);
 * where list and nodes are both pointers to structure
 * that begin with a next pointer. 
 */

void slFreeList(void *listPt);
/* Free all elements in list and set list pointer to null. 
 * Usage:
 *    slFreeList(&list);
 */

void gentleFree(void *pt);
/* check pointer for NULL before freeing. */

void toUpperN(char *s, int n);
/* Convert a section of memory to upper case. */

void touppers(char *s);
/* Convert entire string to upper case. */

void tolowers(char *s);
/* Convert entire string to lower case */

int chopString(char *in, char *sep, char *outArray[], int outSize);
/* int chopString(in, sep, outArray, outSize); */
/* This chops up the input string (cannabilizing it)
 * into an array of zero terminated strings in
 * outArray.  It returns the number of strings. 
 * If you pass in NULL for outArray, it will just
 * return the number of strings that it *would*
 * chop. */

extern char crLfChopper[];
extern char whiteSpaceChopper[];

char *skipLeadingSpaces(char *s);
/* Return first non-white space */

void eraseTrailingSpaces(char *s);
/* Replace trailing white space with zeroes. */

void eraseWhiteSpace(char *s);
/* Remove white space from a string */

char *trimSpaces(char *s);
/* Remove leading and trailing white space. */

#endif /* COMMON_H */
