/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* snof.h - Sorted Name Offset File - stuff to handle a simple
 * indexed file.
 *
 * This accesses a file of name/offset pairs that are sorted by
 * name.  Does a binary search of file to find the offset given name.
 * Most typically this is used to do a quick lookup given an index file. 
 */ 

struct snof
/* Sorted Name Offset File structure.  Get one from snofOpen.  Use
 * with snofFindOffset.  Finish up with snofClose. */
    {
    FILE *file;
    int maxNameSize;
    int itemSize;
    int headSize;
    int endIx;
    char *first;
    char *last;
    char *less;
    char *mid;
    char *more;
    };

struct snof *snofOpen(char *indexName);
/* Open up the index file.  Returns NULL if there's any problem. */

struct snof *snofMustOpen(char *indexName);
/* Open up index file or die. */

void snofClose(struct snof **pSnof);
/* Close down the index file. */

int snofElementCount(struct snof *snof);
/* How many names are in snof file? */

long snofOffsetAtIx(struct snof *snof, int ix);
/* The offset of a particular index in file. */

char *snofNameAtIx(struct snof *snof, int ix);
/* The name at a particular index in file.  (This will be overwritten by
 * later calls to snof system. Strdup if you want to keep it.)
 */

void snofNameOffsetAtIx(struct snof *snof, int ix, char **pName, long *pOffset);
/* Get both name and offset for an index. */

boolean snofFindFirstStartingWith(struct snof *snof, char *prefix, int prefixSize,
    int *pSnofIx);
/* Find first index in snof file whose name begins with prefix. */

boolean snofFindOffset(struct snof *snof, char *name, long *pOffset);
/* Find offset corresponding with name.  Returns FALSE if no such name
 * in the index file. */

