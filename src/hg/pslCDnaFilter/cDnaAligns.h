/* Objects to read and score sets of cDNA alignments. Filtering decissions are
* not made here*/
#ifndef CDNAALIGNS_H
#define CDNAALIGNS_H

struct psl;
struct hash;

struct cDnaAlign
/* information for a single cDNA alignment */
{
    struct cDnaAlign *next;
    struct psl *psl;     /* alignment */
    float ident;         /* fraction ident */
    float cover;         /* fraction of cDNA aligned, excluding polyA if
                          * it is available */
    int alnPolyAT;       /* bases of poly-A head or poly-T tail that are aligned */      
    float score;         /* score weight by having introns and length */
    float repMatch;      /* fraction repeat match */
    boolean drop;         /* drop this psl if set */
    boolean weirdOverlap; /* weird overlap was detected */
};

struct cDnaQuery
/* information abourt the current cDNA query */
{
    struct cDnaReader *reader;  /* link back to reader */
    struct cDnaStats *stats;    /* stats object in reader */
    char *id;                   /* id for this cDNA  (memory not owned) */
    int adjQStart;              /* query range, possibly poly A/T adjusted */
    int adjQEnd;    
    struct cDnaAlign *alns;     /* alignment list */
};

struct cDnaCnts
/* counts kept for alignments */
{
    unsigned queries;        /* count of queries; */
    unsigned aligns;         /* count of alignments */
    unsigned multAlnQueries; /* count of queries with multiple alignments */
    unsigned prevAligns;     /* previous aligns count, used to update queires */
};

struct cDnaStats
/* all statistics collected on cDNA filtering */
{
    struct cDnaCnts totalCnts;         /* number read */
    struct cDnaCnts keptCnts;          /* number of kept */
    struct cDnaCnts badCnts;           /* number bad that were dropped */ 
    struct cDnaCnts minQSizeCnts;      /* number below min size dropped */ 
    struct cDnaCnts weirdOverCnts;     /* number of weird overlapping PSLs */
    struct cDnaCnts weirdKeptCnts;     /* weird overlapping PSLs not dropped  */
    struct cDnaCnts overlapCnts;       /* number dropped due to overlap */
    struct cDnaCnts minIdCnts;         /* number dropped less that min id */
    struct cDnaCnts minCoverCnts;      /* number dropped less that min cover */
    struct cDnaCnts minNonRepLenCnts;  /* number dropped due minNonRepLen */
    struct cDnaCnts maxRepMatchCnts;   /* number dropped due maxRepMatch */
    struct cDnaCnts maxAlignsCnts;     /* number dropped due to over max */
    struct cDnaCnts localBestCnts;     /* number dropped due to local near best */
    struct cDnaCnts globalBestCnts;    /* number dropped due to global near best */
    struct cDnaCnts minSpanCnts;       /* number dropped due to under minSpan */
};

struct cDnaReader
/* Object to read cDNA alignments.  To minimize memory requirements,
 * alignments are read for one cDNA at a time and processed.  Also collects
 * statistics on filtering. */
{
    struct lineFile *pslLf;       /* object for reading psl rows */
    struct cDnaQuery *cdna;       /* current cDNA */
    boolean usePolyTHead;         /* use poly-T head if longer than poly-A tail (as in 3' ESTs),
                                   * otherwise just use poly-A tail */
    struct psl *nextCDnaPsl;      /* if not null, psl for next cDNA */
    struct hash *polyASizes;      /* hash of polyASizes */
    struct cDnaStats stats;       /* all statistics */
};


struct cDnaRange
/* range within a cDNA */
{
    int start;
    int end;
};

struct cDnaReader *cDnaReaderNew(char *pslFile, boolean usePolyTHead, char *polyASizeFile);
/* construct a new object, opening the psl file */

void cDnaReaderFree(struct cDnaReader **readerPtr);
/* free object */

boolean cDnaReaderNext(struct cDnaReader *reader);
/* load the next set of cDNA alignments, return FALSE if no more */

struct cDnaRange cDnaQueryBlk(struct cDnaQuery *cdna, struct psl *psl,
                              int iBlk);
/* Get the query range for a block of a psl, adjust to exclude a polyA tail or
 * a polyT head */

void cDnaQueryRevScoreSort(struct cDnaQuery *cdna);
/* sort the alignments for this query by reverse cover+ident score */

void cDnaQueryWriteKept(struct cDnaQuery *cdna,
                         FILE *outFh);
/* write the current set of psls that are flagged to keep */

void cDnaQueryWriteDrop(struct cDnaQuery *cdna,
                        FILE *outFh);
/* write the current set of psls that are flagged to drop */

void cDnaQueryWriteWeird(struct cDnaQuery *cdna,
                         FILE *outFh);
/* write the current set of psls that are flagged as weird overlap */

void cDnaAlignVerb(int level, struct psl *psl, char *msg, ...);
/* write verbose messager followed by location of a cDNA alignment */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

