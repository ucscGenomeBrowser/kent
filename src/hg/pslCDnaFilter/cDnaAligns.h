/* set of alignments, etc for a single cDNA sequence */
#ifndef CDNAALIGNS_H
#define CDNAALIGNS_H

struct psl;
struct hash;

struct cDnaAlign
/* information for a single cDNA alignment */
{
    struct cDnaAlign *next;
    struct psl *psl;     /* alignment */
    unsigned polyASize;  /* length of polyA tail, if known */
    float ident;         /* fraction ident */
    float cover;         /* fraction of cDNA aligned, excluding polyA if
                          * it is available */
    boolean drop;         /* drop this psl if set */
    boolean weirdOverlap; /* weird overlap was detected */
};

struct cDnaCnts
/* counts kept for alignments */
{
    unsigned aligns;     /* count of alignments */
    unsigned queries;    /* count of queries; */
    unsigned prevAligns; /* previous aligns count, used to update queires */
};

struct cDnaAligns
/* load and holds a set of alignments, etc for a single cDNA sequence */
{
    char *id;                     /* id for this cDNA  (memory not owned) */
    struct cDnaAlign *alns;       /* alignment list */
    struct lineFile *pslLf;       /* object for reading psl rows */
    struct psl *nextCDnaPsl;      /* if not null, psl for next cDNA */
    struct hash *polyASizes;      /* hash of polyASizes */
    struct cDnaCnts totalCnts;         /* number read */
    struct cDnaCnts keptCnts;          /* number of kept */
    struct cDnaCnts badCnts;           /* number bad that were dropped */ 
    struct cDnaCnts weirdOverCnts;     /* number of weird overlapping PSLs */
    struct cDnaCnts overlapCnts;       /* number dropped due to overlap */
    struct cDnaCnts minIdCnts;         /* number dropped less that min id */
    struct cDnaCnts idTopCnts;         /* number dropped less that top id */
    struct cDnaCnts minCoverCnts;      /* number dropped less that min cover */
    struct cDnaCnts coverTopCnts;      /* number dropped less that top cover */
};

struct cDnaAligns *cDnaAlignsNew(char *pslFile, char *polyASizeFile);
/* construct a new object, opening the psl file */

void cDnaAlignsFree(struct cDnaAligns **cdAlnsPtr);
/* free object */

boolean cDnaAlignsNext(struct cDnaAligns *cdAlns);
/* load the next set of cDNA alignments, return FALSE if no more */

void cDnaAlignsWriteKept(struct cDnaAligns *cdAlns,
                         FILE *outFh);
/* write the current set of psls that are flagged to keep */

void cDnaAlignsWriteDrop(struct cDnaAligns *cdAlns,
                         FILE *outFh);
/* write the current set of psls that are flagged to drop */

void cDnaAlignsWriteWeird(struct cDnaAligns *cdAlns,
                          FILE *outFh);
/* write the current set of psls that are flagged as weird overlap */

void cDnaAlignVerbPsl(int level, struct psl *psl);
/* write verbose info describing the location of a cDNA alignment */

void cDnaAlignVerb(int level, struct psl *psl, char *msg, ...);
/* write verbose messager followed by location of a cDNA alignment */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

