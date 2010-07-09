/* Classify mutations as missense etc. */

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

int debug = 0;
char *outputExtension = NULL;
boolean lazyLoading = FALSE;           // avoid loading DNA for all known genes (performance hack if you are classifying only a few items).
float minDelta = -1;
char *clusterTable = "wgEncodeRegTfbsClusteredMotifs";
char *markovTable = "markovModels";
static int maxNearestGene = 10000;
boolean skipGeneModel = FALSE;
boolean oneBased = FALSE;

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
    {"clusterTable", OPTION_STRING},
    {"lazyLoading", OPTION_BOOLEAN},
    {"minDelta", OPTION_FLOAT},
    {"maxNearestGene", OPTION_INT},
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
    int score; /* Score - 0-1000 */
    char strand;
    char *key; // user provided stuff
    char *code; // used internally
    };

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
  "   -lazyLoading\t\tAvoid loading complete gene model (performance hack for when you are classifying only a few items).\n"
  "   -maxNearestGene\tMaximum distance used when computing nearest gene (default: %d).\n"
  "   -minDelta\t\tLook for regulatory sites with score(after) - score(before) < minDelta (default: %.1f).\n"
  "\t\t\tLogic is reversed if positive (i.e. look for sites with score(after) - score(before) > minDelta.)\n"
  "   -outputExtension\tCreate an output file with this extension for each input file (instead of writing to stdout).\n"
  "   -verbose=N\t\tverbose level for extra information to STDERR\n\n"
  "   -oneBased     = Use one-based coordinates instead of zero-based.\n"
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

static int myGenePredCmp(const void *va, const void *vb)
/* slSort callback to sort based on chrom,chromStart. */
{
     const struct genePred *a = *((struct genePred **)va);
     const struct genePred *b = *((struct genePred **)vb);
     int diff = strcmp(a->chrom, b->chrom);
     if (!diff)
	  diff = a->cdsStart - b->cdsStart;
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
//
// if bCommon is NULL we don't return intersected regions in b, AND we only include one copy
// from a (even if it overlaps multiple copies from b).
//
// Running time is O(nlgn) (where n = max(slCount(a), slCount(b)))
// (though b/c of the multiple matching "feature" there's a degenerate case that's O(n^2) 
// if all the items in a overlap all the items in b).

int count = 0;
slSort(&a, myBedCmp);
slSort(&b, myGenePredCmp);
// allocA and allocB point to the last allocated bed struct's
struct bed7 *curA;
struct bed7 *lastAddedA = NULL;
struct genePred *curB = b;
struct genePred *savedB = NULL;
struct genePredStub *lastAddedB = NULL;
boolean curAUsed = FALSE;

for (curA = a; curA != NULL && curB != NULL;)
    {
    if (debug)
        {
        fprintf(stderr, "A: %s:%d-%d\n", curA->chrom, curA->chromStart, curA->chromEnd);
        fprintf(stderr, "B: %s:%d-%d\n", curB->chrom, curB->cdsStart, curB->cdsEnd);
        }
    if (bedItemsOverlap((struct bed *) curA, curB))
        {
        curAUsed = TRUE;
        if (debug)
            fprintf(stderr, "%s:%d-%d", curA->chrom, curA->chromStart, curA->chromEnd);
        if (aCommon != NULL)
            {
            struct bed7 *tmpA = shallowBedCopy(curA);
            if (*aCommon == NULL)
                {
                *aCommon = tmpA;
                }
            else
                {
                lastAddedA->next = tmpA;
                }
            // We put newly allocated bed items at the end of the returned list so they match order in original list
            lastAddedA = tmpA;
            }
        if (bCommon != NULL)
            {
            struct genePredStub *tmpB = genePredStubCopy(curB);
            if (*bCommon == NULL)
                {
                *bCommon = tmpB;
                }
            else
                {
                lastAddedB->next = tmpB;
                }
            lastAddedB = tmpB;
            }
        if (bCommon == NULL)
            {
            // see note at beginning of function
            curA = curA->next;
            curAUsed = FALSE;
            }
        else
            {
            if (savedB == NULL)
                savedB = curB;
            curB = curB->next;
            }
        count++;
        }
    else
        {
        if (savedB)
            {
            // curA has matched at least one entry in b; now rewind curB to look for potentially multiple matches 
            // within b in the next entry from a (see notes in hw1_analyze.h)
            if(!curAUsed && aUnmatched != NULL)
                slAddHead(aUnmatched, shallowBedCopy(curA));
            curA = curA->next;
            curB = savedB;
            savedB = NULL;
            curAUsed = FALSE;
            }
        else
            {
            int diff = strcmp(curA->chrom, curB->chrom);
            if (!diff)
                diff = curA->chromStart - curB->cdsStart;
            if (diff < 0)
                {
                if(!curAUsed && aUnmatched != NULL)
                    slAddHead(aUnmatched, shallowBedCopy(curA));
                curA = curA->next;
                curAUsed = FALSE;
                }
            else
                {
                curB = curB->next;
                }
            }
        }
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
    if (wordCount >= 5)
        bed->score = lineFileNeedNum(lf, row, 4);
    if (wordCount >= 6)
        {
        bed->strand = row[5][0];
        if (bed->strand == '-')
            // we support this so we can process dbSnp data (which has reverse strand SNPs).
            complement(bed->name, strlen(bed->name)); 
        if (wordCount >= 7)
            bed->key = cloneString(row[6]);
        }
    slAddHead(list, bed);
    }
lineFileClose(&lf);
}

static void printLine(FILE *stream, char *chrom, int chromStart, int chromEnd, char *name, char *code, char *additional, char *key)
{
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

static void clipGenPred(char *database, struct genePred *gp)
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
    for (i=0;i<gp->exonCount;i++)
        {
        if (gp->exonEnds[i] >= gp->cdsStart && gp->exonStarts[i] <= gp->cdsEnd)
            {
            char retNibName[HDB_MAX_PATH_STRING];
            newStarts[newCount] = max(gp->exonStarts[i], gp->cdsStart);
            newEnds[newCount] = min(gp->exonEnds[i], gp->cdsEnd);
            hNibForChrom(database, gp->chrom, retNibName);
            struct dnaSeq *dna = hFetchSeqMixed(retNibName, gp->chrom, newStarts[newCount], newEnds[newCount]);
            char *newName = needMem(strlen(gp->name2) + strlen(dna->dna) + 1);
            sprintf(newName, "%s%s", gp->name2, dna->dna);
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
    if (debug)
        printf("%s - %d: %s\n", gp->name2, (int) strlen(gp->name2), gp->name2);
}

static int transformPos(char *database, struct genePred *gp, unsigned pos, boolean *lastExon)
{
// transformPos chrom:chromStart coordinates to relative CDS coordinates
// returns -1 if pos is NOT within the CDS

int i, delta = 0;
boolean reverse = gp->strand[0] == '-';

if (gp->name2 == NULL)
    {
    clipGenPred(database, gp);
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

static struct genePred *readGenes(char *database, struct sqlConnection *conn)
{
struct genePred *el, *retVal = NULL;
char query[256];
struct sqlResult *sr;
char **row;
struct hash *geneHash = newHash(16);

// get geneSymbols for output purposes
safef(query, sizeof(query), "select x.kgID, x.geneSymbol from knownCanonical k, kgXref x where k.transcript = x.kgID");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(geneHash, row[0], (void *) cloneString(row[1]));
sqlFreeResult(&sr);

safef(query, sizeof(query), "select k.* from knownGene k, knownCanonical c where k.cdsStart != k.cdsEnd and k.name = c.transcript");
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

slSort(&retVal, genePredCmp);
if(!lazyLoading)
    {
    for(el = retVal; el != NULL; el = el->next)
        {
        clipGenPred(database, el);
        }
    }
freeHash(&geneHash);
return(retVal);
}

static DNA parseSnp(char *name, DNA *refCall)
{
int len = strlen(name);
if(len == 1)
    {
    return name[0];
    }
else if(strlen(name) == 3)
    {
    // N>N
    return name[2];
    }
else if(strlen(name) == 7)
    {
    // we arbitrarily favor the first listed SNP in unusual case of hetero snp
    if(refCall)
        *refCall = name[0];
    if(name[5] != name[0])
        return name[5];
    else if(name[6] != name[0])
        return name[6];
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
    safef(where, sizeof(where), "name = '%s'", name);
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

static void mutationClassifier(char *database, char **files, int fileCount)
{
int i, fileIndex = 0;
struct sqlConnection *conn = sqlConnect(database);
struct sqlConnection *conn2 = sqlConnect(database);
boolean done = FALSE;
boolean motifTableExists = sqlTableExists(conn, clusterTable);
boolean markovTableExists = sqlTableExists(conn, markovTable);
long time;
struct genePred *genes = NULL;

if(!skipGeneModel)
    {
    time = clock1000();
    genes = readGenes(database, conn);
    verbose(2, "readGenes took: %ld ms\n", clock1000() - time);
    verbose(2, "%d canonical known genes\n", slCount(genes));
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

        int pos = transformPos(database, gp, overlapA->chromStart, &lastExon);
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
                DNA snp = parseSnp(overlapA->name, NULL);
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

    verbose(2, "gene model took: %ld ms\n", clock1000() - time);
    
    used = newHash(0);
    time = clock1000();
    struct genomeRangeTree *tree = genomeRangeTreeNew();
    for(bed = unusedA; bed != NULL; bed = bed->next)
        genomeRangeTreeAddVal(tree, bed->chrom, bed->chromStart, bed->chromEnd, (void *) bed, NULL);
    verbose(2, "cluster genomeRangeTreeAddVal took: %ld ms\n", clock1000() - time);

    if(motifTableExists)
        {
        char **row;
        char query[256];
        struct sqlResult *sr;
        safef(query, sizeof(query), "select chrom, chromStart, chromEnd, name, score, strand from %s order by chrom, chromStart", clusterTable);
        sr = sqlGetResult(conn, query);
        while ((row = sqlNextRow(sr)) != NULL)
            {
            char motifStrand[2];
            int motifStart = sqlUnsigned(row[1]);
            int motifEnd = sqlUnsigned(row[2]);
            float motifScore = sqlFloat(row[4]);
            char *motifName = cloneString(row[3]);
            safef(motifStrand, sizeof(motifStrand), row[5]);
            struct range *range = genomeRangeTreeAllOverlapping(tree, row[0], motifStart, motifEnd);
            for (; range != NULL; range = range->next)
                {
                DNA snp = 0;
                DNA refCall = 0;
                char line[256];
                char *code = NULL;
                safef(line, sizeof(line), ".");
                struct bed7 *bed = (struct bed7 *) range->val;
                snp = parseSnp(bed->name, &refCall);
            
                // XXXX && motifTableExists)
                if(snp)
                    {
                    struct dnaSeq *seq = NULL;
                    char beforeDna[256], afterDna[256];
                    int pos;
                    float delta, before, after, maxDelta = 0;
                    boolean cacheHit;
                    double mark2[5][5][5];
                    struct dnaMotif *motif = loadMotif(database, motifName, &cacheHit);
                    if(!cacheHit)
                        dnaMotifMakeLog2(motif);

                    // XXXX cache markov models? (we should be processing them in manner where we can use the same one repeatedly).
                    if(markovTableExists && loadMark2(conn2, markovTable, bed->chrom, bed->chromStart, bed->chromStart + 1, mark2))
                        {
                        dnaMarkMakeLog2(mark2);

                        seq = hDnaFromSeq(database, bed->chrom, motifStart - 2, motifEnd + 2, dnaUpper);
                        if(refCall && seq->dna[bed->chromStart - motifStart + 2] != refCall)
                            errAbort("Actual reference doesn't match what is reported in input file");
                        if(*motifStrand == '-')
                            {
                            reverseComplement(seq->dna, seq->size);
                            reverseComplement(&snp, 1);
                            // XXXX is the following correct?
                            pos = motifEnd - (bed->chromStart + 1);
                            }
                        else
                            pos = bed->chromStart - motifStart;
                        before = dnaMotifBitScoreWithMarkovBg(motif, seq->dna, mark2);
                        seq->dna[pos + 2] = snp;
                        after = dnaMotifBitScoreWithMarkovBg(motif, seq->dna, mark2);
                        delta = after - before;
                        verbose(2, "markov before/after: %.2f>%.2f (%.2f)\n", before, after, motifScore);
                        }
                    else
                        {
                        seq = hDnaFromSeq(database, bed->chrom, motifStart - 2, motifEnd + 2, dnaUpper);
                        if(refCall && seq->dna[bed->chromStart - motifStart] != refCall)
                            errAbort("Actual reference doesn't match what is reported in input file");
                        if(*motifStrand == '-')
                            {
                            reverseComplement(seq->dna, seq->size);
                            reverseComplement(&snp, 1);
                            // XXXX is the following correct?
                            pos = motifEnd - (bed->chromStart + 1);
                            }
                        else
                            pos = bed->chromStart - motifStart;

                        before = dnaMotifBitScore(motif, seq->dna);
                        strncpy(beforeDna, seq->dna, seq->size);
                        beforeDna[seq->size] = 0;
                        seq->dna[pos] = snp;
                        after = dnaMotifBitScore(motif, seq->dna);
                        strncpy(afterDna, seq->dna, seq->size);
                        afterDna[seq->size] = 0;
                        delta = after - before;
                        }
                    if((minDelta > 0 && delta > minDelta && delta > maxDelta) || (minDelta < 0 && delta < minDelta && delta < maxDelta))
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
                            safef(line, sizeof(line), "%s;%.2f>%.2f;%s%d", motifName, before, after, motifStrand, pos + 1);
                        else
                            safef(line, sizeof(line), "%s;%.2f>%.2f;%s%d;%s;%d", motifName, before, after, motifStrand, pos + 1, nearestGene->name, minDistance);
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
    
    for(bed = unusedA; bed != NULL; bed = bed->next)
        {
        char key[256];
        safef(key, sizeof(key), "%s:%d:%d", bed->chrom, bed->chromStart, bed->chromEnd);
        genomeRangeTreeAddVal(tree, bed->chrom, bed->chromStart, bed->chromEnd, (void *) bed, NULL);
        if (hashLookup(used, key) == NULL)
            {
            if(bed->code == NULL)
                printLine(output, bed->chrom, bed->chromStart, bed->chromEnd, ".", INTERGENIC, ".", bed->key);
            else
                printLine(output, bed->chrom, bed->chromStart, bed->chromEnd, bed->code, NONCODING, ".", bed->key);
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
}

int main(int argc, char** argv)
{
optionInit(&argc, argv, optionSpecs);
clusterTable = optionVal("clusterTable", clusterTable);
lazyLoading = optionExists("lazyLoading");
maxNearestGene = optionInt("maxNearestGene", maxNearestGene);
minDelta = optionFloat("minDelta", minDelta);
outputExtension = optionVal("outputExtension", NULL);
skipGeneModel = optionExists("skipGeneModel");
oneBased = optionExists("oneBased");

if (argc < 3)
    usage();
mutationClassifier(argv[1], argv + 2, argc - 2);
return 0;
}
