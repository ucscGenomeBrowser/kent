/* genePredToMafFrames - create mafFrames tables from genePreds  */
#include "common.h"
#include "options.h"
#include "genePred.h"
#include "mafFrames.h"
#include "maf.h"
#include "dnautil.h"
#include "geneBins.h"
#include "binRange.h"
#include "verbose.h"

static char const rcsid[] = "$Id: genePredToMafFrames.c,v 1.1 2005/10/07 05:11:59 markd Exp $";

/* Notes:
 * - functions are static to allow for implict inlining.
 * - both this code and the browser required no overlapping CDS exons
 * - strand in mafFrames matches the direction of transcript as projected
 *   onto the target species:
 *           maf   maf
 *      gene query target annotation
 *      +    +     +      +
 *      -    +     +      -
 *      +    -     +      -
 *      -    -     +      +
 */

/* assert for a frame being valid */
#define assertFrame(fr) assert(((fr) >= 0) && ((fr) <= 2))

/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"bed", OPTION_STRING},
    {NULL, 0}
};

/* data structure used to scan a mafComp object */
struct compCursor {
    struct mafComp *comp;   /* component object */
    int pos;                /* corresponding position in the sequence; it at a
                             * gap in the alignment, this is the next position
                             * in the sequence*/
    char *ptr;              /* pointer into text */
    boolean atBase;         /* is cursor at a base */
};

/* data structure used to scan a pair of mafComp objects */
struct scanCursor {
    struct compCursor query;   /* cursor for gene (query) */
    struct compCursor target;  /* cursor for target (genome) */
};

static struct compCursor compCursorNew(struct mafComp *comp)
/* construct a new compCursor.  Note: struct is copied, it is not dynamic */
{
struct compCursor cc;
cc.comp = comp;
cc.pos = comp->start;
cc.ptr = comp->text;
return cc;
}

static boolean compCursorAtBase(struct compCursor *cc)
/* Check if a position contains a base, this uses a quick check that really
 * doesn't test for a base, just >= A, so '.', '-', and '\0' fail */
{
return (*cc->ptr >= 'A');
}

static int revFrame(int fr)
/* reverse a frame, as if codon was reversed */
{
return (fr == 2) ? 0 : ((fr == 0) ? 2 : 1);
}


static int frameIncr(int frame, int amt)
/* increment the frame by the specified amount.  If amt is positive,
 * the frame is incremented in the direction of transcription, it 
 * it is negative it's incremented in the opposite direction. */
{
if (amt >= 0)
    return (frame + amt) % 3;
else
    return revFrame((revFrame(frame) - amt) % 3);
}

static struct scanCursor scanCursorNew(struct mafComp *geneComp, struct mafComp *targetComp)
/* construct a new scanCursor.  Note: struct is copied, it is not dynamic */
{
struct scanCursor sc;
sc.query = compCursorNew(geneComp);
sc.target = compCursorNew(targetComp);
return sc;
}

static boolean scanCursorIncr(struct scanCursor *sc)
/* increment scan cursor one position, return FALSE at end */
{
/* the < 'A' tests for '.', '-', '\0' so that we advance past the last position
 * in at the end of the string. */
if (compCursorAtBase(&sc->query))
    sc->query.pos++;
sc->query.ptr++;
if (compCursorAtBase(&sc->target))
    sc->target.pos++;
sc->target.ptr++;
return (*sc->query.ptr != '\0');
}

static boolean scanCursorAdvToQueryPos(struct scanCursor *sc, int qPos)
/* advance cursor to a position in the gene's component space */
{
while (sc->query.pos < qPos)
    {
    if (!scanCursorIncr(sc))
        return FALSE;
    }
return TRUE;
}

struct scanInfo
/* object that holds state while scanning the alignment for an exon */
{
    struct scanCursor *sc;  /* cursor into the alignment */
    struct cdsExon *exon;   /* exon being processed */
    int qStart;             /* exon coordinates mapped to query strand */
    int qEnd;
    int frame;              /* current frame */
    int scanDir;            /* scanning direction (+1|-1) relative to transcription */
    int subQStart;          /* query and target start of current subrange of exon,
                             * -1 for none */
    int subTStart;
    int subStartFrame;      /* frame frame subrange of exon, start scan start */
    int subEndFrame;
    FILE *fh;               /* output file */
    FILE *bedFh;            /* option bed file */
};

static void traceFrameDef(int level, struct scanInfo *si, struct mafFrames *mf)
/* verbose output to indicate why a mafFrames record was defined */
{
verbose(level, "exon %s[%d] %s:%d-%d %c fm: %d; ",
        si->exon->gene, si->exon->iExon,
        si->exon->chrom, si->exon->chromStart, si->exon->chromEnd,
        si->exon->strand, si->exon->frame);
verbose(level, "subQuery %s:%d-%d %c fm: %d-%d; ",
        si->sc->query.comp->src, si->subQStart,
        si->sc->query.pos, si->sc->query.comp->strand, si->subStartFrame, si->subEndFrame);
verbose(level, "subTarget %s:%d-%d %c fm: %d\n",
        si->sc->target.comp->src, si->subTStart,
        si->sc->target.pos, si->sc->target.comp->strand, mf->frame);
}

static void outputMafFrame(struct scanInfo *si)
/* output the current MAF frame record, assumes that the cursor
 * has gone past the last query column to include in record.
 * Reset the state.*/
{
struct mafFrames mf;
char *srcSeq = si->sc->query.comp->src;
char srcDb[32];
char *dot;
int len;
ZeroVar(&mf);
mf.chrom = strchr(si->sc->target.comp->src, '.')+1;
mf.chromStart = si->subTStart;
mf.chromEnd = si->sc->target.pos;

/* strip sequence name from src */
dot = strchr(srcSeq, '.');
len = (dot != NULL) ? (dot-srcSeq) : strlen(srcSeq);
safef(srcDb, sizeof(srcDb), "%.*s", len, srcSeq);
mf.src = srcDb;

/* frame is for first base in direction of transcription */
if (si->scanDir > 0)
    mf.frame = si->subStartFrame;
else
    mf.frame = si->subEndFrame;

/* project strand to target */
mf.strand[0] = (si->exon->strand == si->sc->query.comp->strand) ? '+' : '-';
if (si->sc->target.comp->strand == '-')
    mf.strand[0] = (mf.strand[0] == '+') ? '-' : '+';

mf.name = si->exon->gene;
mafFramesTabOut(&mf, si->fh);

/* optionally write BED file */
if (si->bedFh != NULL)
    fprintf(si->bedFh, "%s\t%d\t%d\t%s\t%d\t%s\n",
            mf.chrom, mf.chromStart, mf.chromEnd,
            mf.name, mf.frame, mf.strand);

if (verboseLevel() >= 3)
    traceFrameDef(3, si, &mf);

/* reset state */
si->subQStart = -1;
si->subTStart = -1;
si->subStartFrame = -1;
si->subEndFrame = -1;
}

static struct scanInfo scanInfoInit(struct scanCursor *sc, struct cdsExon *exon,
                                    FILE *frameFh, FILE *bedFh)
/* initialize scanInfo object for an exon.  This doesn't advance to the first
 * aligned position in the exon */
{
struct scanInfo si;
si.sc = sc;
si.exon = exon;
si.subTStart = -1;
si.subStartFrame = -1;
si.subEndFrame = -1;
si.fh = frameFh;
si.bedFh = bedFh;

/* genePred coordinates are always on the positive strand, we need to reverse
 * them to search the strand-specific MAF coords if the component is on the
 * negative strand */
si.qStart = exon->chromStart;
si.qEnd = exon->chromEnd;
if (sc->query.comp->strand == '-')
    reverseIntRange(&si.qStart, &si.qEnd, sc->query.comp->srcSize);

/* direction frame will increment */
si.scanDir = (exon->strand == sc->query.comp->strand) ? 1 : -1;

/* Get frame of the first base of the exon that will be scanned. */
si.frame =  (si.scanDir > 0) ? exon->frame
    : frameIncr(exon->frame, (exon->chromEnd-exon->chromStart)-1);
assertFrame(si.frame);
return si;
}

static void processColumn(struct scanInfo *si)
/* process one column of the alignment for an exon */
{
if (compCursorAtBase(&si->sc->query))
    {
    /* column contains query */
    if (compCursorAtBase(&si->sc->target))
        {
        /* target also aligns position */
        if (si->subTStart < 0)
            {
            /* remember start */
            si->subQStart = si->sc->query.pos;
            si->subTStart = si->sc->target.pos;
            si->subStartFrame = si->frame;
            }
        si->subEndFrame = si->frame;
        }
    else
        {
        /* target does not align position */
        if (si->subTStart >= 0)
            outputMafFrame(si);
        }
    /* advance frame */
    si->frame = frameIncr(si->frame, si->scanDir); 
    }
}

static void mkCompExonFrames(struct scanCursor *sc, struct cdsExon *exon,
                             FILE *frameFh, FILE *bedFh)
/* create mafFrames objects for a mafComp and an exon. */
{
struct scanInfo si = scanInfoInit(sc, exon, frameFh, bedFh);

/* advance to start of exon in component, which must be there due
 * to the bin search */
if (!scanCursorAdvToQueryPos(sc, si.qStart))
    errAbort("BUG: should have found exon in this component");
si.frame = frameIncr(si.frame, si.scanDir*(sc->query.pos - si.qStart));
assertFrame(si.frame);

/* scan columns of alignment overlapping exon */
while (sc->query.pos < si.qEnd)
    {
    processColumn(&si);
    if (!scanCursorIncr(si.sc))
        break;
    }
if (si.subTStart >= 0)
    outputMafFrame(&si);
}

static void mkCompFrames(struct geneBins *genes, struct mafComp *geneComp,
                         struct mafComp *targetComp, FILE *frameFh, FILE *bedFh)
/* create mafFrames objects for an mafComp */
{
struct scanCursor sc = scanCursorNew(geneComp, targetComp);
struct binElement *exons = geneBinsFind(genes, geneComp);
struct binElement *exonEl;

for (exonEl = exons; exonEl != NULL; exonEl = exonEl->next)
    mkCompExonFrames(&sc, (struct cdsExon*)exonEl->val, frameFh, bedFh);

slFreeList(&exons);
}

static void mkMafFrames(char *geneDb, char *targetDb, struct geneBins *genes,
                        char *mafFilePath, FILE *frameFh, FILE *bedFh)
/* create mafFrames objects for an MAF file */
{
struct mafFile *mafFile = mafOpen(mafFilePath);
struct mafAli *ali;

while ((ali = mafNext(mafFile)) != NULL)
    {
    struct mafComp *geneComp = mafMayFindComponentDb(ali, geneDb);
    struct mafComp *targetComp = mafMayFindComponentDb(ali, targetDb);
    if ((geneComp != NULL) && (targetComp != NULL))
        mkCompFrames(genes, geneComp, targetComp, frameFh, bedFh);
    mafAliFree(&ali);
    }
mafFileFree(&mafFile);
}

static void genePredToMafFrames(char *geneDb, char *targetDb, char *genePredFile,
                                char *mafFramesFile, char *bedFile,
                                int numMafFiles, char **mafFiles)
/* create mafFrames tables from genePreds  */
{
struct geneBins *genes = geneBinsNew(genePredFile);
FILE *frameFh = mustOpen(mafFramesFile, "w");
FILE *bedFh = (bedFile != NULL) ? mustOpen(bedFile, "w") : NULL;
int i;

for (i = 0; i < numMafFiles; i++)
    mkMafFrames(geneDb, targetDb, genes, mafFiles[i], frameFh, bedFh);

geneBinsFree(&genes);
carefulClose(&frameFh);
carefulClose(&bedFh);
}

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s\n"
    "\n"
    "genePredToMafFrames - create mafFrames tables from a genePreds\n"
    "\n"
    "genePredToMafFrames [options] geneDb targetDb genePred mafFrames maf1 [maf2..]\n"
    "\n"
    "Create frame annotations for a component of aa set of MAFs.  The\n"
    "resulting file maybe combined with mafFrames for other components.\n"
    "\n"
    "Arguments:\n"
    "  o geneDb - db in MAF that corresponds to genePred's organism.\n"
    "  o targetDb - db of target track in MAF\n"
    "  o genePred - genePred file.  Overlapping annotations ahould have\n"
    "    be removed.  This file may optionally include frame annotations\n"
    "  o mafFrames - output file\n"
    "  o maf1,... - MAF files\n"
    "Options:\n"
    "  -bed=file - output a bed of for each mafFrame region, useful for debugging.\n"
    "  -verbose=level - enable verbose tracing, the following levels are implemented:\n"
    "     3 - print information about data used to compute each record.\n"
    "\n", msg);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 6)
    usage("wrong # args");
genePredToMafFrames(argv[1], argv[2], argv[3], argv[4], optionVal("bed", NULL),
                    argc-5, argv+5);

return 0;
}
