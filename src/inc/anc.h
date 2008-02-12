/* anc.h - Anchor file format. */
#ifndef ANC_H
#define ANC_H

#include "common.h"

struct ancFile
/* A file full of anchors. */
	{
	struct ancFile *next;
	int version;				/* Required */
	int minLen;					/* Minimum length (bp) of anchors in this file. */
	struct ancBlock *anchors;	/* Possibly empty list of anchors. */
	struct lineFile *lf;		/* Open line file if any. NULL except while parsing */
	};

void ancFileFree(struct ancFile **pObj);
/* Free up an anchor file including closing file handle if necessary. */

void ancFileFreeList(struct ancFile **pList);
/* Free up a list of anchor files. */

struct ancBlock
/* An anchor block. */
	{
	struct ancBlock *prev;
	struct ancBlock *next;
	int ancLen;					/* Length of this anchor. */
	struct ancComp *components; /* List of components of anchor. */
	};

void ancBlockFree(struct ancBlock **pObj);
/* Free up an anchor block. */

void ancBlockFreeList(struct ancBlock **pList);
/* Free up a list of anchor blocks. */

struct ancComp
/* A component of an anchor. */
	{
	struct ancComp *next;
	char *src;		/* Name of sequence source */
	int start;		/* Start within sequence. Zero based.  If strand is - is relative to src end. */
	char strand;	/* Strand of sequence. Either + or - */
	int srcSize;	/* Size of sequence source */
	};

void ancCompFree(struct ancComp **pObj);
/* Free up an anchor component. */

void ancCompFreeList(struct ancComp **pList);
/* Free up a list of anchor components. */

struct ancFile *ancMayOpen(char *fileName);
/* Like ancOpen above, but returns NULL rather than aborting
 * if file does not exist. */

struct ancFile *ancOpen(char *fileName);
/* Open up a .anc file for reading. Read header and
 * verify. Prepare for subsequent calls to ancNext().
 * Prints error message and aborts if there's a problem. */

void ancRewind(struct ancFile *af);
/* Seek to beginning of open anchor file */

struct ancBlock *ancNextWithPos(struct ancFile *af, off_t *refOffset);
/* Return next anchor in FILE or NULL if at end. If refOffset is
 * nonNULL, return start offset of record in file. */

struct ancBlock *ancNext(struct ancFile *ancFile);
/* Return next anchor in file or NULL if at end.
 * This will close the open file handle at end as well. */

struct ancFile *ancReadAll(char *filename);
/* Read in full anchor file */

void ancWriteStart(FILE *f, int minLen);
/* Write anchor header and minLen. */

void ancWrite(FILE *f, struct ancBlock *block);
/* Write next anchor to file. */

void ancWriteEnd(FILE *f);
/* Write end tag of anchor file. */


#endif /* ANC_H */
