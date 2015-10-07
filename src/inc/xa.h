/* xa.h - manage cross species alignments stored in database.
 *
 * This file is copyright 2000 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef XA_H
#define XA_H

struct xaAli
/* This contains information about one xeno alignment. */
    {
    struct xaAli *next;
    char *name;
    char *query;
    int qStart, qEnd;
    char qStrand;
    char *target;
    int tStart, tEnd;
    char tStrand;
    int milliScore;
    int symCount;
    char *qSym, *tSym, *hSym;
    };

void xaAliFree(struct xaAli *xa);
/* Free up a single xaAli. */

void xaAliFreeList(struct xaAli **pXa);
/* Free up a list of xaAlis. */

int xaAliCmpTarget(const void *va, const void *vb);
/* Compare two xaAli's to sort by ascending target positions. */

FILE *xaOpenVerify(char *fileName);
/* Open file, verify it's the right type, and
 * position file pointer for first xaReadNext(). */

FILE *xaIxOpenVerify(char *fileName);
/* Open file, verify that it's a good xa index. */

struct xaAli *xaReadNext(FILE *f, boolean condensed);
/* Read next xaAli from file. If condensed
 * don't fill int query, target, qSym, tSym, or hSym. */

struct xaAli *xaReadRange(char *rangeIndexFileName, char *dataFileName, 
    int start, int end, boolean condensed);
/* Return list of all xaAlis that range from start to end.  If condensed
 * don't fill int query, qSym, tSym, or hSym. */

struct xaAli *xaRdRange(FILE *ix, FILE *data, 
    int start, int end, boolean condensed);
/* Like xaReadRange but pass in open files rather than file names. */

char *xaAlignSuffix();
/* Return suffix of file with actual alignments. */

char *xaChromIxSuffix();
/* Return suffix of files that index xa's by chromosome position. */


#endif /* XA_H */

