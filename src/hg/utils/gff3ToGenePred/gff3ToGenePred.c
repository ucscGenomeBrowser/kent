/* gff3ToGenePred - convert a GFF3 file to a genePred file. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include <limits.h>
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "gff3.h"
#include "genePred.h"

#define LEAK_CHECK 0  // set to 1 to free all memory

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gff3ToGenePred - convert a GFF3 file to a genePred file\n"
  "usage:\n"
  "   gff3ToGenePred inGff3 outGp\n"
  "options:\n"
  "  -warnAndContinue - on bad genePreds being created, put out warning but continue\n"
  "  -useName - rather than using 'id' as name, use the 'name' tag\n"
  "  -rnaNameAttr=attr - If this attribute exists on an RNA record, use it as the genePred\n"
  "   name column\n"
  "  -geneNameAttr=attr - If this attribute exists on a gene record, use it as the genePred\n"
  "   name2 column\n"
  "  -attrsOut=file - output attributes of mRNA record to file.  These are per-genePred row,\n"
  "   not per-GFF3 record. Thery are derived from GFF3 attributes, not the attributes themselves.\n"
  "  -processAllGeneChildren - output genePred for all children of a gene regardless of feature\n"
  "  -unprocessedRootsOut=file - output GFF3 root records that were not used.  This will not be a\n"
  "   valid GFF3 file.  It's expected that many non-root records will not be used and they are not\n"
  "   reported.\n"
  "  -bad=file   - output genepreds that fail checks to file\n"
  "  -maxParseErrors=50 - Maximum number of parsing errors before aborting. A negative\n"
  "   value will allow an unlimited number of errors.  Default is 50.\n"
  "  -maxConvertErrors=50 - Maximum number of conversion errors before aborting. A negative\n"
  "   value will allow an unlimited number of errors.  Default is 50.\n"
  "  -honorStartStopCodons - only set CDS start/stop status to complete if there are\n"
  "   corresponding start_stop codon records\n"
  "  -defaultCdsStatusToUnknown - default the CDS status to unknown rather\n"
  "   than complete.\n"
  "  -allowMinimalGenes - normally this programs assumes that genes contains\n"
  "   transcripts which contain exons.  If this option is specified, genes with exons\n"
  "   as direct children of genes and stand alone genes with no exon or transcript\n"
  "   children will be converted.\n"
  "  -refseqHacks - enable various hacks to make RefSeq conversion work:\n"
  "     This turns on -useName, -allowMinimalGenes, and -processAllGeneChildren.\n"
  "     It try harder to find an accession in attributes\n"
  "\n"
  "This converts:\n"
  "   - top-level gene records with RNA records\n"
  "   - top-level RNA records\n"
  "   - RNA records that contain:\n"
  "       - exon and CDS\n"
  "       - CDS, five_prime_UTR, three_prime_UTR\n"
  "       - only exon for non-coding\n"
  "   - top-level gene records with transcript records\n"
  "   - top-level transcript records\n"
  "   - transcript records that contain:\n"
  "       - exon\n"
  "where RNA can be mRNA, ncRNA, or rRNA, and transcript can be either\n"
  "transcript or primary_transcript\n"
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
    {"warnAndContinue", OPTION_BOOLEAN},
    {"useName", OPTION_BOOLEAN},
    {"rnaNameAttr", OPTION_STRING},
    {"geneNameAttr", OPTION_STRING},
    {"honorStartStopCodons", OPTION_BOOLEAN},
    {"defaultCdsStatusToUnknown", OPTION_BOOLEAN},
    {"allowMinimalGenes", OPTION_BOOLEAN},
    {"attrsOut", OPTION_STRING},
    {"bad", OPTION_STRING},
    {"processAllGeneChildren", OPTION_BOOLEAN},
    {"refseqHacks", OPTION_BOOLEAN},
    {"unprocessedRootsOut", OPTION_STRING},
    {NULL, 0},
};
static boolean useName = FALSE;
static char* rnaNameAttr = NULL;
static char* geneNameAttr = NULL;
static boolean warnAndContinue = FALSE;
static boolean honorStartStopCodons = FALSE;
static boolean defaultCdsStatusToUnknown = FALSE;
static boolean allowMinimalGenes = FALSE;
static boolean processAllGeneChildren = FALSE;
static boolean refseqHacks = FALSE;
static int maxParseErrors = 50;  // maximum number of errors during parse
static int maxConvertErrors = 50;  // maximum number of errors during conversion
static int convertErrCnt = 0;  // number of convert errors

static FILE *outAttrsFp = NULL;
static FILE *outBadFp = NULL;
static FILE *outUnprocessedRootsFp = NULL;

static char **cdsFeatures[] = {
    &gff3FeatCDS,
    NULL
};
static char **cdsExonFeatures[] = {
    &gff3FeatExon,
    &gff3FeatCDS,
    NULL
};
static char **cdjvFeatures[] = {
    &gff3FeatCGeneSegment,
    &gff3FeatDGeneSegment,
    &gff3FeatJGeneSegment,
    &gff3FeatVGeneSegment,
    NULL
};
static char** geneFeatures[] = {
    &gff3FeatGene,
    &gff3FeatPseudogene,
    NULL
};
static char** transFeatures[] = {
    &gff3FeatMRna,
    &gff3FeatNCRna,
    &gff3FeatCDS,
    &gff3FeatRRna,
    &gff3FeatTRna,
    &gff3FeatCGeneSegment,
    &gff3FeatDGeneSegment,
    &gff3FeatJGeneSegment,
    &gff3FeatVGeneSegment,
    &gff3FeatTranscript,
    &gff3FeatPrimaryTranscript,
    NULL
};

static void cnvError(char *format, ...)
/* print a GFF3 to gene conversion error.  This will return.  Code must check
 * for error count to be exceeded and unwind to the top level to print a usefull
 * error message and abort. */
{
if (warnAndContinue)
    fputs("Warning: skipping: ", stderr);
else
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
unsigned flags = 0;
if (warnAndContinue)
    flags |= GFF3_WARN_WHEN_POSSIBLE;
struct gff3File *gff3File = gff3FileOpen(inGff3File, maxParseErrors, flags, NULL);
if (gff3File->errCnt > 0)
    errAbort("%d errors parsing GFF3 file: %s", gff3File->errCnt, inGff3File); 
return gff3File;
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

static boolean featTypeMatch(char *type, char** types[])
/* check if type is in NULL terminated list of feature type name.
 * This has one extra level of indirection to allow initializing
 * from GFF3 feature definitions (gff3FeatCds, etc). */
{
int i;
for (i = 0; types[i] != NULL; i++)
    if (sameString(type, *(types[i])))
        return TRUE;
return FALSE;
}

static struct gff3Ann *findChildTypeMatch(struct gff3Ann *node, struct gff3AnnRef *prevChildRef, char** types[])
/* Find the first or next child of node of one of the particular types.  The prevChild arg should
 * be NULL on the first call */
{
struct gff3AnnRef *child;
for (child = ((prevChildRef == NULL) ? node->children : prevChildRef);
     child != NULL; child = child->next)
    if (featTypeMatch(child->ann->type, types))
        return child->ann;
return NULL;
}

static boolean haveChildTypeMatch(struct gff3Ann *node, char** types[])
/* Does the node have a child matching one of the types. */
{
return findChildTypeMatch(node, NULL, types) != NULL;
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

static boolean isGeneWithCdsChildCase(struct gff3Ann* mrna)
/* is this one of the refseq gene with a direct CDS child? */
{
return sameString(mrna->type, gff3FeatGene) && (mrna->children != NULL)
    && (sameString(mrna->children->ann->type, gff3FeatCDS));
}

static char* refSeqHacksFindName(struct gff3Ann* mrna)
/* return the value to use for the genePred name field under refSeqHacks
 * rules. */
{
// if this is a gene with CDS child, the get the id out of the CDS if it looks like
// a refseq accession
if (isGeneWithCdsChildCase(mrna))
    {
    // is name something like YP_203370.1 (don't try too hard)
    struct gff3Ann *cds = mrna->children->ann;
    if ((strlen(cds->name) > 4) && isupper(cds->name[0]) && isupper(cds->name[1])
        && (cds->name[2] == '_') && isdigit(cds->name[3]))
        return cds->name;
    }
return NULL;
}

static char* getRnaName(struct gff3Ann* mrna)
/* return the value to use for the genePred name field */
{
char *name = NULL;
if (rnaNameAttr != NULL)
    {
    struct gff3Attr *attr = gff3AnnFindAttr(mrna, rnaNameAttr);
    if (attr != NULL)
        name = attr->vals->name;
    }
if (isEmpty(name) && refseqHacks)
    name = refSeqHacksFindName(mrna);
if (isEmpty(name))
    name = (useName ? mrna->name : mrna->id);
if (isEmpty(name))
    name = mrna->id;
return name;
}

static char* getGeneName(struct gff3Ann* gene)
/* return the value to use for the genePred name2 field,
 * or NULL if can't be defined. */
{
char *name2 = NULL;
if (geneNameAttr != NULL)
    {
    struct gff3Attr *attr = gff3AnnFindAttr(gene, geneNameAttr);
    if (attr != NULL)
        name2 = attr->vals->name;
    }
if (isEmpty(name2) && useName)
    name2 = gene->name;
if (isEmpty(name2))
    name2 = gene->id;
return name2;
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

struct genePred *gp = genePredNew(getRnaName(mrna), mrna->seqid, mrna->strand[0],
                                  txStart, txEnd, cdsStart, cdsEnd,
                                  genePredAllFlds, slCount(exons));
gp->name2 = cloneString(getGeneName(gene));

// set start/end status based on codon features if requested
if (honorStartStopCodons)
    {
    setCdsStatFromCodons(gp, mrna);
    }
else if (gp->cdsStart < gp->cdsEnd)
    {
    gp->cdsStartStat = gp->cdsEndStat = (defaultCdsStatusToUnknown ? cdsUnknown : cdsComplete);
    }
else
    {
    gp->cdsStartStat = gp->cdsEndStat = cdsNone;
    }
return gp;
}


static void doOutAttrs(FILE *outAttrsFp, char *name, struct gff3Ann *ann)
/* Write attributes for -attrsOut */
{
struct gff3Attr *attr;
for(attr=ann->attrs; attr; attr=attr->next)
    {
    fprintf(outAttrsFp, "%s\t%s\t", name, attr->tag);
    struct slName *val;
    for (val = attr->vals; val; val = val->next)
        {
        if (val != attr->vals)
            fputc(',', outAttrsFp);
        fprintf(outAttrsFp, "%s", val->name);
        }
    fputc('\n', outAttrsFp);
    }
}

static void outputGenePred(struct gff3Ann *mrna, FILE *gpFh, struct genePred *gp)
/* validate and output a genePred */
{
if (gp->name == NULL)
    gp->name=cloneString("no_name");

char description[PATH_LEN];
safef(description, sizeof(description), "genePred from GFF3: %s:%d",
      ((mrna->file != NULL) ? mrna->file->fileName : "<unknown>"),
      mrna->lineNum);
int ret = genePredCheck(description, stderr, -1, gp);
if (ret == 0)
    {
    genePredTabOut(gp, gpFh);
    if (outAttrsFp)
        doOutAttrs(outAttrsFp, gp->name,  mrna);
    }
else
    {
    if (outBadFp)
        genePredTabOut(gp, outBadFp);
    cnvError("invalid genePred created: %s %s:%d-%d", gp->name, gp->chrom, gp->txStart, gp->txEnd);
    }
}

static bool adjacentBlocks(struct gff3Ann *prevAnn, struct gff3Ann *ann)
/* are two features adjacent? prevAnn can be NULL */
{
return (prevAnn != NULL) && (prevAnn->end == ann->start);
}

static void addExon(struct genePred *gp, struct gff3Ann *prevAnn, struct gff3Ann *ann)
/* add one block as an exon, merging adjacent blocks */
{
int i = gp->exonCount;
if (adjacentBlocks(prevAnn, ann))
    {
    gp->exonEnds[i-1] = ann->end; // extend
    }
else
    {
    gp->exonStarts[i] = ann->start;
    gp->exonEnds[i] = ann->end;
    gp->exonFrames[i] = -1;
    gp->exonCount++;
    }
}
    
static void addExons(struct genePred *gp, struct gff3AnnRef *blks)
/* Add exons.  Blks can either be exon records or utr and cds records.  This
 * is necessary to define genePred exons as GFF3 doesn't insist on an exact
 * annotation hierarchy. If blks are adjacent, they will be joined.  exons
 * records maybe split later when assigning CDS frame
 */
{
struct gff3AnnRef *blk, *prevBlk = NULL;
for (blk = blks; blk != NULL; prevBlk = blk, blk = blk->next)
    {
    addExon(gp, ((prevBlk == NULL) ? NULL : prevBlk->ann), blk->ann);
    }
}

static int findCdsExon(struct genePred *gp, struct gff3Ann *cds)
/* search for the exon containing the CDS, starting with iExon+1, return -1 on error */
{
// don't use cached iExon.  Will fail on ribosomal frame-shifted genes
// see NM_015068.
int iExon;
for (iExon=0; iExon < gp->exonCount; iExon++)
    {
    if ((gp->exonStarts[iExon] <= cds->start) && (cds->end <= gp->exonEnds[iExon]))
        return iExon;
    }
cnvError("no exon in %s contains CDS %d-%d", gp->name, cds->start, cds->end);
return -1;
}

static boolean validateCds(struct genePred *gp, struct gff3AnnRef *cdsBlks)
/* Validate that the CDS is contained within exons.  If this is not
 * true, the code to assign frames will generate bad genePreds. */
{
struct gff3AnnRef *cds;
for (cds = cdsBlks; cds != NULL; cds = cds->next)
    {
    if (findCdsExon(gp, cds->ann) < 0)
        return FALSE; // error
    }
return TRUE;
}

static struct gff3AnnRef* findCdsForExon(struct genePred *gp, int iExon, struct gff3AnnRef *cdsBlks)
/* search for CDS records contained in an exon. Multiple maybe returned if there is
 * are frame shift. */
{
struct gff3AnnRef* exonCds = NULL;
struct gff3AnnRef *cdsBlk;
for (cdsBlk = cdsBlks; cdsBlk != NULL; cdsBlk = cdsBlk->next)
    {
    if ((gp->exonStarts[iExon] <= cdsBlk->ann->start) && (cdsBlk->ann->end <= gp->exonEnds[iExon]))
        slAddTail(&exonCds, gff3AnnRefNew(cdsBlk->ann));
    }
return exonCds;
}

static void expandExonArrays(struct genePred *gp, int iExon, int numEntries)
/* expand the exon arrays by numEntries after iExon, moving subsequent entries
 * down.  */
{
ExpandArray(gp->exonStarts, gp->exonCount, gp->exonCount+numEntries);
ExpandArray(gp->exonEnds, gp->exonCount, gp->exonCount+numEntries);
ExpandArray(gp->exonFrames, gp->exonCount, gp->exonCount+numEntries);

// move down all that follow iExon
int numMove = (gp->exonCount - iExon)-1;
memmove(gp->exonStarts+iExon+1+numEntries, gp->exonStarts+iExon+1, numMove*sizeof(unsigned));
memmove(gp->exonEnds+iExon+1+numEntries, gp->exonEnds+iExon+1, numMove*sizeof(unsigned));
memmove(gp->exonFrames+iExon+1+numEntries, gp->exonFrames+iExon+1, numMove*sizeof(int));
gp->exonCount += numEntries;
}

static void exonCdsReplacePart(struct genePred *gp, int exonStart, int exonEnd,
                               int iExon, struct gff3AnnRef *cds, boolean isFirst)
/* replace one exon record based on CDS. */
{
assert((iExon >= 0) && (iExon < gp->exonCount));
assert(isFirst || (iExon > 0));

// allow for UTR regions at start and when setting bounds
gp->exonStarts[iExon] = isFirst ? exonStart : cds->ann->start;
gp->exonEnds[iExon] = (cds->next == NULL) ? exonEnd : cds->ann->end;
gp->exonFrames[iExon] = gff3PhaseToFrame(cds->ann->phase);

// if the cds blocks overlap, we have a negative frameshift, correct for
// this in the genePred.  Need to adjust frame on the plus strand only
// since we are arbitrary deciding to move the start of the second CDS
// block so it doesn't overlap.
if ((!isFirst) && (gp->exonStarts[iExon] <= gp->exonEnds[iExon-1]))
    {
    int dist = (gp->exonEnds[iExon-1] - gp->exonStarts[iExon]);
    gp->exonStarts[iExon] += dist;
    if (sameString(gp->strand, "+"))
        gp->exonFrames[iExon] = (gp->exonFrames[iExon] + dist) %3;
    }
}

static void exonCdsReplace(struct genePred *gp, int iExon, struct gff3AnnRef *exonCds)
/* replace an exon records based on CDS.  This deals with frame-shifting cases */
{
int exonStart = gp->exonStarts[iExon];
int exonEnd = gp->exonEnds[iExon];
expandExonArrays(gp, iExon, slCount(exonCds)-1);
int cnt = 0;
struct gff3AnnRef *cds;
for (cds = exonCds; cds != NULL; cds = cds->next, cnt++, iExon++)
    exonCdsReplacePart(gp, exonStart, exonEnd, iExon, cds, (cnt == 0));
}

static int addExonFrame(struct genePred *gp, int iExon, struct gff3AnnRef *cdsBlks)
/* assign frame for an exon.  This might have to add exon array entries.  The next
 * exon entry is returned.*/
{
struct gff3AnnRef *exonCds = findCdsForExon(gp, iExon, cdsBlks);
int exonCdsCnt = slCount(exonCds);
if (exonCdsCnt == 0)
    return iExon+1;  // no CDS
else if (exonCdsCnt == 1)
    {
    gp->exonFrames[iExon] = gff3PhaseToFrame(exonCds->ann->phase);
    slFreeList(&exonCds);
    return iExon+1;
    }
else
    {
    // slow path, must make room.  This might leave a gap if there is
    // programmed frameshift
    exonCdsReplace(gp, iExon, exonCds);
    slFreeList(&exonCds);
    return iExon+exonCdsCnt;
    }
}

static void addExonFrames(struct genePred *gp, struct gff3AnnRef *cdsBlks)
/* assign frames for exons. this may expand the existing exon array due to
 * needing to track frameshifts. */
{
int iExon;
for (iExon = 0; iExon < gp->exonCount;)
    {
    iExon = addExonFrame(gp, iExon, cdsBlks);
    }
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
/* construct a genePred from an mRNA or transript record, return NULL if there is an error.
 * The gene argument maybe null if there isn't a gene parent */
{
// allow for only having UTR/CDS children
struct gff3AnnRef *exons = getChildFeatures(mrna, gff3FeatExon);
struct gff3AnnRef *cdsBlks = getChildFeatures(mrna, gff3FeatCDS);
struct gff3AnnRef *cdsUtrBlks = NULL;  // used if no exons
if (exons == NULL)
    cdsUtrBlks = getCdsUtrBlks(mrna);
struct gff3AnnRef *useExons = (exons != NULL) ? exons : cdsUtrBlks;

if (useExons == NULL) // if we don't have any exons, just use the feature
    {
    useExons = gff3AnnRefNew(mrna);
    if (sameString(mrna->type, gff3FeatCDS))
	cdsBlks = useExons;
    }

struct genePred *gp = makeGenePred((gene != NULL) ? gene : mrna, mrna, useExons, cdsBlks);
if (gp != NULL)
    {
    addExons(gp, useExons);
    if (!validateCds(gp, cdsBlks))
        genePredFree(&gp);
    else
        addExonFrames(gp, cdsBlks);
    }
slFreeList(&exons);
slFreeList(&cdsBlks);
slFreeList(&cdsUtrBlks);
return gp;  // NULL if error above
}

static struct genePred *standaloneGeneToGenePred(struct gff3Ann *gene)
/* construct a genePred using only the gene record and no children */
{
// fake structure so entire gene becomes one block
struct gff3AnnRef fakeExons = {NULL, gene};
struct genePred *gp = makeGenePred(gene, gene, &fakeExons, NULL);
if (gp != NULL)
    addExons(gp, &fakeExons);
return gp;  // NULL if error above
}

static boolean isNcbiLikeSegmentGene(struct gff3Ann *gene)
/* NCBI annotates [CDJV]_gene_segment genes as a gene with a CDJV]_gene_segment
 * child with exon children. However the CDS features are direct children of the gene
 * rather than the [CDJV]_gene_segment.  This is different than all other
 * transcript for some reason.  This will find NCBI annotations and imitations.   */
{
return haveChildTypeMatch(gene, cdsFeatures)
    && haveChildTypeMatch(gene, cdjvFeatures);
}

static void fixNcbiLikeSegmentGene(struct gff3Ann *gene)
/* adjust gene structure (see above) dropping CDS annotation and keeping
 * [CDJV]_gene_segment features as `transcripts' along with their exons.
 * Sometimes the CDS extends outside of exon bounds.  For multi-transcript
 * genes it is hard to figure out where to put the CDS. */
{
// Drop CDS, always starting search from start due to removing
warn("Warning: dropping CDS from %s %s at %s:%d-%d as we are unable to convert this form of annotation to genePred",
     gene->type, getGeneName(gene), gene->seqid, gene->start, gene->end);
struct gff3Ann *cdsAnn;
while ((cdsAnn = findChildTypeMatch(gene, NULL, cdsFeatures)) != NULL)
    gff3UnlinkChild(gene, cdsAnn);
}

static boolean shouldProcessAsTranscript(struct gff3Ann *node)
/* Decide if we should process this feature as a transcript and turn it into a
 * genePred. */
{
char *parentType = node->parents ? node->parents->ann->type : NULL;
return (processAllGeneChildren
        && (node->parents != NULL)
        && featTypeMatch(parentType, geneFeatures))
    || featTypeMatch(node->type, transFeatures);
}

static boolean shouldProcessGeneAsStandard(struct gff3Ann *gene)
/* does a gene have the standard structure of transcript or transcript-related
   annotations (like CDS) children? */
{
struct gff3AnnRef *child;
for (child = gene->children; child != NULL; child = child->next)
    {
    if (shouldProcessAsTranscript(child->ann))
        return TRUE;
    }
return FALSE;
}

static boolean shouldProcessGeneAsTranscript(struct gff3Ann *gene)
/* should this gene be processed as a transcripts? */
{
// check for any exon children
return allowMinimalGenes && haveChildTypeMatch(gene, cdsExonFeatures);
}

static void processTranscript(FILE *gpFh, struct gff3Ann *gene, struct gff3Ann *mrna,
                              struct hash *processed)
/* process a mRNA/transcript node in the tree; gene can be NULL. Error count
   increment on error and genePred discarded */
{
recProcessed(processed, mrna);

struct genePred *gp = mrnaToGenePred(gene, mrna);
if (gp != NULL)
    {
    outputGenePred(mrna, gpFh, gp);
    genePredFree(&gp);
    }
}

static void processGeneTranscripts(FILE *gpFh, struct gff3Ann *gene, struct hash *processed)
/* process transcript records of a gene */
{
if (outAttrsFp)
    doOutAttrs(outAttrsFp, gene->id,  gene);
struct gff3AnnRef *child;
for (child = gene->children; child != NULL; child = child->next)
    {
    if (shouldProcessAsTranscript(child->ann) 
        && !isProcessed(processed, child->ann))
        processTranscript(gpFh, gene, child->ann, processed);
    if ((convertErrCnt >= maxConvertErrors) && !warnAndContinue)
        break;
    }
}

static void processGeneStandalone(FILE *gpFh, struct gff3Ann *gene, struct hash *processed)
/* process a gene as a single entry with ignoring non-supported children */
{
struct genePred *gp = standaloneGeneToGenePred(gene);
if (gp != NULL)
    {
    outputGenePred(gene, gpFh, gp);
    genePredFree(&gp);
    }
}

static void processGene(FILE *gpFh, struct gff3Ann *gene, struct hash *processed)
/* process a gene node in the tree.  Stop process if maximum errors reached */
{
recProcessed(processed, gene);
if (isNcbiLikeSegmentGene(gene))
    fixNcbiLikeSegmentGene(gene);

if (shouldProcessGeneAsTranscript(gene))
    processTranscript(gpFh, NULL, gene, processed);
else if (shouldProcessGeneAsStandard(gene))
    processGeneTranscripts(gpFh, gene, processed);
else if (allowMinimalGenes)
    processGeneStandalone(gpFh, gene, processed);
}

static void processRoot(FILE *gpFh, struct gff3Ann *node, struct hash *processed)
/* process a root node in the tree */
{
if (sameString(node->type, gff3FeatGene) || sameString(node->type, gff3FeatPseudogene))
    processGene(gpFh, node, processed);
else if (shouldProcessAsTranscript(node))
    processTranscript(gpFh, NULL, node, processed);
}

static void processRoots(FILE *gpFh, struct gff3AnnRef *roots, struct hash *processed)
/* process all root node in the tree */
{
struct gff3AnnRef *root;
for (root = roots; root != NULL; root = root->next)
    {
    if (!isProcessed(processed, root->ann))
        {
        processRoot(gpFh, root->ann, processed);
        if (convertErrCnt >= maxConvertErrors)
            break;
        }
    }
}

static void writeUnprocessedRoots(struct gff3AnnRef *roots, struct hash *processed)
/* output unprocessed records */
{
struct gff3AnnRef *root;
for (root = roots; root != NULL; root = root->next)
    {
    if (!isProcessed(processed, root->ann))
        gff3AnnWrite(root->ann, outUnprocessedRootsFp);
    }
}

static void gff3ToGenePred(char *inGff3File, char *outGpFile)
/* gff3ToGenePred - convert a GFF3 file to a genePred file. */
{
// hash of nodes ptrs, prevents dup processing due to dup parents
struct hash *processed = hashNew(12);
struct gff3File *gff3File = loadGff3(inGff3File);
FILE *gpFh = mustOpen(outGpFile, "w");
processRoots(gpFh, gff3File->roots, processed);
carefulClose(&gpFh);
if (outUnprocessedRootsFp != NULL)
    writeUnprocessedRoots(gff3File->roots,  processed);
if (convertErrCnt > 0)
    {
    if (warnAndContinue)
        warn("%d warnings converting GFF3 file: %s", convertErrCnt, inGff3File);
    else
        errAbort("%d errors converting GFF3 file: %s", convertErrCnt, inGff3File);
    }

#if LEAK_CHECK  // free memory for leak debugging if 1
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

warnAndContinue = optionExists("warnAndContinue");
useName = optionExists("useName");
rnaNameAttr = optionVal("rnaNameAttr", NULL);
geneNameAttr = optionVal("geneNameAttr", NULL);
maxParseErrors = optionInt("maxParseErrors", maxParseErrors);
if (maxParseErrors < 0)
    maxParseErrors = INT_MAX;
maxConvertErrors = optionInt("maxConvertErrors", maxConvertErrors);
if (maxConvertErrors < 0)
    maxConvertErrors = INT_MAX;
honorStartStopCodons = optionExists("honorStartStopCodons");
defaultCdsStatusToUnknown = optionExists("defaultCdsStatusToUnknown");
if (honorStartStopCodons && defaultCdsStatusToUnknown)
    errAbort("can't specify both -honorStartStopCodons and -defaultCdsStatusToUnknown");
allowMinimalGenes = optionExists("allowMinimalGenes");
processAllGeneChildren = optionExists("processAllGeneChildren");
refseqHacks = optionExists("refseqHacks");
if (refseqHacks)
    {
    useName = TRUE;
    allowMinimalGenes = TRUE;
    processAllGeneChildren = TRUE;
    }
char *bad = optionVal("bad", NULL);
if (bad != NULL)
    outBadFp = mustOpen(bad, "w");
char *attrsOut = optionVal("attrsOut", NULL);
if (attrsOut != NULL)
    outAttrsFp = mustOpen(attrsOut, "w");
char *unprocessedRootsOut = optionVal("unprocessedRootsOut", NULL);
if (unprocessedRootsOut != NULL)
    outUnprocessedRootsFp = mustOpen(unprocessedRootsOut, "w");
gff3ToGenePred(argv[1], argv[2]);
carefulClose(&outBadFp);
carefulClose(&outAttrsFp);
carefulClose(&outUnprocessedRootsFp);
return 0;
}
