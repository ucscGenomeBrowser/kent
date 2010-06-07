/* ace - a format written by phrap, read by consed.
 *
 * This file contains routines to read such files.
 * Note that though the coordinates are one based and
 * closed on disk, they get converted to our usual half
 * open zero based in memory.  */

#ifndef ACE_H
#define ACE_H

#ifndef LINEFILE_H
#include "linefile.h"
#endif 

#ifndef DNASEQ_H
#include "dnaseq.h"
#endif

struct aceAS
/* This contains an AS entry. */
    {
    int contigs;
    int reads;
    };

struct aceCO
/* This contains a CO entry. */
    {
    char *contigName;
    int bases;
    int reads;
    struct dnaSeq *seq;
    };

struct aceAF
/* This contains an AF entry. */
    {
    struct aceAF *next;
    char *readName;
    int startPos;
    };

struct aceRD
/* This contains an RD entry. */
    {
    struct aceRD *next;
    char *readName;
    int bases;
    struct dnaSeq *seq;
    };

struct ace
/* This contains information about one ace element. */
    {
    struct ace *next;
    struct aceAS aceAS;
    struct aceCO aceCO;
    struct aceAF *afList;
    struct aceRD *rdList;
    };

void aceFree(struct ace **pEl);
/* Free an ace. */

void aceFreeList(struct ace **pList);
/* Free a list of dynamically allocated ace's */

struct ace *aceRead(struct lineFile *lf);
/* Read in from .ace file and return it.
 * Returns NULL at EOF. */

boolean aceCheck(struct ace *ace, struct lineFile *lf);
/* Return FALSE if there's a problem with ace. */

void aceWrite(struct ace *ace, FILE *f);
/* Output ace to ace file. */

#endif /* ACE_H */

