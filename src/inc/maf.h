/* maf.h - Multiple alignment format.  */
#ifndef MAF_H
#define MAF_H

struct mafFile
/* A file full of multiple alignments. */
    {
    struct mafFile *next;
    int version;		/* Required */
    char *scoring;		/* Optional (may be NULL). Name of  scoring scheme. */
    struct mafAli *alignments;	/* Possibly empty list of alignments. */
    struct lineFile *lf; 	/* Open line file if any. NULL except while parsing. */
    };

void mafFileFree(struct mafFile **pObj);
/* Free up a maf file including closing file handle if necessary. */

void mafFileFreeList(struct mafFile **pList);
/* Free up a list of maf files. */

struct mafAli
/* A multiple alignment. */
    {
    struct mafAli *next;
    double score;        /* Score.  Meaning depends on mafFile.scoring.  0.0 if no scoring. */
    struct mafComp *components;	/* List of components of alignment */
    int textSize;	 /* Size of text in each component. */
    };

void mafAliFree(struct mafAli **pObj);
/* Free up a maf alignment. */

void mafAliFreeList(struct mafAli **pList);
/* Free up a list of maf alignmentx. */

struct mafComp
/* A component of a multiple alignment. */
    {
    struct mafComp *next;
    char *src;	 /* Name of sequence source.  */
    int srcSize; /* Size of sequence source.  */
    char strand; /* Strand of sequence.  Either + or -*/
    int start;	 /* Start within sequence. Zero based. If strand is - is relative to src end. */
    int size;	 /* Size in sequence (does not include dashes).  */
    char *text;  /* The sequence including dashes. */
    };

void mafCompFree(struct mafComp **pObj);
/* Free up a maf component. */

void mafCompFreeList(struct mafComp **pList);
/* Free up a list of maf components. */

int mafPlusStart(struct mafComp *comp);
/* Return start relative to plus strand of src. */

struct mafFile *mafOpen(char *fileName);
/* Open up a .maf file for reading.  Read header and
 * verify. Prepare for subsequent calls to mafNext().
 * Prints error message and aborts if there's a problem. */

struct mafFile *mafMayOpen(char *fileName);
/* Like mafOpen above, but returns NULL rather than aborting 
 * if file does not exist. */

struct mafAli *mafNext(struct mafFile *mafFile);
/* Return next alignment in file or NULL if at end. 
 * This will close the open file handle at end as well. */

struct mafFile *mafReadAll(char *fileName);
/* Read in full maf file */

void mafWriteStart(FILE *f, char *scoring);
/* Write maf header and scoring scheme name (may be null) */

void mafWrite(FILE *f, struct mafAli *maf);
/* Write next alignment to file. */

void mafWriteEnd(FILE *f);
/* Write end tag of maf file. */

void mafWriteAll(struct mafFile *mf, char *fileName);
/* Write out full mafFile. */

#endif /* MAF_H */

