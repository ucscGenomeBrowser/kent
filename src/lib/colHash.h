/* colHash - stuff for fast lookup of index given an
 * rgb value. */
#ifndef COLHASH_H
#define COLHASH_H

#define colHashFunc(r,g,b) (r+g+g+b)

struct colHashEl
/* An element in a color hash. */
    {
    struct colHashEl *next;	/* Next in list. */
    struct rgbColor col;	/* Color RGB. */
    int ix;			/* Color Index. */
    };

struct colHash
/* A hash on RGB colors. */
    {
    struct colHashEl *lists[4*256];	/* Hash chains. */
    struct colHashEl elBuf[256];	/* Buffer of elements. */
    struct colHashEl *freeEl;		/* Pointer to next free element. */
    };

struct colHash *colHashNew();
/* Get a new color hash. */

void colHashFree(struct colHash **pEl);
/* Free up color hash. */

struct colHashEl *colHashAdd(struct colHash *cHash, 
	unsigned r, unsigned g, unsigned b, int ix);
/* Add new element to color hash. */

struct colHashEl *colHashLookup(struct colHash *cHash, 
	unsigned r, unsigned g, unsigned b);
/* Lookup value in hash. */

#endif /* COLHASH_H */
