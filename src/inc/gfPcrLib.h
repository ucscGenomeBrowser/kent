/* gfPcrLib - Routines to help do in silico PCR.
 * Copyright 2004 Jim Kent.  All rights reserved. */

#ifndef GFPCRLIB_H
#define GFPCRLIB_H

struct gfPcrInput
/* Info for input to one PCR experiment. */
    {
    struct gfPcrInput *next;  /* Next in singly linked list. */
    char *name;	/* Name of experiment */
    char *fPrimer;	/* Forward primer - 15-30 bases */
    char *rPrimer;	/* Reverse primer - after fPrimer and on opposite strand */
    };

void gfPcrInputStaticLoad(char **row, struct gfPcrInput *ret);
/* Load a row from gfPcrInput table into ret.  The contents of ret will
 * be replaced at the next call to this function. */

struct gfPcrInput *gfPcrInputLoad(char **row);
/* Load a gfPcrInput from row fetched with select * from gfPcrInput
 * from database.  Dispose of this with gfPcrInputFree(). */

struct gfPcrInput *gfPcrInputLoadAll(char *fileName);
/* Load all gfPcrInput from a whitespace-separated file.
 * Dispose of this with gfPcrInputFreeList(). */

void gfPcrInputFree(struct gfPcrInput **pEl);
/* Free a single dynamically allocated gfPcrInput such as created
 * with gfPcrInputLoad(). */

void gfPcrInputFreeList(struct gfPcrInput **pList);
/* Free a list of dynamically allocated gfPcrInput's */

struct gfPcrOutput
/* Output of PCR experiment. */
    {
    struct gfPcrOutput *next;	/* Next in list */
    char *name;			/* Name of experiment. */
    char *fPrimer;	/* Forward primer - 15-30 bases */
    char *rPrimer;	/* Reverse primer - after fPrimer and on opposite strand */
    char *seqName;	/* Name of sequence (chromosome maybe) that gets amplified. */
    int seqSize;	/* Size of sequence (chromosome maybe) */
    int fPos;		/* Position of forward primer in seq. */
    int rPos;		/* Position of reverse primer in seq. */
    char strand;	/* Strand of amplified sequence. */
    char *dna;		/* Fragment of sequence that gets amplified.
                         * Note in some senses you'll want to replace
			 * start and ends of this with fPrimer/rPrimer to
			 * be strictly accurate.  */
    };

void gfPcrOutputFree(struct gfPcrOutput **pOut);
/* Free up a gfPcrOutput structure. */

void gfPcrOutputFreeList(struct gfPcrOutput **pList);
/* Free up a list of gfPcrOutputs. */




void gfPcrLocal(char *pcrName, 
	struct dnaSeq *seq, int seqOffset, char *seqName, int seqSize,
	int maxSize, char *fPrimer, int fPrimerSize, char *rPrimer, int rPrimerSize,
	int minPerfect, int minGood, char strand, struct gfPcrOutput **pOutList);
/* Do detailed PCR scan on DNA already loaded into memory and put results
 * (in reverse order) on *pOutList. */

struct gfRange *gfPcrGetRanges(char *host, char *port, char *fPrimer, char *rPrimer,
	int maxSize);
/* Query gfServer with primers and convert response to a list of gfRanges. */

struct gfPcrOutput *gfPcrViaNet(char *host, char *port, char *seqDir, 
	struct gfPcrInput *inList,
	int maxSize, int minPerfect, int minGood);
/* Do PCRs using gfServer index, returning list of results. */

void gfPcrOutputWriteOne(struct gfPcrOutput *out, char *outType, 
	char *url, FILE *f);
/* Write a single output in specified format (either "fa" or "bed") 
 * to file.  If url is non-null it should be a printf formatted
 * string that takes %s, %d, %d for chromosome, start, end. */

void gfPcrOutputWriteList(struct gfPcrOutput *outList, char *outType, 
	char *url, FILE *f);
/* Write list of outputs in specified format (either "fa" or "bed") 
 * to file.  If url is non-null it should be a printf formatted
 * string that takes %s, %d, %d for chromosome, start, end. */

void gfPcrOutputWriteAll(struct gfPcrOutput *outList, 
	char *outType, char *url, char *fileName);
/* Create file of outputs in specified format (either "fa" or "bed") 
 * to file.  If url is non-null it should be a printf formatted
 * string that takes %s, %d, %d for chromosome, start, end. */

char *gfPcrMakePrimer(char *s);
/* Make primer (lowercased DNA) out of text.  Complain if
 * it is too short or too long. */

#endif /* GFPCRLIB_H */
