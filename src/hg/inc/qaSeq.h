/* qaSeq.h - read and write quality scores. */

#ifndef QASEQ_H
#define QASEQ_H

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif

#ifndef LINEFILE_H
#include "linefile.h"
#endif

struct qaSeq
/* Structure containing quality info and optionally
 * base by base sequence for a piece of DNA. */
    {
    struct qaSeq *next;  /* Next in list. */
    char *name;           /* Name of sequence. */
    DNA *dna;             /* Sequence base by base. */
    int size;             /* Size of sequence. */
    UBYTE *qa;            /* Quality scores in -log10 error rate */
    };

void qaSeqFree(struct qaSeq **pQa);
/* Free up qaSeq. */

void qaSeqFreeList(struct qaSeq **pList);
/* Free a list of QA seq's. */

struct qaSeq *qaMustReadBoth(char *qaName, char *faName);
/* Read both QA(C) and FA files into qaSeq structure or die trying. */

struct qaSeq *qaReadBoth(char *qaName, char *faName);
/* Attempt to read both QA(C) and FA files into qaSeq structure.
 * FA file must exist, but if qaFile doesn't exist it will just
 * make it up. */

struct qaSeq *qaRead(char *qaName);
/* Read in a .qa(c) file (all records) or die trying. */

struct qaSeq *qaReadNext(struct lineFile *lf);
/* Read in next record in .qa file. */

boolean qaFastReadNext(struct lineFile *lf, UBYTE **retQa, int *retSize, 
    char **retName);
/* Read in next QA entry as fast as we can. Return FALSE at EOF. 
 * The returned QA info and name will be overwritten by the next call
 * to this function. */

void qaWriteNext(FILE *f, struct qaSeq *qa);
/* Write next record to a .qa file. */

struct qaSeq *qacReadNext(FILE *f, boolean isSwapped);
/* Read in next record in .qa file. */

void qacWriteHead(FILE *f);
/* Write qac header. */

void qacWriteNext(FILE *f, struct qaSeq *qa);
/* Write next record to qac file. */

boolean isQacFile(char *fileName);
/* Return TRUE if fileName is a qac file. */

FILE *qacOpenVerify(char *fileName, boolean *retIsSwapped);
/* Open qac file, and verify that it is indeed a qac file. */

char *qacPathFromFaPath(char *faName);
/* Given an fa path name return path name of corresponding qac
 * file.  Copy this result somewhere if you wish to keep it 
 * longer than the next call to qacPathFromFaPath. */

void qaMakeFake(struct qaSeq *qa);
/* Allocate and fill in fake quality info. */

void qaFake(struct qaSeq *qa, int badTailSize, int poorTailSize);
/* Fill in fake quality info. */

struct qaSeq *qaMakeConstant(char *name, int val, int size);
/* Allocate and fill in constant quality info. */

boolean qaIsGsFake(struct qaSeq *qa);
/* Return TRUE if quality info appears to be faked by 
 * Greg Schuler. (So we can refake it our way...) */
#endif /* QASEQ_H */

