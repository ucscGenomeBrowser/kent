/* cDnaAligns - Objects to read and score sets of cDNA alignments. Filtering decissions are
 * not made here*/
#ifndef CDNAALIGNS_H
#define CDNAALIGNS_H

#include "psl.h"
struct hash;
struct cDnaCnts;
struct polyASize;

/* global control id alignment ids be added to qNames of PSLs being
 * written. User for debugging */
extern boolean cDnaAlignsAlnIdQNameMode;

enum cDnaOpts
/* bit set of options to control scoring */
{
    cDnaUsePolyTHead    = 0x01,  /* use poly-T head if longer than poly-A tail (as in 3' ESTs),
                                  * otherwise just use poly-A tail */
    cDnaIgnoreNs        = 0x02,  /* don't include Ns while calculating the score and coverage.*/
    cDnaRepeatMisMatch  = 0x04,  /* count all repeats as mismatches */
    cDnaIgnoreIntrons   = 0x08   /* don't favor apparent introns in scoring */
};

struct cDnaAlign
/* information for a single cDNA alignment */
{
    struct cDnaAlign *next;
    struct cDnaQuery *cdna;  /* query object for alignment */
    struct psl *psl;         /* alignment */
    int alnId;               /* number id to identify this alignment */
    int adjAlnSize;          /* number of bases considered aligned after polyAT and ingore N
                              * adjustments */
    float ident;             /* fraction ident */
    float cover;             /* fraction of cDNA aligned, excluding polyA if
                              * it is available */
    int alnPolyAT;           /* bases of poly-A head or poly-T tail that are aligned */
    float score;             /* score weight by having introns and length */
    float repMatch;          /* fraction repeat match */
    boolean drop;            /* drop this psl if set */
    boolean weirdOverlap;    /* weird overlap was detected */
    struct cDnaAlignRef *hapAlns; /* best scoring haplotype alignments for each chrom */
    struct cDnaAlign *hapRefAln;  /* best score reference alignment for a haplotype chom alignment */
};

struct cDnaAlignRef
/* singly-linked list of references to cDnaAlign objects  */
{
    struct cDnaAlignRef *next;
    struct cDnaAlign  *ref;
};

struct cDnaQuery
/* information abourt the current cDNA query */
{
    unsigned opts;              /* cDnaOpts set */
    struct cDnaStats *stats;    /* stats object in reader */
    char *id;                   /* id for this cDNA  (memory not owned) */
    int adjQStart;              /* query range, possibly poly A/T adjusted */
    int adjQEnd;
    int numAln;                 /* number of alignments */
    int numDrop;                /* number of dropped alignments */
    struct cDnaAlign *alns;     /* all alignments for cDNAs */
    struct cDnaAlignRef *hapSets;  /* reference alignments linked to their
                                    * corresponding haplotype alignments on
                                    * pseudo-chroms, plus unlinked
                                    * haplotypes alignments. */
};

struct range
/* sequence range */
{
    int start;
    int end;
};

struct cDnaQuery *cDnaQueryNew(unsigned opts, struct cDnaStats *stats,
                               struct psl *psl, struct polyASize *polyASize);
/* construct a new cDnaQuery.  This does not add an initial alignment.
 * polyASize is null if not available.*/

void cDnaQueryFree(struct cDnaQuery **cdnaPtr);
/* free cDnaQuery and contained data  */

struct range cDnaQueryBlk(struct cDnaQuery *cdna, struct psl *psl,
                          int iBlk);
/* Get the query range for a block of a psl, adjust to exclude a polyA tail or
 * a polyT head */

void cDnaQueryRevScoreSort(struct cDnaQuery *cdna);
/* sort the alignments for this query by reverse cover+ident score */

void cDnaQueryWriteKept(struct cDnaQuery *cdna, FILE *outFh);
/* write the current set of psls that are flagged to keep */

void cDnaQueryWriteDrop(struct cDnaQuery *cdna, FILE *outFh);
/* write the current set of psls that are flagged to drop */

void cDnaQueryWriteWeird(struct cDnaQuery *cdna, FILE *outFh);
/* write the current set of psls that are flagged as weird overlap */

INLINE int cDnaQuerySize(struct cDnaQuery *cdna)
/* get the size of a cDNA sequence */
{
return cdna->alns->psl->qSize;
}

struct cDnaAlign *cDnaAlignNew(struct cDnaQuery *cdna, struct psl *psl);
/* construct a new object and add to the cdna list, updating the stats */

void cDnaAlignDrop(struct cDnaAlign *aln, boolean dropHapSetLinked, struct cDnaCnts *cnts, char *reasonFmt, ...);
/* flag an alignment as dropped, optionally dropping linked in hapSet */

void cDnaAlignPslOut(struct psl *psl, int alnId, FILE *fh);
/* output a PSL to a tab file.  If alnId is non-negative and
 * aldId out mode is set, append it to qName */

INLINE float cDnaAlignHapSetScore(struct cDnaAlign *aln)
/* get the score for all cDNAs forming a haplotype set */
{
assert(aln->hapRefAln == NULL);
float score = aln->score;
struct cDnaAlignRef *hapAln;
for (hapAln = aln->hapAlns; hapAln != NULL; hapAln = hapAln->next)
    score = max(score, hapAln->ref->score);
return score;
}

void cDnaAlignOut(struct cDnaAlign *aln, FILE *fh);
/* Output a PSL to a tab file, include alnId in qname based on
 * alnIdQNameMode */

void cDnaAlignVerbPsl(int level, struct psl *psl);
/* print psl location using verbose level */

void cDnaAlignVerbLoc(int level, struct cDnaAlign *aln);
/* print alignment location using verbose level */

void cDnaAlignVerb(int level, struct cDnaAlign *aln, char *msg, ...);
/* write verbose messager followed by location of a cDNA alignment */

INLINE struct cDnaAlignRef* cDnaAlignRefNew(struct cDnaAlign *cDnaAlign)
/* construct a reference to a cDnaAlign object */
{
struct cDnaAlignRef* ref;
AllocVar(ref);
ref->ref = cDnaAlign;
return ref;
}

INLINE struct cDnaAlignRef* cDnaAlignRefFromList(struct cDnaAlign *cDnaAligns)
/* construct a reference list from cDnaAlign list */
{
struct cDnaAlignRef* refs = NULL;
struct cDnaAlign *aln;
for (aln = cDnaAligns; aln != NULL; aln = aln->next)
    slAddHead(&refs, cDnaAlignRefNew(aln));
slReverse(&refs);
return refs;
}

#endif
