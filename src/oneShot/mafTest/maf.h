/* maf.h - Multiple alignment format.  */
#ifndef MAF_H
#define MAF_H

struct mafFile
/* A file full of multiple alignments. */
    {
    struct mafFile *next;
    int version;	/* Required */
    char *scoring;	/* Optional. Name of  scoring scheme. */
    struct mafAli *mafAli;	/** Possibly empty list of alignments. **/
    FILE *f;		/* Open file if any.  NULL except while parsing. */
    struct lineFile *lf; /* Open line file if any. NULL except while parsing. */
    };

struct mafAli
/* A multiple alignment. */
    {
    struct mafAli *next;
    double score;	/* Optional, depending on mafFile->scoring. */
    struct mafComp *components;	/* List of components of alignment */
    };

struct mafComp
/* A component of a multiple alignment. */
    {
    struct mafComp *next;
    char *src;	 /* Name of sequence source.  */
    int srcSize; /* Size of sequence source.  */
    char *strand; /* Strand of sequence.  Either + or -*/
    int start;	/* Start within sequence. Zero based. */
    int size;	/* Size in sequence (does not include dashes).  */
    char *text; /* The sequence including dashes. */
    };

/* Hand generated Routines. */

struct mafFile *mafOpen(char *fileName);
/* Open up a .maf file for reading.  Read header and
 * verify. Prepare for subsequent calls to mafNext().
 * Prints error message and aborts if there's a problem. */

struct mafFile *mafMayOpen(char *fileName);
/* Like above, but returns NULL rather than aborting on a
 * problem. */

struct mafAli *mafNext(struct mafFile *mafFile);
/* Return next alignment in FILE or NULL if at end. */

void mafWriteStart(FILE *f, char *scoring);
/* Write maf header and scoring scheme name (may be null) */

void mafWrite(FILE *f, struct mafAli *maf);
/* Write next alignment to file. */

void mafWriteEnd(FILE *f);
/* Write end tag of maf file. */

void mafWriteAll(struct mafFile *mf, char *fileName);
/* Write out full mafFile. */

struct mafFile *mafReadAll(char *fileName);
/* Read all elements in a maf file */

/* AutoXML Generated Routines - typically not called directly. */
void mafMafSave(struct mafFile *obj, int indent, FILE *f);
/* Save mafFile to file. */

struct mafFile *mafMafLoad(char *fileName);
/* Load mafFile from file. */

void mafAliSave(struct mafAli *obj, int indent, FILE *f);
/* Save mafAli to file. */

struct mafAli *mafAliLoad(char *fileName);
/* Load mafAli from file. */

void mafSSave(struct mafComp *obj, int indent, FILE *f);
/* Save mafComp to file. */

struct mafComp *mafSLoad(char *fileName);
/* Load mafComp from file. */

#endif /* MAF_H */

