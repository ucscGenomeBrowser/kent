/* ntContig.h - A chain of clones in an NT contig. */

#ifndef NTCONTIG_H
#define NTCONTIG_H

struct ntContig
/* A chain of clones in an NT contig. */
    {
    struct ntContig *next;	/* Next in list. */
    char *name;			/* Name of NT contig. (Allocated in hash). */
    struct ntClonePos *cloneList; /* List of clones. */
    struct oogClone *psuedoClone; /* Faked clone that embraces contig. */
    int size;		        /* Size of whole contig. */
    };

struct ntClonePos
/* Info on one clone in an NT contig.
 * (NT contigs are contigs of finished clones.) */
    {
    struct ntClonePos *next;	/* Next in list. */
    char *name;                 /* Name of clone.  Allocated in hash. */
    struct oogClone *clone;	/* Clone. */
    struct ntContig *ntContig;  /* Contig this is in. */
    int pos;                    /* Start position of clone in contig. */
    int size;                   /* Size of clone in contig. */
    int orientation;            /* +1 or -1 orientation. */
    };

struct ntContig *readNtFile(char *fileName, 
	struct hash *ntContigHash, struct hash *ntCloneHash);
/* Read in NT contig info. (NT contigs are contigs of finished clones.) */

#endif /* NTCONTIG_H */

