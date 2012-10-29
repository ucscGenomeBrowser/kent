/* gff3ToGenePred - convert a GFF3 file to a genePred file. */
#include "common.h"
#include <limits.h>
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "gff3.h"
#include "genePred.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "gff3ToGenePred - convert a GFF3 file to a genePred file\n"
  "usage:\n"
  "   gff3ToGenePred inGff3 outGp\n"
  "options:\n"
  "  -maxParseErrors=50 - Maximum number of parsing errors before aborting. A negative\n"
  "   value will allow an unlimited number of errors.  Default is 50.\n"
  "  -maxConvertErrors=50 - Maximum number of conversion errors before aborting. A negative\n"
  "   value will allow an unlimited number of errors.  Default is 50.\n"
  "  -honorStartStopCodons - only set CDS start/stop status to complete if there are\n"
  "   corresponding start_stop codon records\n"
  "This converts:\n"
  "   - top-level gene records with mRNA records\n"
  "   - top-level mRNA records\n"
  "   - mRNA records that contain:\n"
  "       - exon and CDS\n"
  "       - CDS, five_prime_UTR, three_prime_UTR\n"
  "       - only exon for non-coding\n"
  "   - top-level gene records with transcript records\n"
  "   - top-level transcript records\n"
  "   - transcript records that contain:\n"
  "       - exon\n"
  "The first step is to parse GFF3 file, up to 50 errors are reported before\n"
  "aborting.  If the GFF3 files is successfully parse, it is converted to gene,\n"
  "annotation.  Up to 50 conversion errors are reported before aborting.\n"
  "\n"
  "Input file must conform to the GFF3 specification:\n"
  "   http://www.sequenceontology.org/gff3.shtml\n"
  );
}

static struct optionSpec options[] = {
    {"maxParseErrors", OPTION_INT},
    {"maxConvertErrors", OPTION_INT},
    {"honorStartStopCodons", OPTION_BOOLEAN},
    {NULL, 0},
};
static boolean honorStartStopCodons = FALSE;
static int maxParseErrors = 50;  // maximum number of errors during parse
static int maxConvertErrors = 50;  // maximum number of errors during conversion
static int convertErrCnt = 0;  // number of convert errors


static void cnvError(char *format, ...)
/* print a GFF3 to gene conversion error.  This will return.  Code must check
 * for error count to be exceeded and unwind to the top level to print a usefull
 * error message and abort. */
{
fputs("Error: ", stderr);
va_list args;
va_start(args, format);
vfprintf(stderr, format, args);
va_end(args);
fputc('\n', stderr);
convertErrCnt++;
}

static char *mkAnnAddrKey(struct gff3Ann *ann)
/* create a key for a gff3Ann from its address.  WARNING: static return */
{
static char buf[64];
safef(buf, sizeof(buf), "%lu", (unsigned long)ann);
return buf;
}

static boolean isProcessed(struct hash *processed, struct gff3Ann *ann)
/* has an ann record be processed? */
{
return hashLookup(processed, mkAnnAddrKey(ann)) != NULL;
}

static void recProcessed(struct hash *processed, struct gff3Ann *ann)
/* add an ann record to processed hash */
{
hashAdd(processed, mkAnnAddrKey(ann), ann);
}

static struct gff3File *loadGff3(char *inGff3File)
/* load GFF3 into memory */
{
struct gff3File *gff3File = gff3FileOpen(inGff3File, maxParseErrors, NULL);
if (gff3File->errCnt > 0)
    errAbort("%d errors parsing GFF3 file: %s", gff3File->errCnt, inGff3File); 
return gff3File;
}

static boolean haveChildFeature(struct gff3Ann *parent, char *featName)
/* does a child feature of the specified they exist as a child */
{
struct gff3AnnRef *child;
for (child = parent->children; child != NULL; child = child->next)
    {
    if (sameString(child->ann->type, featName))
        return TRUE;
    }
return FALSE;
}

static struct gff3AnnRef *getChildFeatures(struct gff3Ann *parent, char *featName)
/* build sorted list of the specified children */
{
struct gff3AnnRef *child, *feats = NULL;
for (child = parent->children; child != NULL; child = child->next)
    {
    if (sameString(child->ann->type, featName))
        slAddHead(&feats, gff3AnnRefNew(child->ann));
    }
slSort(&feats, gff3AnnRefLocCmp);
return feats;
}

static void setCdsStatFromCodons(struct genePred *gp, struct gff3Ann *mrna)
/* set cds start/stop status based on start/stop codon annotation */
{
if (gp->strand[0] == '+')
    {
    gp->cdsStartStat = haveChildFeature(mrna, gff3FeatStartCodon) ? cdsComplete : cdsIncomplete;
    gp->cdsEndStat =  haveChildFeature(mrna, gff3FeatStopCodon) ? cdsComplete : cdsIncomplete;
    }
else
    {
    gp->cdsStartStat = haveChildFeature(mrna, gff3FeatStopCodon) ? cdsComplete : cdsIncomplete;
    gp->cdsEndStat =  haveChildFeature(mrna, gff3FeatStartCodon) ? cdsComplete : cdsIncomplete;
    }
}

static struct genePred *makeGenePred(struct gff3Ann *gene, struct gff3Ann *mrna, struct gff3AnnRef *exons, struct gff3AnnRef *cdsBlks)
/* construct the empty genePred, return NULL on a failure. */
{
if (exons == NULL)
    {
    cnvError("no exons defined for mRNA %s", mrna->id);
    return NULL;
    }

int txStart = exons->ann->start;
int txEnd = ((struct gff3AnnRef*)slLastEl(exons))->ann->end;
int cdsStart = (cdsBlks == NULL) ? txEnd : cdsBlks->ann->start;
int cdsEnd = (cdsBlks == NULL) ? txEnd : ((struct gff3AnnRef*)slLastEl(cdsBlks))->ann->end;

if ((mrna->strand == NULL) || (mrna->strand[0] == '?'))
    {
    cnvError("invalid strand for mRNA %s", mrna->id);
    return NULL;
    }

struct genePred *gp = genePredNew(mrna->id, mrna->seqid, mrna->strand[0],
                                  txStart, txEnd, cdsStart, cdsEnd,
                                  genePredAllFlds, slCount(exons));
gp->name2 = cloneString(gene->id);

// set start/end status based on codon features if requested
if (honorStartStopCodons)
    {
    setCdsStatFromCodons(gp, mrna);
    }
else
    {
    gp->cdsStartStat = cdsComplete;
    gp->cdsEndStat = cdsComplete;
    }
return gp;
}

static void outputGenePredAndFree(FILE *gpFh, struct genePred *gp)
/* validate and output a genePred and free it */
{
// output before checking so it can be examined
genePredTabOut(gp, gpFh);
if (genePredCheck("GFF3 convert to genePred", stderr, -1, gp) != 0)
    cnvError("invalid genePred created for: %s", gp->name);
genePredFree(&gp);
}

static bool adjacentBlockDiffTypes(struct gff3AnnRef *prevBlk, struct gff3AnnRef *blk)
/* are two block adjacent and of a different type? prevBlk can be NULL */
{
return (prevBlk != NULL) && (prevBlk->ann->end == blk->ann->start)
    && !sameString(prevBlk->ann->type, blk->ann->type);
}

static void addExons(struct genePred *gp, struct gff3AnnRef *blks)
/* Add exons.  blks can either be exon records or utr and cds records.
 * If blks are adjacent and are of different types, they will be joined.
 * If blks are adjacent and the same type, they are not joined, which might
 * indicate a CDS frameshift.
*/
{
struct gff3AnnRef *blk, *prevBlk = NULL;
for (blk = blks; blk != NULL; prevBlk = blk, blk = blk->next)
    {
    int i = gp->exonCount;
    if (adjacentBlockDiffTypes(prevBlk, blk))
        {
        gp->exonEnds[i-1] = blk->ann->end; // extend
        }
    else
        {
        gp->exonStarts[i] = blk->ann->start;
        gp->exonEnds[i] = blk->ann->end;
        gp->exonFrames[i] = -1;
        gp->exonCount++;
        }
    }
}

static int findCdsExon(struct genePred *gp, struct gff3Ann *cds, int iExon)
/* search for the exon containing the CDS, starting with iExon+1, return -1 on error */
{
for (iExon++; iExon < gp->exonCount; iExon++)
    {
    if ((gp->exonStarts[iExon] <= cds->start) && (cds->end <= gp->exonEnds[iExon]))
        return iExon;
    }
cnvError("no exon in %s contains CDS %d-%d", gp->name, cds->start, cds->end);
return -1;
}

static boolean addCdsFrame(struct genePred *gp, struct gff3AnnRef *cdsBlks)
/* assign frame based on CDS regions.  Return FALSE error */
{
struct gff3AnnRef *cds;
int iExon = -1; // caches current position
for (cds = cdsBlks; cds != NULL; cds = cds->next)
    {
    iExon = findCdsExon(gp, cds->ann, iExon);
    if (iExon < 0)
        return FALSE; // error
    gp->exonFrames[iExon] = gff3PhaseToFrame(cds->ann->phase);
    }
return TRUE;
}

static struct gff3AnnRef *getCdsUtrBlks(struct gff3Ann *mrna)
/* build sorted list of UTR + CDS blocks */
{
struct gff3AnnRef *cdsUtrBlks = slCat(getChildFeatures(mrna, gff3FeatFivePrimeUTR),
                                      slCat(getChildFeatures(mrna, gff3FeatCDS),
                                            getChildFeatures(mrna, gff3FeatThreePrimeUTR)));
slSort(&cdsUtrBlks, gff3AnnRefLocCmp);
return cdsUtrBlks;
}

static struct genePred *mrnaToGenePred(struct gff3Ann *gene, struct gff3Ann *mrna)
/* construct a genePred from an mRNA, return NULL if there is an error */
{
// allow for only having UTR/CDS children
struct gff3AnnRef *exons = getChildFeatures(mrna, gff3FeatExon);
struct gff3AnnRef *cdsBlks = getChildFeatures(mrna, gff3FeatCDS);
struct gff3AnnRef *cdsUtrBlks = NULL;  // used if no exons
if (exons == NULL)
    cdsUtrBlks = getCdsUtrBlks(mrna);
struct gff3AnnRef *useExons = (exons != NULL) ? exons : cdsUtrBlks;

struct genePred *gp = makeGenePred((gene != NULL) ? gene : mrna, mrna, useExons, cdsBlks);
if (gp != NULL)
    {
    addExons(gp, useExons);
    if (!addCdsFrame(gp, cdsBlks))
        genePredFree(&gp);  // got error, free it and NULL.
    }
slFreeList(&exons);
slFreeList(&cdsBlks);
slFreeList(&cdsUtrBlks);
return gp;  // NULL if error above
}

static struct genePred *transcriptToGenePred(struct gff3Ann *gene, struct gff3Ann *transcript)
/* construct a genePred from an transcript record, return NULL if there is an error */
{
// allow for only having UTR/CDS children
struct gff3AnnRef *exons = getChildFeatures(transcript, gff3FeatExon);
struct genePred *gp = makeGenePred((gene != NULL) ? gene : transcript, transcript, exons, NULL);
if (gp != NULL)
    addExons(gp, exons);
slFreeList(&exons);
return gp;  // NULL if error above
}

static void processTranscript(FILE *gpFh, struct gff3Ann *gene, struct gff3Ann *transcript, struct hash *processed)
/* process a transcript node in the tree; gene can be NULL. Error count increment on error and genePred discarded */
{
recProcessed(processed, transcript);

struct genePred *gp = transcriptToGenePred(gene, transcript);
if (gp != NULL)
    outputGenePredAndFree(gpFh, gp);
}

static void processMRna(FILE *gpFh, struct gff3Ann *gene, struct gff3Ann *mrna, struct hash *processed)
/* process a mRNA node in the tree; gene can be NULL. Error count increment on error and genePred discarded */
{
recProcessed(processed, mrna);

struct genePred *gp = mrnaToGenePred(gene, mrna);
if (gp != NULL)
    outputGenePredAndFree(gpFh, gp);
}

static void processGene(FILE *gpFh, struct gff3Ann *gene, struct hash *processed)
/* process a gene node in the tree.  Stop process if maximum errors reached */
{
recProcessed(processed, gene);

struct gff3AnnRef *child;
for (child = gene->children; child != NULL; child = child->next)
    {
    if (sameString(child->ann->type, gff3FeatMRna) && !isProcessed(processed, child->ann))
        processMRna(gpFh, gene, child->ann, processed);
    else if (sameString(child->ann->type, gff3FeatTranscript) && !isProcessed(processed, child->ann))
        processTranscript(gpFh, gene, child->ann, processed);
    if (convertErrCnt >= maxConvertErrors)
        break;
    }
}

static void processRoot(FILE *gpFh, struct gff3Ann *node, struct hash *processed)
/* process a root node in the tree */
{
recProcessed(processed, node);

if (sameString(node->type, gff3FeatGene))
    processGene(gpFh, node, processed);
else if (sameString(node->type, gff3FeatMRna))
    processMRna(gpFh, NULL, node, processed);
else if (sameString(node->type, gff3FeatTranscript))
    processTranscript(gpFh, NULL, node, processed);
}

static void gff3ToGenePred(char *inGff3File, char *outGpFile)
/* gff3ToGenePred - convert a GFF3 file to a genePred file. */
{
// hash of nodes ptrs, prevents dup processing due to dup parents
struct hash *processed = hashNew(12);
struct gff3File *gff3File = loadGff3(inGff3File);
FILE *gpFh = mustOpen(outGpFile, "w");
struct gff3AnnRef *root;
for (root = gff3File->roots; root != NULL; root = root->next)
    {
    if (!isProcessed(processed, root->ann))
        {
        processRoot(gpFh, root->ann, processed);
        if (convertErrCnt >= maxConvertErrors)
            break;
        }
    }
carefulClose(&gpFh);
if (convertErrCnt > 0)
    errAbort("%d errors converting GFF3 file: %s", convertErrCnt, inGff3File); 

#if 0  // free memory for leak debugging if 1
gff3FileFree(&gff3File);
hashFree(&processed);
#endif
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();

maxParseErrors = optionInt("maxParseErrors", maxParseErrors);
if (maxParseErrors < 0)
    maxParseErrors = INT_MAX;
maxConvertErrors = optionInt("maxConvertErrors", maxConvertErrors);
if (maxConvertErrors < 0)
    maxConvertErrors = INT_MAX;
honorStartStopCodons = optionExists("honorStartStopCodons");
gff3ToGenePred(argv[1], argv[2]);
return 0;
}
