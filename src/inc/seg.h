/* seg.h - Segment file format. */
#ifndef SEG_H
#define SEG_H

#include "common.h"

struct segComp
/* A genomic segment. */
	{
	struct segComp *next;
	char *src;		/* Name of segment source. */
	int start;		/* Start of segment. Zero based. If strand is - it is */
					/*   relative to source end.*/
	int size;		/* Size of segment. */
	char strand;	/* Strand of segment. Either + or -. */
	int srcSize;	/* Size of segment source. */
	};

struct segBlock
/* A list of genomic segments. */
	{
	struct segBlock *next;
	struct segBlock *prev;
	char *name;					/* Name of this segment list. */
	int val;					/* Integer value for this segment list. */
	struct segComp *components;	/* List of segments in this segment list. */
	};

struct segFile
/* A file full of genomic segments. */
	{
	struct segFile *next;
	int version;				/* segFile version. */
	struct segBlock *blocks;	/* Possibly empty list of segment blocks. */
	struct lineFile *lf;		/* Open lineFile if any. */
	};

void segCompFree(struct segComp **pObj);
/* Free up a segment component. */

void segCompFreeList(struct segComp **pList);
/* Free up a list of segment components. */

void segBlockFree(struct segBlock **pObj);
/* Free up a segment block. */

void segBlockFreeList(struct segBlock **pList);
/* Free up a list of segment blocks. */

void segFileFree(struct segFile **pObj);
/* Free up a segment file including closing file handle if necessary. */

void segFileFreeList(struct segFile **pList);
/* Free up a list of segment files. */

struct segFile *segMayOpen(char *fileName);
/* Open up a segment file for reading. Read header and verify. Prepare
 * for subsequent calls to segNext(). Return NULL if file does not exist. */

struct segFile *segOpen(char *fileName);
/* Like segMayOpen() above, but prints an error message and aborts if
 * there is a problem. */

void segRewind(struct segFile *sf);
/* Seek to beginning of open segment file */

struct segBlock *segNextWithPos(struct segFile *sf, off_t *retOffset);
/* Return next segment in segment file or NULL if at end. If retOffset
 * is not NULL, return start offset of record in file. */

struct segBlock *segNext(struct segFile *sf);
/* Return next segment in segment file or NULL if at end.  This will
 * close the open file handle at the end as well. */

struct segFile *segReadAll(char *fileName);
/* Read in full segment file */

void segWriteStart(FILE *f);
/* Write segment file header to the file. */

void segWrite(FILE *f, struct segBlock *block);
/* Write next segment block to the file. */

void segWriteEnd(FILE *f);
/* Write segment file end tag to the file. */

struct segComp *segMayFindCompSpecies(struct segBlock *sb, char *src,
	char sepChar);
/* Find component with a source that matches src up to and possibly
   including sepChar. Return NULL if not found. */

struct segComp *segFindCompSpecies(struct segBlock *sb, char *src,
	char sepChar);
/* Find component with a source that matches src up to and possibly
   including sepChar or die trying. */

struct segComp *cloneSegComp(struct segComp *sc);
/* Return a copy of the argument segment component. */

char *segFirstCompSpecies(struct segBlock *sb, char sepChar);
/* Return the species possibly followed by sepChar of the first component
   of the argument block. Return NULL if the block has no components. */

struct slName *segSecSpeciesList(struct segBlock *sb, struct segComp *refComp,
	char sepChar);
/* Return a name list containing the species possibly followed by sepChar
of all components other than refComp on the block. */

#endif /* SEG_H */
