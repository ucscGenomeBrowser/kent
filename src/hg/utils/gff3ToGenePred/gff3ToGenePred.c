/* gff3ToGenePred - convert a GFF3 file to a genePred file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "gff3.h"
#include "genePred.h"

static char const rcsid[] = "$Id: gff3ToGenePred.c,v 1.1 2009/08/12 07:48:06 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gff3ToGenePred - convert a GFF3 file to a genePred file\n"
  "usage:\n"
  "   gff3ToGenePred inGff3 outGp\n"
  "options:\n"
  "  -honorStartStopCodons - only set CDS start/stop status to complete if there are\n"
  "   corresponding start_stop codon records\n"
  "This converts:\n"
  "   - top-level gene records with mRNA records\n"
  "   - top-level mRNA records\n"
  "   - mRNA records can contain exon and CDS, or only CDS, or only\n"
  "     exon for non--coding.\n"
  );
}

static struct optionSpec options[] = {
    {"honorStartStopCodons", OPTION_BOOLEAN},
    {NULL, 0},
};
static boolean honorStartStopCodons = FALSE;
static int maxParseErrs = 50;  // maximum number of errors during parse

static struct gff3File *loadGff3(char *inGff3File)
/* load GFF3 into memory */
{
struct gff3File *gff3File = gff3FileOpen(inGff3File, maxParseErrs, NULL);
if (gff3File->errCnt > 0)
    errAbort("errors parsing GFF3 file: %s", inGff3File); 
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

static struct genePred *makeGenePred(struct gff3Ann *gene, struct gff3Ann *mrna, struct gff3AnnRef *exons, struct gff3AnnRef *cdsBlks)
/* construct the empty genePred */
{
if (exons == NULL)
    errAbort("no exons defined for mRNA %s", mrna->id);

int txStart = exons->ann->start;
int txEnd = ((struct gff3AnnRef*)slLastEl(exons))->ann->end;
int cdsStart = (cdsBlks == NULL) ? txEnd : cdsBlks->ann->start;
int cdsEnd = (cdsBlks == NULL) ? txEnd : ((struct gff3AnnRef*)slLastEl(cdsBlks))->ann->end;

if ((mrna->strand == NULL) || (mrna->strand[0] == '?'))
    errAbort("invalid strand for mRNA %s", mrna->id);

struct genePred *gp = genePredNew(mrna->id, mrna->seqid, mrna->strand[0],
                                  txStart, txEnd, cdsStart, cdsEnd,
                                  genePredAllFlds, slCount(exons));
gp->name2 = cloneString(gene->id);

// set start/end status based on codon features if requested
if (honorStartStopCodons)
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
else
    {
    gp->cdsStartStat = cdsComplete;
    gp->cdsEndStat = cdsComplete;
    }
return gp;
}

static void addExons(struct genePred *gp, struct gff3AnnRef *exons)
/* add exons */
{
struct gff3AnnRef *exon;
for (exon = exons; exon != NULL; exon = exon->next)
    {
    int i = gp->exonCount++;
    gp->exonStarts[i] = exon->ann->start;
    gp->exonEnds[i] = exon->ann->end;
    gp->exonFrames[i] = -1;
    }
}

static int findCdsExon(struct genePred *gp, struct gff3Ann *cds, int iExon)
/* search for the exon containing the CDS, starting with iExon+1 */
{
for (iExon++; iExon < gp->exonCount; iExon++)
    {
    if ((gp->exonStarts[iExon] <= cds->start) && (cds->end <= gp->exonEnds[iExon]))
        return iExon;
    }
errAbort("no exon in %s contains CDS %d-%d", gp->name, cds->start, cds->end);
return -1;
}

static void addCdsFrame(struct genePred *gp, struct gff3AnnRef *cdsBlks)
/* assign frame based on CDS regions */
{
struct gff3AnnRef *cds;
int iExon = -1; // caches current position
for (cds = cdsBlks; cds != NULL; cds = cds->next)
    {
    iExon = findCdsExon(gp, cds->ann, iExon);
    gp->exonFrames[iExon] = gff3PhaseToFrame(cds->ann->phase);
    }
}

static void processMRna(FILE *gpFh, struct gff3Ann *gene, struct gff3Ann *mrna, struct hash *processed)
/* process a mRNA node in the tree; gene can be NULL */
{
hashStore(processed, mrna->id);
// allow for only having CDS children
struct gff3AnnRef *exons = getChildFeatures(mrna, gff3FeatExon);
struct gff3AnnRef *cdsBlks = getChildFeatures(mrna, gff3FeatCDS);
struct gff3AnnRef *useExons = (exons != NULL) ? exons : cdsBlks;

struct genePred *gp = makeGenePred((gene != NULL) ? gene : mrna, mrna, useExons, cdsBlks);
addExons(gp, useExons);
addCdsFrame(gp, cdsBlks);

// output before checking so it can be examined
genePredTabOut(gp, gpFh);
if (genePredCheck("GFF3 converted to genePred", stderr, -1, gp) != 0)
    errAbort("conversion failed");

genePredFree(&gp);
slFreeList(&exons);
slFreeList(&cdsBlks);
}

static void processGene(FILE *gpFh, struct gff3Ann *gene, struct hash *processed)
/* process a gene node in the tree */
{
hashStore(processed, gene->id);

struct gff3AnnRef *child;
for (child = gene->children; child != NULL; child = child->next)
    {
    if (sameString(child->ann->type, gff3FeatMRna) && (hashLookup(processed, child->ann->id) == NULL))
        processMRna(gpFh, gene, child->ann, processed);
    }
}

static void processRoot(FILE *gpFh, struct gff3Ann *node, struct hash *processed)
/* process a root node in the tree */
{
hashStore(processed, node->id);

if (sameString(node->type, gff3FeatGene))
    processGene(gpFh, node, processed);
else if (sameString(node->type, gff3FeatMRna))
    processMRna(gpFh, NULL, node, processed);
}

static void gff3ToGenePred(char *inGff3File, char *outGpFile)
/* gff3ToGenePred - convert a GFF3 file to a genePred file. */
{
// hash of nodes record ids, prevents dup processing due to dup parents
struct hash *processed = hashNew(12);
struct gff3File *gff3File = loadGff3(inGff3File);
FILE *gpFh = mustOpen(outGpFile, "w");
struct gff3AnnRef *root;
for (root = gff3File->roots; root != NULL; root = root->next)
    {
    if (hashLookup(processed, root->ann->id) == NULL)
        processRoot(gpFh, root->ann, processed);
    }
    
carefulClose(&gpFh);

#if 1  // free memory for leak debugging if 1
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
honorStartStopCodons = optionExists("honorStartStopCodons");
gff3ToGenePred(argv[1], argv[2]);
return 0;
}
