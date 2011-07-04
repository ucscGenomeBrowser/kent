/* cDnaReader - read PSLs into cDnaQuery objects, also collects stats on
 * filtering */
#ifndef CDNAREADER_H
#define CDNAREADER_H
#include "cDnaStats.h"

struct cDnaReader
/* Object to read cDNA alignments.  To minimize memory requirements,
 * alignments are read for one cDNA at a time and processed.  Also collects
 * statistics on filtering. */
{
    struct lineFile *pslLf;       /* object for reading psl rows */
    struct cDnaQuery *cdna;       /* current cDNA */
    unsigned opts;                /* option bit set from cDnaOpts */
    struct psl *nextCDnaPsl;      /* if not null, psl for next cDNA */
    struct hash *polyASizes;      /* hash of polyASizes */
    struct hapRegions *hapRegions; /* optional table of haplotype regions */
    struct cDnaStats stats;       /* all statistics */
};

struct cDnaReader *cDnaReaderNew(char *pslFile, unsigned opts, char *polyASizeFile,
                                 struct hapRegions *hapRegions);
/* construct a new object, opening the psl file */

void cDnaReaderFree(struct cDnaReader **readerPtr);
/* free object */

boolean cDnaReaderNext(struct cDnaReader *reader);
/* load the next set of cDNA alignments, return FALSE if no more */


#endif
