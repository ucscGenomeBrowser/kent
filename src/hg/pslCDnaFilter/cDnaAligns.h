/* cDnaAligns - Objects to read and score sets of cDNA alignments. Filtering decissions are
 * not made here*/
#ifndef CDNAALIGNS_H
#define CDNAALIGNS_H

struct psl;
struct hash;
struct cDnaCnts;
struct polyASize;

enum cDnaOpts
/* bit set of options to control scoring */
{
    cDnaUsePolyTHead = 0x01,  /* use poly-T head if longer than poly-A tail (as in 3' ESTs),
                               * otherwise just use poly-A tail */
    cDnaIgnoreNs     = 0x02   /* don't include Ns while calculating the score and coverage.*/
};

struct cDnaAlign
/* information for a single cDNA alignment */
{
    struct cDnaAlign *next;
    struct cDnaQuery *cdna;  /* query object for alignment */
    struct psl *psl;         /* alignment */
    int alnId;               /* number id to identify this alignment */
    int adjMisMatch;         /* number of misMatches; includes Ns unless the
                              * are ignored */
    float ident;             /* fraction ident */
    float cover;             /* fraction of cDNA aligned, excluding polyA if
                              * it is available */
    int alnPolyAT;           /* bases of poly-A head or poly-T tail that are aligned */
    float score;             /* score weight by having introns and length */
    float repMatch;          /* fraction repeat match */
    boolean isHapRegion;     /* in a haplotype region of a reference chromosome */
    boolean isHapChrom;      /* is in a haplotype pseudochromosome */
    boolean drop;            /* drop this psl if set */
    boolean weirdOverlap;    /* weird overlap was detected */
    unsigned refLinkCount;   /* number of ref sequences linked to this if hapAln  */
    struct cDnaHapAln *hapAlns;  /* links to objects linking corresponding alignments in
                                  * haplotype regions */
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
    boolean haveHaps;           /* have any alignments in haplotype chromosomes
                                 * or regions. */
    struct cDnaAlign *alns;     /* alignment list */
};


struct cDnaHapAln
/* structure used to link reference alignments in haplotype regions to
 * alignments on haplotype chromosomes. */
{
    struct cDnaHapAln *next;
    struct cDnaAlign *hapAln;  /* similar cDNA haplotype alignment */
    float score;               /* similarity score */
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

void cDnaQueryWriteKept(struct cDnaQuery *cdna,
                         FILE *outFh);
/* write the current set of psls that are flagged to keep */

void cDnaQueryWriteDrop(struct cDnaQuery *cdna,
                        FILE *outFh);
/* write the current set of psls that are flagged to drop */

void cDnaQueryWriteWeird(struct cDnaQuery *cdna,
                         FILE *outFh);
/* write the current set of psls that are flagged as weird overlap */

struct cDnaAlign *cDnaAlignNew(struct cDnaQuery *cdna,
                               struct psl *psl);
/* construct a new object and add to the cdna list, updating the stats */

void cDnaAlnLinkHapAln(struct cDnaAlign *aln, struct cDnaAlign *hapAln, float score);
/* link a reference alignment to a happlotype alignment */

void cDnaAlignDrop(struct cDnaAlign *aln, struct cDnaCnts *cnts);
/* flag an alignment as dropped */

/* should alignment ids be added to qNames? */
extern boolean alnIdQNameMode;

void cDnaAlignPslOut(struct psl *psl, int alnId, FILE *fh);
/* output a PSL to a tab file.  If alnId is non-negative and
 * aldId out mode is set, append it to qName */

void cDnaAlignOut(struct cDnaAlign *aln, FILE *fh);
/* Output a PSL to a tab file, include alnId in qname based on
 * alnIdQNameMode */

void cDnaAlignVerbPsl(int level, struct psl *psl);
/* print psl location using verbose level */

void cDnaAlignVerbLoc(int level, struct cDnaAlign *aln);
/* print alignment location using verbose level */

void cDnaAlignVerb(int level, struct cDnaAlign *aln, char *msg, ...);
/* write verbose messager followed by location of a cDNA alignment */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

