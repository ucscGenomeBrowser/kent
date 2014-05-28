/* Classify mutations as missense etc. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include <math.h>
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "bed.h"
#include "jksql.h"
#include "options.h"
#include "hdb.h"
#include "genePred.h"
#include "dnautil.h"
#include "assert.h"
#include "hash.h"
#include "dnaMotif.h"
#include "dnaMotifSql.h"
#include "genomeRangeTree.h"
#include "dnaMarkov.h"
#include "dnaMarkovSql.h"
#include "bed6FloatScore.h"
#include "nibTwo.h"
#include "binRange.h"

int debug = 0;
char *outputExtension = NULL;
boolean bindingSites = FALSE;
char *geneModel;
boolean lazyLoading = FALSE;           // avoid loading DNA for all known genes (performance hack if you are classifying only a few items).
float minDelta = -1;                   // minDelta == 0 means output all deltas
char *clusterTable = "wgEncodeRegTfbsClusteredMotifs";
char *motifTable;
char *markovTable;
static int maxNearestGene = 10000;
boolean oneBased = FALSE;
boolean skipGeneModel = FALSE;

#ifdef TCGA_CODES

#define SPLICE_SITE "Splice_Site_SNP"
#define MISSENSE "Missense_Mutation"
#define READ_THROUGH "Read_Through"
#define NONSENSE "Nonsense_Mutation"
#define NONSENSE_LAST_EXON "Nonsense_Mutation(Last_Exon)"
#define SYNONYMOUS "Silent"
#define IN_FRAME_DEL "In_Frame_Del"
#define IN_FRAME_INS "In_Frame_Ins"
#define FRAME_SHIFT_DEL "Frame_Shift_Del"
#define FRAME_SHIFT_INS "Frame_Shift_Ins"
#define THREE_PRIME_UTR "3'UTR"
#define FIVE_PRIME_UTR "5'UTR"
#define INTRON "Intron"
#define INTERGENIC "Intergenic"
#define REGULATORY "Regulatory"
#define NONCODING "Non_Coding"

#else

#define SPLICE_SITE "spliceSite"
#define MISSENSE "missense"
#define READ_THROUGH "readThrough"
#define NONSENSE "nonsense"
#define NONSENSE_LAST_EXON "nonsenseLastExon"
#define SYNONYMOUS "synonymous"
#define IN_FRAME_DEL "inFrameDel"
#define IN_FRAME_INS "inFrameIns"
#define FRAME_SHIFT_DEL "frameShiftDel"
#define FRAME_SHIFT_INS "frameShiftIns"
#define THREE_PRIME_UTR "threePrimeUtr"
#define FIVE_PRIME_UTR "fivePrimeUtr"
#define INTERGENIC "intergenic"
#define REGULATORY "regulatory"
#define NONCODING "nonCoding"
#define INTRON "intron"

#endif


static struct optionSpec optionSpecs[] = {
    {"bindingSites", OPTION_BOOLEAN},
    {"clusterTable", OPTION_STRING},
    {"geneModel", OPTION_STRING},
    {"lazyLoading", OPTION_BOOLEAN},
    {"minDelta", OPTION_FLOAT},
    {"markovTable", OPTION_STRING},
    {"maxNearestGene", OPTION_INT},
    {"motifTable", OPTION_STRING},
    {"outputExtension", OPTION_STRING},
    {"skipGeneModel", OPTION_BOOLEAN},
    {"oneBased", OPTION_BOOLEAN},
    {NULL, 0}
};

struct bed7
/* A seven field bed. */
    {
    struct bed7 *next;
    char *chrom;	/* Allocated in hash. */
    unsigned chromStart;	/* Start (0 based) */
    unsigned chromEnd;	/* End (non-inclusive) */
    char *name;	/* Name of item */
    float score;
    char strand;
    char *key; // user provided stuff
    char *code; // used internally
    };

static DNA parseSnp(char *name, DNA *before, DNA *refCall);

struct genePredStub
{
    // stub used to allow return multiple references to the same gene prediction in a linked list;
    // this is a hack to save a lot of memory by avoiding making lots of copies of a genePred which only have
    // a different next value.

    struct genePredStub *next;
    struct genePred *genePred;
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mutationClassifier - classify mutations \n"
  "usage:\n"
  "   mutationClassifier database snp.bed(s)\n"
  "options:\n"
  "   -bindingSites\tPrint out only mutations in binding sites.\n"
  "   -clusterTable\tUse this binding site table to look for mutations in binding sites.\n"
  "   -geneModel=table\tGene model. Default model is knownCanonical. Table must be a valid gene prediction table;\n"
  "   \t\t\te.g. ensGene or refGene.\n"
  "   -lazyLoading\t\tAvoid loading complete gene model (performance hack for when you are classifying only a few items).\n"
  "   -maxNearestGene\tMaximum distance used when computing nearest gene (default: %d).\n"
  "   -minDelta\t\tLook for regulatory sites with score(after) - score(before) < minDelta (default: %.1f).\n"
  "\t\t\tLogic is reversed if positive (i.e. look for sites with score(after) - score(before) > minDelta.)\n"
  "   -oneBased\t\tUse one-based coordinates instead of zero-based.\n"
  "   -outputExtension\tCreate an output file with this extension for each input file (instead of writing to stdout).\n"
  "   -verbose=N\t\tverbose level for extra information to STDERR\n\n"
  "Classifies SNPs and indels which are in coding regions of UCSC\n"
  "canononical genes as synonymous or non-synonymous.\n"
  "Prints bed4 for identified SNPs; name field contains the codon transformation.\n"
  "Standard single character amino acid codes are used; '*' == stop codon.\n"
  "output is bed4+ with classification code in the name field, and additonal\n"
  "annotations in subsequent fields.\n\n"
  "Mutations are classified with the following codes:\n"
  "%s\n"
  "%s\n"
  "%s\n"
  "%s\n"
  "%s\n"
  "%s\n"
  "%s\n"
  "%s\n"
  "%s\n"
  "%s\n"
  "%s\toutput includes factor name, score change, and nearest gene (within %d bases)\n"
  "%s\n"
  "\n"
  "snp.bed should be bed4 (with SNP base in the name field).\n"
  "SNPs are assumed to be on positive strand, unless snp.bed is bed7 with\n"
  "explicit strand field (score field is ignored). SNPs should be\n"
  "zero-length bed entries; all other entries are assumed to be indels.\n"
  "\n"
  "example:\n"
  "     mutationClassifier hg19 snp.bed\n",
  maxNearestGene, minDelta,
  SPLICE_SITE, MISSENSE, READ_THROUGH, NONSENSE, NONSENSE_LAST_EXON, SYNONYMOUS, IN_FRAME_DEL, IN_FRAME_INS, FRAME_SHIFT_DEL, FRAME_SHIFT_INS, REGULATORY, maxNearestGene, INTRON
  );
}

static struct bed7 *shallowBedCopy(struct bed7 *bed)
/* Make a shallow copy of given bed item (i.e. only replace the next pointer) */
{
struct bed7 *newBed;
if (bed == NULL)
    return NULL;
AllocVar(newBed);
memcpy(newBed, bed, sizeof(*newBed));
newBed->next = NULL;
return newBed;
}

static struct genePredStub *genePredStubCopy(struct genePred *gp)
// Make a genePredStub of genePred.
{
struct genePredStub *newGenePred;
if (gp == NULL)
    return NULL;
AllocVar(newGenePred);
newGenePred->genePred = gp;
newGenePred->next = NULL;
return newGenePred;
}

static int myBedCmp(const void *va, const void *vb)
/* slSort callback to sort based on chrom,chromStart. */
{
     const struct bed *a = *((struct bed **)va);
     const struct bed *b = *((struct bed **)vb);
     int diff = strcmp(a->chrom, b->chrom);
     if (!diff)
	  diff = a->chromStart - b->chromStart;
     return diff;
}

static int bedItemsOverlap(struct bed *a, struct genePred *b)
{
     return (!strcmp(a->chrom, b->chrom) &&
	     a->chromEnd > b->txStart &&
	     a->chromStart <= b->txEnd);
}

static int intersectBeds (struct bed7 *a, struct genePred *b,
                          struct bed7 **aCommon, struct genePredStub **bCommon,
                          struct bed7 **aUnmatched)
{
// NOTE that because of the definition of this function, aCommon and bCommon can have 
// duplicate copies from a and b respectively.

int count = 0;
struct hash *hash = newHash(0);
struct hashEl *hel;
struct binKeeper *bk;
struct bed7 *curA;
struct genePred *curB;

for (curB = b; curB != NULL; curB = curB->next)
    {
    hel = hashLookup(hash, curB->chrom);
    if (hel == NULL)
        {
        bk = binKeeperNew(0, 511*1024*1024);
        hel = hashAdd(hash, curB->chrom, bk);
        }
    else
        bk = hel->val;
    binKeeperAdd(bk, curB->txStart, curB->txEnd, curB);
    }

for (curA = a; curA != NULL; curA = curA->next)
    {
    boolean added = FALSE;
    bk = hashFindVal(hash, curA->chrom);
    if (bk == NULL)
        // this is unlikely to happen when using large bed files
        verbose(2, "Couldn't find chrom %s\n", curA->chrom);
    else
        {
	struct binElement *hit, *hitList = binKeeperFind(bk, curA->chromStart, curA->chromEnd == curA->chromStart ? curA->chromStart + 1 : curA->chromEnd);
        for (hit = hitList; hit != NULL; hit = hit->next)
            {
            added = TRUE;
            struct bed7 *tmpA = shallowBedCopy(curA);
            slAddHead(aCommon, tmpA);
            struct genePredStub *tmpB = genePredStubCopy((struct genePred *) hit->val);
            slAddHead(bCommon, tmpB);
            count++;
            }
	slFreeList(&hitList);
        }
    if(!added)
        slAddHead(aUnmatched, shallowBedCopy(curA));
    }
return count;
}

void readBedFile(struct bed7 **list, char *fileName)
{
// read bed file (we handle bed4 or bed7)
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordCount;
char *row[40];
struct bed7 *bed;
int count = 0;
int valid = 0;

while ((wordCount = lineFileChop(lf, row)) != 0)
    {
    char *chrom = row[0];
    int start = lineFileNeedNum(lf, row, 1);
    int end = lineFileNeedNum(lf, row, 2);
    if (start > end)
        errAbort("start after end line %d of %s", lf->lineIx, lf->fileName);
    AllocVar(bed);
    bed->chrom = cloneString(chrom);
    bed->chromStart = start;
    bed->chromEnd = end;
    if (oneBased)
	{
	bed->chromStart -= 1;
	bed->chromEnd   -= 1;
	}
    bed->name = cloneString(row[3]);

    count++;
    if (wordCount >= 5)
        {
        bed->score = lineFileNeedDouble(lf, row, 4);
        // fprintf(stderr, "%.2f\n", bed->score);
        // XXXX Add a way to have a score cutoff?
//        if(bed->score < 0.95)
//            continue;
        }

    if(bed->chromStart == bed->chromEnd && parseSnp(bed->name, NULL, NULL))
        valid++;

    if (wordCount >= 6)
        {
        bed->strand = row[5][0];
        if (bed->strand == '-')
            {
            // we support this so we can process dbSnp data (which has reverse strand SNPs).
            int i, len = strlen(bed->name);
            // complement doesn't work on some punctuation, so only complement the bases
            for(i = 0; i < len; i++)
                if(isalpha(bed->name[i]))
                    complement(bed->name + i, 1);
            }
        if (wordCount >= 7)
            bed->key = cloneString(row[6]);
        }
    slAddHead(list, bed);
    }
lineFileClose(&lf);
verbose(2, "%s: valid: %d; count: %d; %.2f\n", fileName, valid, count, ((float) valid) / count);
}

static void printLine(FILE *stream, char *chrom, int chromStart, int chromEnd, char *name, char *code, char *additional, char *key)
{
if(bindingSites)
    return;
if(oneBased)
    {
    chromStart++;
    chromEnd++;
    }
fprintf(stream, "%s\t%d\t%d\t%s\t%s\t%s", chrom, chromStart, chromEnd, name, code, additional);
if(key == NULL)
    fprintf(stream, "\n");
else
    fprintf(stream, "\t%s\n", key);
}

static struct dnaSeq *getChromSeq(struct nibTwoCache *ntc, char *seqName)
{
// We read whole chromosome sequences at a time in order to speedup loading of whole gene models.
// This yields very large speedup compared to using nibTwoCacheSeqPartExt to load regions.
// This speedup assumes that gene models have been sorted by chromosome!!

static char *cachedSeqName = NULL;
static struct dnaSeq *cachedSeq = NULL;
if(cachedSeqName == NULL || !sameString(seqName, cachedSeqName))
    {
    freeDnaSeq(&cachedSeq);
    freeMem(cachedSeqName);
    cachedSeq = nibTwoCacheSeq(ntc, seqName);
    cachedSeqName = cloneString(seqName);
    }
return cachedSeq;
}


static void clipGenPred(char *database, struct nibTwoCache *ntc, struct genePred *gp)
{
// Clip exonStarts/exonEnds to cdsStart/cdsEnd and then read in the whole DNA for this gene in preparation for a SNP check.
// After this call, exonStarts/exonEnds contain only the exons used for CDS (i.e. some may be removed).
// DNA is put in name2 field; whole dna is read to make it easier to deal with AA's that cross exon junctions.
// Sequence of negative strand genes is reverse-complemented.
    int i;
    unsigned *newStarts = needMem(gp->exonCount * sizeof(unsigned));
    unsigned *newEnds = needMem(gp->exonCount * sizeof(unsigned));
    int newCount = 0;
    gp->name2 = cloneString("");
    struct dnaSeq *seq = getChromSeq(ntc, gp->chrom);
    for (i=0;i<gp->exonCount;i++)
        {
        if (gp->exonEnds[i] >= gp->cdsStart && gp->exonStarts[i] <= gp->cdsEnd)
            {
            newStarts[newCount] = max(gp->exonStarts[i], gp->cdsStart);
            newEnds[newCount] = min(gp->exonEnds[i], gp->cdsEnd);
            int oldLen = strlen(gp->name2);
            int newLen = newEnds[newCount] - newStarts[newCount];
            char *newName = needMem(oldLen + newLen + 1);
            strcpy(newName, gp->name2);
            memcpy(newName + oldLen, seq->dna + newStarts[newCount], newLen);
            newName[oldLen + newLen] = 0;
            touppers(newName + oldLen);
            freeMem(gp->name2);
            gp->name2 = newName;
            newCount++;
            }
        }
    gp->exonCount = newCount;
    freeMem(gp->exonStarts);
    freeMem(gp->exonEnds);
    gp->exonStarts = newStarts;
    gp->exonEnds = newEnds;
    if (gp->strand[0] == '-')
        {
        reverseComplement(gp->name2, strlen(gp->name2));
        }
    gp->score = strlen(gp->name2);
    verbose(2, "read gene %s %s:%d-%d (%d)\n", gp->name, gp->chrom, gp->txStart, gp->txEnd, gp->score);
    verbose(3, "%s - %d: %s\n", gp->name2, (int) strlen(gp->name2), gp->name2);
}

static int transformPos(char *database, struct nibTwoCache *ntc, struct genePred *gp, unsigned pos, boolean *lastExon)
{
// transformPos chrom:chromStart coordinates to relative CDS coordinates
// returns -1 if pos is NOT within the CDS

int i, delta = 0;
boolean reverse = gp->strand[0] == '-';

if (gp->name2 == NULL)
    {
    clipGenPred(database, ntc, gp);
    }
for (i=0;i<gp->exonCount;i++)
    {
    if (pos < gp->exonStarts[i])
        {
        return -1;
        }
    else if (pos < gp->exonEnds[i])
        {
        pos = delta + pos - gp->exonStarts[i];
        if (gp->strand[0] == '-')
            pos = gp->score - pos - 1;
        // assert(pos >= 0 && pos < strlen(gp->name2));
        *lastExon = reverse ? i == 0 : (i + 1) == gp->exonCount;
        return pos;
        }
    delta += gp->exonEnds[i] - gp->exonStarts[i];
    }
return -1;
}

static struct genePred *readGenes(char *database, struct sqlConnection *conn, struct nibTwoCache *ntc)
{
struct genePred *el, *retVal = NULL;
char query[256];
struct sqlResult *sr;
char **row;
struct hash *geneHash = newHash(16);

// XXXX support just "knownGene" too?

if(sameString(geneModel, "knownCanonical"))
    {
    // get geneSymbols for output purposes
    sqlSafef(query, sizeof(query), "select x.kgID, x.geneSymbol from knownCanonical k, kgXref x where k.transcript = x.kgID");
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        hashAdd(geneHash, row[0], (void *) cloneString(row[1]));
    sqlFreeResult(&sr);

    sqlSafef(query, sizeof(query), "select k.* from knownGene k, knownCanonical c where k.cdsStart != k.cdsEnd and k.name = c.transcript");
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        struct hashEl *el;
        struct genePred *gp = genePredLoad(row);
        if ((el = hashLookup(geneHash, gp->name)))
            {
            freeMem(gp->name);
            gp->name = cloneString((char *) el->val);
            }
        gp->name2 = NULL;
        slAddHead(&retVal, gp);
        }
    sqlFreeResult(&sr);
    }
else if(sqlTableExists(conn, geneModel))
    {
    sqlSafef(query, sizeof(query), "select name, chrom, strand, txStart, txEnd, cdsStart, cdsEnd, exonCount, exonStarts, exonEnds from %s where cdsStart != cdsEnd", geneModel);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        struct genePred *gp = genePredLoad(row);
        gp->name2 = NULL;
        slAddHead(&retVal, gp);
        }
    sqlFreeResult(&sr);
    }
else
    errAbort("Unsupported gene model '%s'", geneModel);
slSort(&retVal, genePredCmp);
if(!lazyLoading)
    {
    for(el = retVal; el != NULL; el = el->next)
        {
        clipGenPred(database, ntc, el);
        }
    }
freeHash(&geneHash);
return(retVal);
}

static DNA parseSnp(char *name, DNA *before, DNA *ref)
{
// If before != NULL and *before != 0, then before is the reference call.
int len = strlen(name);
if(len == 1)
    {
    if(before != NULL)
        *before = 0;
    return name[0];
    }
else if(strlen(name) == 3)
    {
    // N>N (dbSnp format). dbSnp sorts the bases alphabetically (i.e. reference call is not necessarily first, so we can infer
    // before only if the caller supplies a reference call).
    DNA after = name[2];
    if(ref != NULL && *ref)
        {
        if(*ref == name[0])
            *before = name[0];
        else if(*ref == name[2])
            {
            // watch out for sorted SNP calls (where reference is listed second).
            after = name[0];
            *before = name[2];
            }
        else
            // reference matches neither SNP call so just randomly pick the first one.
            *before = name[0];
        }
    else
        ; // caller isn't telling us the reference call, so we cannot set before
    return after;
    }
else if(strlen(name) == 7)
    {
    if(ref)
        *ref = name[0];
    if(name[2] == name[3])
        {
        // homo -> ...
        // We arbitrarily favor the first listed SNP in unusual case of hetero snp
        DNA snp = 0;
        if(before != NULL)
            *before = name[2];
        if(name[5] != name[2])
            snp = name[5];
        else if(name[6] != name[2])
            snp = name[6];
        return snp;
        }
    else if(name[5] == name[6])
        {
        // het => hom - this are perhaps (probably?) LOH events
        // These have a dramatic effect on the heatmaps; in general, they reduce the counts in the null model;
        // I'm not sure why that is...
        DNA snp = name[5];
        if(before != NULL)
            {
            if(snp != name[2])
                *before = name[2];
            else
                *before = name[3];
            }
        return 0;
        return snp;
        }
    // else het => het - I'm just ignoring those
    }
else
    {
    errAbort("Unrecognized SNP format: %s", name);
    }
return 0;
}

static struct dnaMotif *loadMotif(char *database, char *name, boolean *cacheHit)
// cached front-end to dnaMotifLoadWhere
{
static struct hash *motifHash = NULL;
struct hashEl *el;
if(motifHash == NULL)
    motifHash = newHash(0);

if ((el = hashLookup(motifHash, name)))
    {
    if(cacheHit)
        *cacheHit = TRUE;
    return (struct dnaMotif *) el->val;
    }
else
    {
    char where[256];
    struct sqlConnection *conn = sqlConnect(database);
    sqlSafefFrag(where, sizeof(where), "name = '%s'", name);
    struct dnaMotif *motif = dnaMotifLoadWhere(conn, "transRegCodeMotifPseudoCounts", where);
    hashAdd(motifHash, name, (void *) motif);
    sqlDisconnect(&conn);
    if(cacheHit)
        *cacheHit = FALSE;
    return motif;
    }
}

static struct genePred *findNearestGene(struct bed *bed, struct genePred *genes, int maxDistance, int *minDistance)
// return nearest gene (or NULL if none found).
// minDistance is set to 0 if item is within returned gene, otherwise the distance to txStart or txEnd.
// Currently inefficient (O(length genes)).
{
struct genePred *retVal = NULL;
for(; genes != NULL; genes = genes->next)
    {
    if(sameString(bed->chrom, genes->chrom))
        {
        if(bedItemsOverlap(bed, genes))
            {
            retVal = genes;
            *minDistance = 0;
            }
        else
            {
            int diff;
            if(bed->chromStart < genes->txStart)
                diff = genes->txStart - bed->chromStart;
            else
                diff = bed->chromStart - genes->txEnd;
            if(diff < maxDistance && (retVal == NULL || diff < *minDistance))
                {
                retVal = genes;
                *minDistance = diff;
                }
            }
        }
    }
    return retVal;
}

struct hit
{
    struct hit *next;
    float delta;
    float absDelta;
    float before;
    float after;
    int start;
    char strand;
    float maxScore;
};

static void addHit(struct hit **hit, float before, float after, int start, char strand)
{
double delta = after - before;
double absDelta = delta < 0 ? -delta : delta;
double maxScore = max(before, after);
boolean add = FALSE;
if((before > 0 || after > 0) && absDelta >= 1)
    {
    if(*hit == NULL)
        {
        add = TRUE;
        *hit = needMem(sizeof(**hit));
        }
    if(add || absDelta > (*hit)->absDelta || (absDelta == (*hit)->absDelta && maxScore > (*hit)->maxScore))
        {
        (*hit)->delta = delta;
        (*hit)->absDelta = absDelta;
        (*hit)->start = start;
        (*hit)->before = before;
        (*hit)->after = after;
        (*hit)->strand = strand;
        (*hit)->maxScore = maxScore;
        }
    }
}

static void mutationClassifier(char *database, char **files, int fileCount)
{
int i, fileIndex = 0;
struct sqlConnection *conn = sqlConnect(database);
struct sqlConnection *conn2 = sqlConnect(database);
boolean done = FALSE;
boolean clusterTableExists = sqlTableExists(conn, clusterTable);
boolean markovTableExists = markovTable != NULL && sqlTableExists(conn, markovTable);
long time;
struct genePred *genes = NULL;
char pathName[2056];
struct nibTwoCache *ntc;
struct dnaMotif *motifs = NULL;
int maxMotifLen = 0;

safef(pathName, sizeof(pathName), "/gbdb/%s/%s.2bit", database, database);
ntc = nibTwoCacheNew(pathName);
if(!skipGeneModel || bindingSites)
    {
    time = clock1000();
    genes = readGenes(database, conn, ntc);
    verbose(1, "readGenes took: %ld ms\n", clock1000() - time);
    verbose(1, "%d canonical known genes\n", slCount(genes));
    }

if(motifTable)
    {
    // look for de novo mutations.
    struct dnaMotif *motif = NULL;
    struct slName *motifName, *motifNames = NULL;
    char query[256];
    char where[256];
    struct sqlResult *sr;
    char **row;
    sqlSafef(query, sizeof(query), "select name from %s", motifTable);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        slAddHead(&motifNames, slNameNew(row[0]));
    sqlFreeResult(&sr);
    for(motifName = motifNames; motifName != NULL; motifName = motifName->next)
        {
        sqlSafefFrag(where, sizeof(where), "name = '%s'", motifName->name);
        motif = dnaMotifLoadWhere(conn, motifTable, where);
        if(motif == NULL)
            errAbort("couldn't find motif '%s'", motifName->name);
        maxMotifLen = max(maxMotifLen, motif->columnCount);
        slAddHead(&motifs, motif);
        }
    slReverse(&motifs);
    }

while(!done)
    {
    struct bed7 *overlapA = NULL;
    struct bed7 *bed, *unusedA = NULL;
    struct genePredStub *overlapB = NULL;
    struct bed7 *snps = NULL;
    FILE *output;
    static struct hash *used = NULL;

    if(outputExtension == NULL)
        {
        for(i = fileCount - 1; i >= 0; i--)
            readBedFile(&snps, files[i]);
        done = TRUE;
        output = stdout;
        }
    else
        {
        char dir[PATH_LEN], name[PATH_LEN], extension[FILEEXT_LEN];
        char path[PATH_LEN+1];

        splitPath(files[fileIndex], dir, name, extension);
        sprintf(path, "%s%s.%s", dir, name, outputExtension);
        verbose(2, "writing to output file: %s\n", path);
        readBedFile(&snps, files[fileIndex]);
        fileIndex++;
        done = fileIndex == fileCount;
        output = fopen(path, "w");
        if(output == NULL)
            errAbort("Couldn't create output file: %s", path);
        }

    verbose(2, "read %d mutations\n", slCount(snps));

    if(skipGeneModel)
        {
        unusedA = snps;
        snps = NULL;
        overlapA = NULL;
        }
    else
        {
        time = clock1000();
        int count = intersectBeds(snps, genes, &overlapA, &overlapB, &unusedA);
        verbose(2, "intersectBeds took: %ld ms\n", clock1000() - time);
        verbose(2, "number of intersects: %d\n", count);
        }

    // reading unmasked file is much faster - why?
    // sprintf(retNibName, "/hive/data/genomes/hg19/hg19.2bit");
    time = clock1000();
    for (;overlapA != NULL; overlapA = overlapA->next, overlapB = overlapB->next)
        {
        struct genePred *gp = overlapB->genePred;
        char *code = NULL;
        char additional[256];
        additional[0] = 0;
        // boolean reverse = !strcmp(overlapA->strand, "-");
        boolean lastExon;

        int pos = transformPos(database, ntc, gp, overlapA->chromStart, &lastExon);
        if (pos >= 0)
            {
            int len = strlen(overlapA->name);
            if (overlapA->chromEnd > overlapA->chromStart)
                {
                int delta = len - (overlapA->chromEnd - overlapA->chromStart);
                if (delta % 3)
                    code = FRAME_SHIFT_INS;
                else
                    code = IN_FRAME_INS;
                }
            else
                {
                DNA snp = parseSnp(overlapA->name, NULL, NULL);
                if(snp)
                    {
                    unsigned codonStart;
                    unsigned aaIndex = (pos / 3) + 1;
                    if ((pos % 3) == 0)
                        codonStart = pos;
                    else if ((pos % 3) == 1)
                        codonStart = pos - 1;
                    else
                        codonStart = pos - 2;
                    char original[4];
                    char new[4];
                    strncpy(original, gp->name2 + codonStart, 3);
                    strncpy(new, gp->name2 + codonStart, 3);
                    original[3] = new[3] = 0;
                    new[pos % 3] = snp;
                    if (gp->strand[0] == '-')
                        complement(new + (pos % 3), 1);
                    AA originalAA = lookupCodon(original);
                    AA newAA = lookupCodon(new);
                    if (!originalAA)
                        originalAA = '*';
                    if (newAA)
                        {
                        code = originalAA == newAA ? SYNONYMOUS : originalAA == '*' ? READ_THROUGH : MISSENSE;
                        }
                    else
                        {
                        newAA = '*';
                        code = lastExon ? NONSENSE_LAST_EXON : NONSENSE;
                        }
                    if (debug)
                        fprintf(stderr, "original: %s:%c; new: %s:%c\n", original, originalAA, new, newAA);

#ifdef TCGA_CODES
                    safef(additional, sizeof(additional), "g.%s:%d%c>%c\tc.%d%c>%c\tc.(%d-%d)%s>%s\tp.%c%d%c", 
                          gp->chrom + 3, overlapA->chromStart + 1, original[pos % 3], new[pos % 3], 
                          pos + 1, original[pos % 3], new[pos % 3],
                          codonStart + 1, codonStart + 3, original, new,
                          originalAA, aaIndex, newAA);
#else
                    safef(additional, sizeof(additional), "%c%d%c", originalAA, aaIndex, newAA);
#endif
                    if (debug)
                        fprintf(stderr, "mismatch at %s:%d; %d; %c => %c\n", overlapA->chrom, overlapA->chromStart, pos, originalAA, newAA);
                    }
                else
                    code = SYNONYMOUS;
		}
            }
        else
            {
            boolean reverse = gp->strand[0] == '-';
            if (overlapA->chromStart < gp->cdsStart)
                {
                code = reverse ? THREE_PRIME_UTR : FIVE_PRIME_UTR;
                }
            else if (overlapA->chromStart >= gp->cdsEnd)
                {
                code = reverse ? FIVE_PRIME_UTR : THREE_PRIME_UTR;
                }
            else
                {
                // In intro, so check for interuption of splice junction (special case first and last exon).
                int i;
                code = NONCODING;
                for (i=0;i<gp->exonCount;i++)
                    {
                    int start = gp->exonStarts[i] - overlapA->chromStart;
                    int end   = overlapA->chromEnd - gp->exonEnds[i];
                    // XXXX I still think this isn't quite right (not sure if we s/d use chromEnd or chromStart).
                    if (i == 0)
                        {
                        if (end == 1 || end == 2)
                            code = SPLICE_SITE;
                        }
                    else if (i == (gp->exonCount - 1))
                        {
                        if (start == 1 || start == 2)
                            code = SPLICE_SITE;
                        }
                    else if ((start == 1 || start == 2) || (end == 1 || end == 2))
                        code = SPLICE_SITE;
		    else
			code = INTRON;		     
                    }
                }
            }

        if (code)
            {
            if(sameString(code, NONCODING))
                {
                // we want to look for regulatory mutations in this item before classifying it simply as non-coding
                struct bed7 *tmp = shallowBedCopy(overlapA);
                tmp->code = gp->name;
                slAddHead(&unusedA, tmp);
                }
            else
                printLine(output, overlapA->chrom, overlapA->chromStart, overlapA->chromEnd, gp->name, code, additional, overlapA->key);
            }
        }

    verbose(1, "gene model took: %ld ms\n", clock1000() - time);
    
    if(motifs)
        {
        for(bed = unusedA; bed != NULL; bed = bed->next)
            {
            DNA ref, beforeSnp, snp;
            int start = max(0, bed->chromStart - maxMotifLen + 1);
            int end = bed->chromStart + maxMotifLen;
            struct dnaSeq *beforeSeq = nibTwoCacheSeqPartExt(ntc, bed->chrom, start, end - start, TRUE, NULL);
            // XXXX watch out for end of chrom
            touppers(beforeSeq->dna);
            ref = beforeSeq->dna[bed->chromStart - start];
            verbose(3, "ref: %c; snp: %s\n", ref, bed->name);
            snp = parseSnp(bed->name, &beforeSnp, &ref);
            verbose(3, "beforeSnp: %c; snp: %c\n", beforeSnp, snp);
            if(beforeSnp && snp && beforeSnp != snp)
                {
                struct dnaMotif *motif;
                struct dnaSeq *beforeSeq, *afterSeq;
                double mark0[5];
                mark0[0] = 1;
                mark0[1] = mark0[2] = mark0[3] = mark0[4] = 0.25;
                // XXXX stick in beforeSnp
                beforeSeq = nibTwoCacheSeqPartExt(ntc, bed->chrom, start, end - start, TRUE, NULL);
                touppers(beforeSeq->dna);
                afterSeq = nibTwoCacheSeqPartExt(ntc, bed->chrom, start, end - start, TRUE, NULL);
                touppers(afterSeq->dna);
                afterSeq->dna[bed->chromStart - start] = snp;
                for (motif = motifs; motif != NULL; motif = motif->next)
                    {
                    int looper;
                    struct hit *hit = NULL;
                    for (looper = 0; looper < 2; looper++)
                        {
                        int i = max(start, bed->chromStart - motif->columnCount + 1);
                        char strand = looper == 0 ? '+' : '-';
                        for (; i <= bed->chromStart; i++)
                            {
                            char buf[128];
                            safencpy(buf, sizeof(buf), beforeSeq->dna + (i - start), motif->columnCount);
                            if(looper == 1)
                                reverseComplement(buf, motif->columnCount);
                            double before = dnaMotifBitScoreWithMark0Bg(motif, buf, mark0);
                            safencpy(buf, sizeof(buf), afterSeq->dna + (i - start), motif->columnCount);
                            if(looper == 1)
                                reverseComplement(buf, motif->columnCount);
                            double after = dnaMotifBitScoreWithMark0Bg(motif, buf, mark0);
                            verbose(4, "%s: %s: %f => %f\n", motif->name, buf, before, after);
                            addHit(&hit, before, after, i, strand);
                            }
                        }
                    if(hit)
                        {
                        fprintf(output, "%s\t%d\t%d\t%s\t%.2f\t%.2f\t%.2f\t%c\n", 
                                bed->chrom, hit->start, hit->start + motif->columnCount, motif->name, hit->absDelta, hit->before, hit->after, hit->strand);
                        }
                    }
                }
            }
        }
    else if(clusterTableExists)
        {
        char **row;
        char query[256];
        struct sqlResult *sr;
        struct bed6FloatScore *site, *sites = NULL;

        used = newHash(0);
        time = clock1000();
        struct genomeRangeTree *tree = genomeRangeTreeNew();
        for(bed = unusedA; bed != NULL; bed = bed->next)
            genomeRangeTreeAddVal(tree, bed->chrom, bed->chromStart, bed->chromEnd, (void *) bed, NULL);
        verbose(2, "cluster genomeRangeTreeAddVal took: %ld ms\n", clock1000() - time);

        sqlSafef(query, sizeof(query), "select chrom, chromStart, chromEnd, name, score, strand from %s", clusterTable);
        sr = sqlGetResult(conn, query);
        while ((row = sqlNextRow(sr)) != NULL)
            {
            struct bed6FloatScore *site = bed6FloatScoreLoad(row);
            slAddHead(&sites, site);
            }
        sqlFreeResult(&sr);
        slSort(&sites, myBedCmp);
        for (site = sites; site != NULL; site = site->next)
            {
            struct range *range = genomeRangeTreeAllOverlapping(tree, site->chrom, site->chromStart, site->chromEnd);
            // fprintf(stderr, "%s\t%d\t%d\t%s\n", site->chrom, site->chromStart, site->chromEnd, site->name);
            if(bindingSites)
                {
                if(markovTable != NULL)
                    errAbort("markovTables not supported for -bindingSites");
                if(range)
                    {
                    struct dnaSeq *beforeSeq, *afterSeq = NULL;
                    float delta, before, after, maxDelta = 0;
                    int count = 0;

                    struct dnaMotif *motif = loadMotif(database, site->name, NULL);
                    beforeSeq = nibTwoCacheSeqPartExt(ntc, site->chrom, site->chromStart, site->chromEnd - site->chromStart, TRUE, NULL);
                    touppers(beforeSeq->dna);
                    afterSeq = nibTwoCacheSeqPartExt(ntc, site->chrom, site->chromStart, site->chromEnd - site->chromStart, TRUE, NULL);
                    touppers(afterSeq->dna);
                    if(*site->strand == '-')
                        {
                        reverseComplement(beforeSeq->dna, beforeSeq->size);
                        reverseComplement(afterSeq->dna, afterSeq->size);
                        }
                    for (; range != NULL; range = range->next)
                        {
                        DNA snp, beforeSnp = 0;
                        int pos;
                        struct bed7 *bed = (struct bed7 *) range->val;

                        snp = parseSnp(bed->name, &beforeSnp, NULL);
                        if(beforeSnp && snp && beforeSnp != snp)
                            {
                            count++;
                            if(*site->strand == '-')
                                {
                                reverseComplement(&snp, 1);
                                reverseComplement(&beforeSnp, 1);
                                pos = site->chromEnd - (bed->chromStart + 1);
                                }
                            else
                                pos = bed->chromStart - site->chromStart;
                            beforeSeq->dna[pos] = beforeSnp;
                            afterSeq->dna[pos] = snp;
                            }
                        }
                    before = dnaMotifBitScore(motif, beforeSeq->dna);
                    after = dnaMotifBitScore(motif, afterSeq->dna);
                    delta = after - before;
                    freeDnaSeq(&beforeSeq);
                    freeDnaSeq(&afterSeq);
                    if((!minDelta && count) || (minDelta > 0 && delta > minDelta && delta > maxDelta) || (minDelta < 0 && delta < minDelta && delta < maxDelta))
                        {
                        int minDistance = 0;
                        char buf[256];
                        struct genePred *nearestGene = findNearestGene((struct bed *) site, genes, maxNearestGene, &minDistance);
                        if(nearestGene == NULL)
                            sprintf(buf, "\t");
                        else
                            safef(buf, sizeof(buf), "%s\t%d", nearestGene->name, minDistance);
                        fprintf(output, "%s\t%d\t%d\t%s\t%d\t%.2f\t%.2f\t%s\n", 
                                site->chrom, site->chromStart, site->chromEnd, site->name, count, before, after, buf);
                        }
                    }
                }
            else
                {
                for (; range != NULL; range = range->next)
                    {
                    DNA snp = 0;
                    DNA refCall = 0;
                    char line[256];
                    char *code = NULL;
                    safef(line, sizeof(line), ".");
                    struct bed7 *bed = (struct bed7 *) range->val;
                    snp = parseSnp(bed->name, NULL, &refCall);
            
                    // XXXX && clusterTableExists)
                    if(snp)
                        {
                        struct dnaSeq *seq = NULL;
                        char beforeDna[256], afterDna[256];
                        int pos;
                        float delta, before, after, maxDelta = 0;
                        boolean cacheHit;
                        struct dnaMotif *motif = loadMotif(database, site->name, &cacheHit);
                        double mark2[5][5][5];

                        // XXXX cache markov models? (we should be processing them in manner where we can use the same one repeatedly).
                        if(markovTableExists && loadMark2(conn2, markovTable, bed->chrom, bed->chromStart, bed->chromStart + 1, mark2))
                            {
                            if(!cacheHit)
                                dnaMotifMakeLog2(motif);
                            dnaMarkMakeLog2(mark2);

                            seq = nibTwoCacheSeqPartExt(ntc, bed->chrom, site->chromStart - 2, site->chromEnd - site->chromStart + 4, TRUE, NULL);
                            touppers(seq->dna);
                            if(refCall && seq->dna[bed->chromStart - site->chromStart + 2] != refCall)
                                errAbort("Actual reference (%c) doesn't match what is reported in input file (%c): %s:%d-%d", 
                                         refCall, seq->dna[bed->chromStart - site->chromStart + 2], bed->chrom, site->chromStart - 2, site->chromEnd + 2);
                            if(*site->strand == '-')
                                {
                                reverseComplement(seq->dna, seq->size);
                                reverseComplement(&snp, 1);
                                // XXXX is the following correct?
                                pos = site->chromEnd - (bed->chromStart + 1);
                                }
                            else
                                pos = bed->chromStart - site->chromStart;
                            before = dnaMotifBitScoreWithMarkovBg(motif, seq->dna, mark2);
                            seq->dna[pos + 2] = snp;
                            after = dnaMotifBitScoreWithMarkovBg(motif, seq->dna, mark2);
                            delta = after - before;
                            verbose(2, "markov before/after: %.2f>%.2f (%.2f)\n", before, after, site->score);
                            }
                        else
                            {
                            seq = nibTwoCacheSeqPartExt(ntc, bed->chrom, site->chromStart, site->chromEnd - site->chromStart, TRUE, NULL);
                            touppers(seq->dna);
                            if(refCall && seq->dna[bed->chromStart - site->chromStart] != refCall)
                                errAbort("Actual reference (%c) doesn't match what is reported in input file (%c): %s:%d-%d", 
                                         refCall, seq->dna[bed->chromStart - site->chromStart], bed->chrom, site->chromStart, site->chromEnd);
                            if(*site->strand == '-')
                                {
                                reverseComplement(seq->dna, seq->size);
                                reverseComplement(&snp, 1);
                                // XXXX is the following correct?
                                pos = site->chromEnd - (bed->chromStart + 1);
                                }
                            else
                                pos = bed->chromStart - site->chromStart;

                            before = dnaMotifBitScore(motif, seq->dna);
                            strncpy(beforeDna, seq->dna, seq->size);
                            beforeDna[seq->size] = 0;
                            seq->dna[pos] = snp;
                            after = dnaMotifBitScore(motif, seq->dna);
                            strncpy(afterDna, seq->dna, seq->size);
                            afterDna[seq->size] = 0;
                            delta = after - before;
                            }
                        if(!minDelta || (minDelta > 0 && delta > minDelta && delta > maxDelta) || (minDelta < 0 && delta < minDelta && delta < maxDelta))
                            {
                            int minDistance = 0;
                            maxDelta = delta;
                            if(debug)
                                fprintf(stderr, "%s:%d-%d: %s\t%c-%c: %d == %d ?; %.2f -> %.2f\n\t%s\t%s\n",
                                        bed->chrom, bed->chromStart, bed->chromEnd,
                                        bed->name, seq->dna[pos], snp,
                                        seq->size, motif->columnCount, before, after, beforeDna, afterDna);
                            struct genePred *nearestGene = findNearestGene((struct bed *) bed, genes, maxNearestGene, &minDistance);
                            if(nearestGene == NULL)
                                safef(line, sizeof(line), "%s;%.2f>%.2f;%s%d", site->name, before, after, site->strand, pos + 1);
                            else
                                safef(line, sizeof(line), "%s;%.2f>%.2f;%s%d;%s;%d", site->name, before, after, site->strand, pos + 1, nearestGene->name, minDistance);
                            code = REGULATORY;
                            // XXXX record (somehow) that we have used this record?
                            }
                        freeDnaSeq(&seq);
                        }
                    if(code != NULL)
                        {
                        char key[256];
                        printLine(output, bed->chrom, bed->chromStart, bed->chromEnd, ".", code, line, bed->key);
                        safef(key, sizeof(key), "%s:%d:%d", bed->chrom, bed->chromStart, bed->chromEnd);
                        if (hashLookup(used, key) == NULL)
                            hashAddInt(used, key, 1);
                        }
                    }
                }
            }
        bed6FloatScoreFreeList(&sites);
        }

    if(!bindingSites)
        {
        for(bed = unusedA; bed != NULL; bed = bed->next)
            {
            char key[256];
            safef(key, sizeof(key), "%s:%d:%d", bed->chrom, bed->chromStart, bed->chromEnd);
            if (hashLookup(used, key) == NULL)
                {
                if(bed->code == NULL)
                    printLine(output, bed->chrom, bed->chromStart, bed->chromEnd, ".", INTERGENIC, ".", bed->key);
                else
                    printLine(output, bed->chrom, bed->chromStart, bed->chromEnd, bed->code, NONCODING, ".", bed->key);
                }
            }
        }
    
    verbose(2, "regulation model took: %ld ms\n", clock1000() - time);
    if(outputExtension != NULL)
        fclose(output);
    slFreeList(&overlapA);
    slFreeList(&overlapB);
    slFreeList(&snps);
    slFreeList(&unusedA);
    freeHash(&used);
    }

sqlDisconnect(&conn);
sqlDisconnect(&conn2);
nibTwoCacheFree(&ntc);
}

int main(int argc, char** argv)
{
optionInit(&argc, argv, optionSpecs);
bindingSites = optionExists("bindingSites");
clusterTable = optionVal("clusterTable", clusterTable);
geneModel = optionVal("geneModel", "knownCanonical");
lazyLoading = optionExists("lazyLoading");
markovTable = optionVal("markovTable", NULL);
maxNearestGene = optionInt("maxNearestGene", maxNearestGene);
motifTable = optionVal("motifTable", NULL);
minDelta = optionFloat("minDelta", minDelta);
outputExtension = optionVal("outputExtension", NULL);
skipGeneModel = optionExists("skipGeneModel");
oneBased = optionExists("oneBased");
if(bindingSites)
    lazyLoading = TRUE;
if (argc < 3)
    usage();
mutationClassifier(argv[1], argv + 2, argc - 2);
return 0;
}
